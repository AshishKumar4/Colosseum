#pragma once

#include "CoreMinimal.h"
#include "PIPCamera.h"
#include "common/ImageCaptureBase.hpp"
#include "common/common_utils/UniqueValueMap.hpp"
#include "SharedMemoryImageTransport.h"

class UnrealImageCapture : public msr::airlib::ImageCaptureBase
{
public:
    typedef msr::airlib::ImageCaptureBase::ImageType ImageType;

    UnrealImageCapture(const common_utils::UniqueValueMap<std::string, APIPCamera*>* cameras);
    virtual ~UnrealImageCapture();

    virtual void getImages(const std::vector<ImageRequest>& requests, std::vector<ImageResponse>& responses) const override;

    // Enable/disable shared memory transport (disabled by default for backwards compatibility)
    void EnableSharedMemory(bool bEnable);
    bool IsSharedMemoryEnabled() const { return bUseSharedMemory; }

private:
    void getSceneCaptureImage(const std::vector<msr::airlib::ImageCaptureBase::ImageRequest>& requests,
                              std::vector<msr::airlib::ImageCaptureBase::ImageResponse>& responses, bool use_safe_method) const;

    void addScreenCaptureHandler(UWorld* world);
    bool getScreenshotScreen(ImageType image_type, std::vector<uint8_t>& compressedPng);

    bool updateCameraVisibility(APIPCamera* camera, const msr::airlib::ImageCaptureBase::ImageRequest& request);

    // Convert FColor (BGRA) to RGB24 for shared memory
    void ConvertToRGB24(const TArray<FColor>& BGRAData, TArray<uint8>& RGB24Data) const;

private:
    const common_utils::UniqueValueMap<std::string, APIPCamera*>* cameras_;
    std::vector<uint8_t> last_compressed_png_;

    // Shared memory transport (optional, for localhost high-performance mode)
    FSharedMemoryImageTransport* SharedMemTransport;
    bool bUseSharedMemory;
};
