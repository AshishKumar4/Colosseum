#pragma once

#include "CoreMinimal.h"
#include "Async/AsyncWork.h"
#include "RenderRequest.h"
#include "AirBlueprintLib.h"

// Async image compression and packing task
// Offloads PNG compression to background thread pool
class FImagePackingAsyncTask : public FNonAbandonableTask
{
    friend class FAutoDeleteAsyncTask<FImagePackingAsyncTask>;

public:
    FImagePackingAsyncTask(
        std::shared_ptr<RenderRequest::RenderParams>* InParams,
        std::vector<std::shared_ptr<RenderRequest::RenderResult>>& InResults,
        unsigned int InReqSize)
    {
        ReqSize = InReqSize;
        Results = std::move(InResults);

        Params.Reserve(InReqSize);
        for (unsigned int i = 0; i < InReqSize; ++i) {
            Params.Add(InParams[i]);
        }
    }

    void DoWork()
    {
        for (unsigned int i = 0; i < ReqSize; ++i) {
            auto& result = Results[i];
            const auto& param = Params[i];

            if (!param->pixels_as_float) {
                if (result->width != 0 && result->height != 0) {
                    result->image_data_uint8.SetNumUninitialized(
                        result->width * result->height * 3, false);

                    if (param->compress) {
                        UAirBlueprintLib::CompressImageArray(
                            result->width,
                            result->height,
                            result->bmp,
                            result->image_data_uint8);
                    }
                    else {
                        uint8* ptr = result->image_data_uint8.GetData();
                        for (const auto& item : result->bmp) {
                            *ptr++ = item.B;
                            *ptr++ = item.G;
                            *ptr++ = item.R;
                        }
                    }
                }
            }
            else {
                result->image_data_float.SetNumUninitialized(
                    result->width * result->height);

                float* ptr = result->image_data_float.GetData();
                for (const auto& item : result->bmp_float) {
                    *ptr++ = item.R.GetFloat();
                }
            }
        }
    }

    FORCEINLINE TStatId GetStatId() const
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(FImagePackingAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
    }

private:
    TArray<std::shared_ptr<RenderRequest::RenderParams>> Params;
    std::vector<std::shared_ptr<RenderRequest::RenderResult>> Results;
    unsigned int ReqSize;
};
