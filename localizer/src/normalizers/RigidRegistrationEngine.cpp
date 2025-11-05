#include "RigidRegistrationEngine.h"
#include <vnl/vnl_vector.h>
#include <iostream>
#include "itkCrossHelper.h"
#include "../utils/onnx_path_utils.h"

// Helper functions from original Rigid.cpp
namespace {
    ImageType::PointType getPhysicalPoint(const std::vector<float> voxelPoint,
                                          const ImageType::DirectionType& direction,
                                          const ImageType::PointType& origin,
                                          const ImageType::SpacingType& spacing) {
        ImageType::PointType physicalPoint;
        for (unsigned int i = 0; i < 3; i++) {
            physicalPoint[i] = origin[i];
            for (unsigned int j = 0; j < 3; j++) {
                physicalPoint[i] += direction[i][j] * voxelPoint[j] * spacing[j];
            }
        }
        return physicalPoint;
    }

    vnl_vector<double> world2voxel(const vnl_vector<double> world,
                                   const itk::Matrix<double, 3, 3>& direction,
                                   const vnl_vector<double>& origin,
                                   const vnl_vector<double>& spacing) {
        auto tmp = direction.GetInverse() * (world - origin);
        for (unsigned int i = 0; i < 3; i++) tmp[i] /= spacing[i];
        return tmp;
    }

    itk::Vector<double, 3> normalizeVector(const itk::Vector<double, 3>& vec) {
        double norm = vec.GetNorm();
        itk::Vector<double, 3> normalizedVec = vec;
        for (unsigned int i = 0; i < 3; ++i) {
            normalizedVec[i] /= norm;
        }
        return normalizedVec;
    }
}

RigidRegistrationEngine::RigidRegistrationEngine(const std::string& modelPath)
    : env_(ORT_LOGGING_LEVEL_WARNING, "RigidRegistration"), session_(nullptr) {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(1);
    auto ortModelPath = OrtUtils::MakeOrtPath(modelPath);

    try {
        session_ = new Ort::Session(env_, ortModelPath.c_str(), sessionOptions);
    } catch (const Ort::Exception& e) {
        std::cerr << "Error loading rigid registration model: " << e.what() << std::endl;
        throw std::runtime_error("Failed to load rigid registration model.");
    }
}

RigidRegistrationEngine::~RigidRegistrationEngine() {
    if (session_) delete session_;
}

std::unordered_map<std::string, std::vector<float>> RigidRegistrationEngine::predict(
    const std::vector<float>& inputTensor, const std::vector<int64_t>& inputShape) {
    Ort::AllocatorWithDefaultOptions allocator;
    
    // Prepare input tensor
    auto input_name_allocated = session_->GetInputNameAllocated(0, allocator);
    const char* input_name = input_name_allocated.get();
    
    std::vector<int64_t> input_shape = {1, 1, 64, 64, 64};
    Ort::MemoryInfo memory_info =
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeDefault);
    Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
        memory_info, const_cast<float*>(inputTensor.data()), inputTensor.size(), 
        input_shape.data(), input_shape.size());
    
    // Define output names
    const char* output_names[] = {"ac", "nose", "top"};
    size_t num_outputs = 3;

    // Run inference
    std::vector<Ort::Value> output_tensors;
    std::vector<std::vector<float>> output_buffers(num_outputs,
                                                   std::vector<float>(3));
    for (size_t i = 0; i < num_outputs; ++i) {
        Ort::Value output_tensor = Ort::Value::CreateTensor<float>(
            memory_info, output_buffers[i].data(), output_buffers[i].size(),
            std::vector<int64_t>{1, 3}.data(), 2);
        output_tensors.push_back(std::move(output_tensor));
    }

    session_->Run(Ort::RunOptions{nullptr}, &input_name, &input_tensor, 1,
                  output_names, output_tensors.data(), 3);

    std::unordered_map<std::string, std::vector<float>> output;
    for (size_t i = 0; i < output_tensors.size(); i++) {
        Ort::TensorTypeAndShapeInfo output_info =
            output_tensors[i].GetTensorTypeAndShapeInfo();

        float* output_data = output_tensors[i].GetTensorMutableData<float>();
        std::vector<float> output_vector(
            output_data, output_data + output_info.GetElementCount());
        output[output_names[i]] = output_vector;
    }

    return output;
}

std::tuple<ImageType::PointType, ImageType::DirectionType>
RigidRegistrationEngine::getNewOriginAndDirection(
    ImageType::Pointer preprocessedImage, ImageType::Pointer originalImage,
    const std::vector<float>& AC, const std::vector<float>& PA,
    const std::vector<float>& IS) {
    
    std::vector<float> ac = AC, pa = PA, is = IS;
    std::for_each(ac.begin(), ac.end(), [](float& x) { x *= 64; });
    std::for_each(pa.begin(), pa.end(), [](float& x) { x *= 99999; });
    std::for_each(is.begin(), is.end(), [](float& x) { x *= 99999; });

    const ImageType::DirectionType& preprocessedDirection =
        preprocessedImage->GetDirection();
    const ImageType::PointType& preprocessedOrigin =
        preprocessedImage->GetOrigin();
    const ImageType::SpacingType& preprocessedSpacing =
        preprocessedImage->GetSpacing();
    const ImageType::SpacingType& originalSpacing = originalImage->GetSpacing();

    // Calculate ac nose top in physical space
    auto acPhysical = getPhysicalPoint(ac, preprocessedDirection,
                                       preprocessedOrigin, preprocessedSpacing);
    auto originalVoxelAC =
        world2voxel(acPhysical.GetVnlVector(), originalImage->GetDirection(),
                    originalImage->GetOrigin().GetVnlVector(),
                    originalImage->GetSpacing().GetVnlVector());

    auto nosePhysical = getPhysicalPoint(pa, preprocessedDirection,
                                         preprocessedOrigin, preprocessedSpacing);
    auto zeroPhysical =
        getPhysicalPoint(std::vector<float>{0, 0, 0}, preprocessedDirection,
                         preprocessedOrigin, preprocessedSpacing);
    auto topPhysical = getPhysicalPoint(is, preprocessedDirection,
                                        preprocessedOrigin, preprocessedSpacing);
    itk::Vector<double, 3> noseVec, topVec;
    for (unsigned int i = 0; i < 3; i++) {
        noseVec[i] = nosePhysical[i] - zeroPhysical[i];
        topVec[i] = topPhysical[i] - zeroPhysical[i];
    }
    
    float projectionLength = 0;
    itk::Vector<double, 3> topNormalVec = normalizeVector(topVec);
    for (unsigned int i = 0; i < 3; i++) {
        projectionLength += noseVec[i] * topNormalVec[i];
    }
    for (unsigned int i = 0; i < 3; i++)
        noseVec[i] -= projectionLength * topNormalVec[i];
    itk::Vector<double, 3> noseNormalVec = normalizeVector(noseVec);
    itk::Vector<double, 3> orthoVec =
        itk::CrossProduct(noseNormalVec, topNormalVec);

    ImageType::DirectionType newDirection;
    for (unsigned int i = 0; i < 3; i++) {
        newDirection(0, i) = -orthoVec[i];
        newDirection(1, i) = -noseNormalVec[i];
        newDirection(2, i) = topNormalVec[i];
    }
    newDirection = newDirection * originalImage->GetDirection();

    ImageType::PointType elementWiseProduct;
    for (unsigned i = 0; i < 3; i++) {
        elementWiseProduct[i] = originalSpacing[i] * originalVoxelAC[i];
    }
    ImageType::PointType newOrigin = newDirection * elementWiseProduct;

    for (unsigned i = 0; i < 3; i++) {
        newOrigin[i] = -newOrigin[i];
    }
    return std::tuple<ImageType::PointType, ImageType::DirectionType>(
        newOrigin, newDirection);
}
