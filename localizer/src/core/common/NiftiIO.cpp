#include "NiftiIO.h"

#include <itkImageFileWriter.h>

namespace refactorCommon::nifti {

void saveImage(ImageType::Pointer image, const std::string& filename) {
    using WriterType = itk::ImageFileWriter<ImageType>;
    WriterType::Pointer writer = WriterType::New();
    writer->SetFileName(filename);
    writer->SetInput(image);
    writer->Update();
}

ImageType::Pointer loadImage(const std::string& filename) {
    ReaderType::Pointer reader = ReaderType::New();
    reader->SetFileName(filename);
    reader->Update();
    return reader->GetOutput();
}

}  // namespace refactorCommon::nifti


