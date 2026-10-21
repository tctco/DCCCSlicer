#include "NiftiIO.h"

#include "PathUtils.h"
#include <chrono>
#include <filesystem>
#include <itkImageFileWriter.h>
#include <nifti1_io.h>
#include <cmath>
#include <memory>
#include <vector>

namespace Common::nifti {

namespace {

bool containsNonAscii(const std::string& value) {
    for (const unsigned char ch : value) {
        if (ch > 0x7F) {
            return true;
        }
    }
    return false;
}

std::string niftiExtension(const std::filesystem::path& path) {
    if (path.extension() == ".gz" && path.stem().extension() == ".nii") {
        return ".nii.gz";
    }
    return Common::path::toUtf8(path.extension());
}

std::filesystem::path makeAsciiTempPath(const std::filesystem::path& originalPath) {
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "dcccslicer_nifti_io";
    std::filesystem::create_directories(tempDir);

    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string extension = niftiExtension(originalPath);
    return tempDir / ("dccc_" + std::to_string(now) + extension);
}

std::string readablePathForItk(const std::filesystem::path& originalPath,
                               std::filesystem::path& stagingPath,
                               bool& usesTempCopy) {
    stagingPath.clear();
    usesTempCopy = false;

#ifdef _WIN32
    const std::string originalUtf8 = Common::path::toUtf8(originalPath);
    if (containsNonAscii(originalUtf8)) {
        stagingPath = makeAsciiTempPath(originalPath);
        std::filesystem::copy_file(
            originalPath, stagingPath, std::filesystem::copy_options::overwrite_existing);
        usesTempCopy = true;
        return Common::path::legacyFileName(stagingPath);
    }
#endif
    return Common::path::legacyFileName(originalPath);
}

std::string writablePathForItk(const std::filesystem::path& originalPath,
                               std::filesystem::path& stagingPath,
                               bool& requiresCopyBack) {
    requiresCopyBack = false;
    stagingPath.clear();

#ifdef _WIN32
    const std::string originalUtf8 = Common::path::toUtf8(originalPath);
    if (containsNonAscii(originalUtf8)) {
        stagingPath = makeAsciiTempPath(originalPath);
        requiresCopyBack = true;
        return Common::path::legacyFileName(stagingPath);
    }
#endif
    return Common::path::legacyFileName(originalPath);
}

void cleanupTempFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

template <typename T>
void checkFinitePayload(znzFile stream, const nifti_image& header) {
    std::vector<T> chunk(65536);
    size_t remaining = header.nvox;
    while (remaining > 0) {
        const size_t count = std::min(remaining, chunk.size());
        if (znzread(chunk.data(), sizeof(T), count, stream) != count)
            throw std::runtime_error("Truncated NIfTI voxel data");
        if (header.byteorder != nifti_short_order())
            nifti_swap_Nbytes(static_cast<int>(count), sizeof(T), chunk.data());
        for (size_t i = 0; i < count; ++i)
            if (!std::isfinite(chunk[i]))
                throw std::invalid_argument("PET must contain finite intensities");
        remaining -= count;
    }
}

void checkFiniteNifti(const std::string& path) {
    // nifti_read_buffer silently replaces NaN/Inf with zero. Inspect the raw
    // floating-point payload before ITK reads it, using the same NIfTI library
    // for header parsing, compression and endianness (not a second parser).
    std::unique_ptr<nifti_image, decltype(&nifti_image_free)> header(
        nifti_image_read(path.c_str(), 0), &nifti_image_free);
    if (!header) throw std::runtime_error("Cannot read NIfTI header: " + path);
    if (header->datatype != NIFTI_TYPE_FLOAT32 && header->datatype != NIFTI_TYPE_FLOAT64)
        return;
    struct Stream {
        znzFile file;
        ~Stream() { if (!znz_isnull(file)) znzclose(file); }
    } stream{znzopen(header->iname, "rb", nifti_is_gzfile(header->iname))};
    if (znz_isnull(stream.file) || header->iname_offset < 0 ||
        znzseek(stream.file, header->iname_offset, SEEK_SET) < 0)
        throw std::runtime_error("Cannot read NIfTI voxel data: " + path);
    if (header->datatype == NIFTI_TYPE_FLOAT32) checkFinitePayload<float>(stream.file, *header);
    else checkFinitePayload<double>(stream.file, *header);
}

} // namespace

void saveImage(ImageType::Pointer image, const std::string& filename) {
    using WriterType = itk::ImageFileWriter<ImageType>;
    const std::filesystem::path outputPath = Common::path::fromUtf8(filename);
    std::filesystem::path stagingPath;
    bool requiresCopyBack = false;

    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName(writablePathForItk(outputPath, stagingPath, requiresCopyBack));
    writer->SetInput(image);
    try {
        writer->Update();

        if (requiresCopyBack) {
            std::filesystem::copy_file(
                stagingPath, outputPath, std::filesystem::copy_options::overwrite_existing);
            cleanupTempFile(stagingPath);
        }
    } catch (...) {
        if (requiresCopyBack) {
            cleanupTempFile(stagingPath);
        }
        throw;
    }
}

void saveBinaryImage(BinaryImageType::Pointer image, const std::string& filename) {
    using WriterType = itk::ImageFileWriter<BinaryImageType>;
    const std::filesystem::path outputPath = Common::path::fromUtf8(filename);
    std::filesystem::path stagingPath;
    bool requiresCopyBack = false;

    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName(writablePathForItk(outputPath, stagingPath, requiresCopyBack));
    writer->SetInput(image);
    try {
        writer->Update();

        if (requiresCopyBack) {
            std::filesystem::copy_file(
                stagingPath, outputPath, std::filesystem::copy_options::overwrite_existing);
            cleanupTempFile(stagingPath);
        }
    } catch (...) {
        if (requiresCopyBack) {
            cleanupTempFile(stagingPath);
        }
        throw;
    }
}

void saveLabelImage(LabelImageType::Pointer image, const std::string& filename) {
    using WriterType = itk::ImageFileWriter<LabelImageType>;
    const std::filesystem::path outputPath = Common::path::fromUtf8(filename);
    std::filesystem::path stagingPath;
    bool requiresCopyBack = false;
    auto writer = WriterType::New();
    writer->SetFileName(writablePathForItk(outputPath, stagingPath, requiresCopyBack));
    writer->SetInput(image);
    writer->UseCompressionOn();
    try {
        writer->Update();
        if (requiresCopyBack) {
            std::filesystem::copy_file(stagingPath, outputPath,
                                      std::filesystem::copy_options::overwrite_existing);
            cleanupTempFile(stagingPath);
        }
    } catch (...) {
        if (requiresCopyBack) cleanupTempFile(stagingPath);
        throw;
    }
}

ImageType::Pointer loadImage(const std::string& filename, bool require3D) {
    const std::filesystem::path inputPath = Common::path::fromUtf8(filename);
    std::filesystem::path stagingPath;
    bool usesTempCopy = false;
    const std::string itkPath = readablePathForItk(inputPath, stagingPath, usesTempCopy);

    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(itkPath);
    try {
        if (require3D) {
            reader->UpdateOutputInformation();
            if (reader->GetImageIO()->GetNumberOfDimensions() != 3)
                throw std::invalid_argument("A single 3-D PET volume is required");
            if (reader->GetImageIO()->GetNumberOfComponents() != 1)
                throw std::invalid_argument("A scalar PET volume is required");
            if (std::string(reader->GetImageIO()->GetNameOfClass()) == "NiftiImageIO")
                checkFiniteNifti(itkPath);
        }
        reader->Update();
        if (usesTempCopy) {
            cleanupTempFile(stagingPath);
        }
        return reader->GetOutput();
    } catch (...) {
        if (usesTempCopy) {
            cleanupTempFile(stagingPath);
        }
        throw;
    }
}

}  // namespace Common::nifti
