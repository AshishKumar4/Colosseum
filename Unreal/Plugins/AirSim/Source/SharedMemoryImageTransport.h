#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

/**
 * Shared memory transport for zero-copy image transfer between Unreal and Python client
 * Uses POSIX shared memory (shm_open/mmap) for maximum performance on localhost
 *
 * Memory layout:
 * [Header: 4KB]
 *   - Magic number (verification)
 *   - Number of slots
 *   - Slot size
 *   - Write index (atomic)
 *   - Timestamps
 * [Slot 0: ImageData]
 * [Slot 1: ImageData]
 * [Slot 2: ImageData]
 * ...
 *
 * Each slot contains:
 *   - Width (4 bytes)
 *   - Height (4 bytes)
 *   - Timestamp (8 bytes)
 *   - ImageType (4 bytes)
 *   - DataSize (4 bytes)
 *   - RGB24 pixel data (Width * Height * 3 bytes)
 */

struct FSharedImageSlot
{
    uint32 Width;
    uint32 Height;
    uint64 Timestamp;
    uint32 ImageType;
    uint32 DataSize;
    uint8 PixelData[1]; // Variable length: Width * Height * 3

    static constexpr uint32 HeaderSize = sizeof(Width) + sizeof(Height) + sizeof(Timestamp) + sizeof(ImageType) + sizeof(DataSize);
};

struct FSharedMemoryHeader
{
    uint32 MagicNumber;     // 0x41495253 ('AIRS' in hex)
    uint32 NumSlots;
    uint32 SlotSize;        // Max size per slot in bytes
    uint32 WriteIndex;      // Current write position (atomic)
    uint64 LastUpdateTime;  // Last write timestamp

    static constexpr uint32 MAGIC_NUMBER = 0x41495253;
};

class AIRSIM_API FSharedMemoryImageTransport
{
public:
    FSharedMemoryImageTransport();
    ~FSharedMemoryImageTransport();

    // Initialize shared memory with specified number of slots and max image size
    bool Initialize(uint32 NumSlots = 3, uint32 MaxWidth = 1920, uint32 MaxHeight = 1080);

    // Write image data to next available slot (non-blocking, zero-copy)
    bool WriteImage(uint32 Width, uint32 Height, uint64 Timestamp, uint32 ImageType, const TArray<uint8>& PixelData);

    // Write multiple images in batch
    bool WriteImages(const TArray<FSharedImageSlot*>& Images);

    // Cleanup shared memory
    void Shutdown();

    // Check if shared memory is initialized
    bool IsInitialized() const { return bIsInitialized; }

    // Get shared memory name (for Python client to connect)
    FString GetSharedMemoryName() const { return SharedMemoryName; }

private:
    FString SharedMemoryName;
    int32 SharedMemoryFD;
    void* SharedMemoryPtr;
    size_t TotalSize;

    FSharedMemoryHeader* Header;
    uint8* SlotDataStart;

    sem_t* WriteSemaphore;
    sem_t* ReadSemaphore;

    bool bIsInitialized;
    uint32 MaxSlots;
    uint32 SingleSlotSize;

    // Helper functions
    FSharedImageSlot* GetSlot(uint32 Index);
    uint32 GetNextWriteIndex();
    void* MapSharedMemory(size_t Size);
    void UnmapSharedMemory();
};
