#include "NiftiIO.h"

#include "PathUtils.h"
#include <chrono>
#include <filesystem>
#include <itkImageFileWriter.h>

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

ImageType::Pointer loadImage(const std::string& filename) {
    const std::filesystem::path inputPath = Common::path::fromUtf8(filename);
    std::filesystem::path stagingPath;
    bool usesTempCopy = false;
    const std::string itkPath = readablePathForItk(inputPath, stagingPath, usesTempCopy);

    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(itkPath);
    try {
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
