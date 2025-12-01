#include "SharedMemoryImageTransport.h"
#include "HAL/PlatformAtomics.h"
#include "Misc/DateTime.h"

FSharedMemoryImageTransport::FSharedMemoryImageTransport()
    : SharedMemoryFD(-1)
    , SharedMemoryPtr(MAP_FAILED)
    , TotalSize(0)
    , Header(nullptr)
    , SlotDataStart(nullptr)
    , WriteSemaphore(SEM_FAILED)
    , ReadSemaphore(SEM_FAILED)
    , bIsInitialized(false)
    , MaxSlots(0)
    , SingleSlotSize(0)
{
}

FSharedMemoryImageTransport::~FSharedMemoryImageTransport()
{
    Shutdown();
}

bool FSharedMemoryImageTransport::Initialize(uint32 NumSlots, uint32 MaxWidth, uint32 MaxHeight)
{
    if (bIsInitialized)
    {
        UE_LOG(LogTemp, Warning, TEXT("SharedMemoryImageTransport already initialized"));
        return true;
    }

    MaxSlots = NumSlots;

    // Calculate slot size: header + max RGB24 data
    SingleSlotSize = FSharedImageSlot::HeaderSize + (MaxWidth * MaxHeight * 3);

    // Align to page boundary for performance
    SingleSlotSize = (SingleSlotSize + 4095) & ~4095;

    // Total size: header + all slots
    TotalSize = sizeof(FSharedMemoryHeader) + (SingleSlotSize * MaxSlots);

    // Pad header to 4KB boundary
    size_t HeaderPadding = 4096 - sizeof(FSharedMemoryHeader);
    TotalSize += HeaderPadding;

    // Generate unique shared memory name with process ID
    SharedMemoryName = FString::Printf(TEXT("/airsim_images_%d"), FPlatformProcess::GetCurrentProcessId());

    // Open/create shared memory
    SharedMemoryFD = shm_open(TCHAR_TO_UTF8(*SharedMemoryName), O_CREAT | O_RDWR, 0666);
    if (SharedMemoryFD < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create shared memory: %s"), *SharedMemoryName);
        return false;
    }

    // Set size
    if (ftruncate(SharedMemoryFD, TotalSize) < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to set shared memory size"));
        close(SharedMemoryFD);
        shm_unlink(TCHAR_TO_UTF8(*SharedMemoryName));
        return false;
    }

    // Map into process address space
    SharedMemoryPtr = MapSharedMemory(TotalSize);
    if (SharedMemoryPtr == MAP_FAILED)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to map shared memory"));
        close(SharedMemoryFD);
        shm_unlink(TCHAR_TO_UTF8(*SharedMemoryName));
        return false;
    }

    // Initialize header
    Header = static_cast<FSharedMemoryHeader*>(SharedMemoryPtr);
    Header->MagicNumber = FSharedMemoryHeader::MAGIC_NUMBER;
    Header->NumSlots = MaxSlots;
    Header->SlotSize = SingleSlotSize;
    Header->WriteIndex = 0;
    Header->LastUpdateTime = 0;

    // Slot data starts after 4KB header
    SlotDataStart = static_cast<uint8*>(SharedMemoryPtr) + 4096;

    // Create semaphores for synchronization
    FString WriteSemName = FString::Printf(TEXT("/airsim_write_%d"), FPlatformProcess::GetCurrentProcessId());
    FString ReadSemName = FString::Printf(TEXT("/airsim_read_%d"), FPlatformProcess::GetCurrentProcessId());

    WriteSemaphore = sem_open(TCHAR_TO_UTF8(*WriteSemName), O_CREAT, 0666, MaxSlots);
    ReadSemaphore = sem_open(TCHAR_TO_UTF8(*ReadSemName), O_CREAT, 0666, 0);

    if (WriteSemaphore == SEM_FAILED || ReadSemaphore == SEM_FAILED)
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to create semaphores"));
        Shutdown();
        return false;
    }

    bIsInitialized = true;
    UE_LOG(LogTemp, Log, TEXT("Shared memory initialized: %s (%zu bytes, %d slots)"),
        *SharedMemoryName, TotalSize, MaxSlots);

    return true;
}

bool FSharedMemoryImageTransport::WriteImage(uint32 Width, uint32 Height, uint64 Timestamp, uint32 ImageType, const TArray<uint8>& PixelData)
{
    if (!bIsInitialized || !Header)
    {
        return false;
    }

    // Validate dimensions to prevent invalid writes
    if (Width == 0 || Height == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid image dimensions: %dx%d"), Width, Height);
        return false;
    }

    uint32 RequiredSize = Width * Height * 3;
    if (PixelData.Num() < RequiredSize)
    {
        UE_LOG(LogTemp, Warning, TEXT("Insufficient pixel data: %d < %d"), PixelData.Num(), RequiredSize);
        return false;
    }

    // Wait for available slot (non-blocking with timeout)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 5000000; // 5ms timeout
    if (ts.tv_nsec >= 1000000000)
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    if (sem_timedwait(WriteSemaphore, &ts) < 0)
    {
        // No slots available, skip this frame
        return false;
    }

    // Get next write slot
    uint32 SlotIndex = GetNextWriteIndex();
    FSharedImageSlot* Slot = GetSlot(SlotIndex);

    if (!Slot)
    {
        sem_post(WriteSemaphore);
        return false;
    }

    // Write slot data
    Slot->Width = Width;
    Slot->Height = Height;
    Slot->Timestamp = Timestamp;
    Slot->ImageType = ImageType;
    Slot->DataSize = RequiredSize;

    // Zero-copy memcpy of pixel data
    FMemory::Memcpy(Slot->PixelData, PixelData.GetData(), RequiredSize);

    // Update header
    Header->LastUpdateTime = Timestamp;

    // Signal reader
    sem_post(ReadSemaphore);

    return true;
}

bool FSharedMemoryImageTransport::WriteImages(const TArray<FSharedImageSlot*>& Images)
{
    // Batch write multiple images
    bool bAllSuccess = true;
    for (FSharedImageSlot* Image : Images)
    {
        TArray<uint8> PixelData;
        PixelData.Append(Image->PixelData, Image->DataSize);
        bAllSuccess &= WriteImage(Image->Width, Image->Height, Image->Timestamp, Image->ImageType, PixelData);
    }
    return bAllSuccess;
}

void FSharedMemoryImageTransport::Shutdown()
{
    if (WriteSemaphore != SEM_FAILED)
    {
        sem_close(WriteSemaphore);
        FString SemName = FString::Printf(TEXT("/airsim_write_%d"), FPlatformProcess::GetCurrentProcessId());
        sem_unlink(TCHAR_TO_UTF8(*SemName));
        WriteSemaphore = SEM_FAILED;
    }

    if (ReadSemaphore != SEM_FAILED)
    {
        sem_close(ReadSemaphore);
        FString SemName = FString::Printf(TEXT("/airsim_read_%d"), FPlatformProcess::GetCurrentProcessId());
        sem_unlink(TCHAR_TO_UTF8(*SemName));
        ReadSemaphore = SEM_FAILED;
    }

    UnmapSharedMemory();

    if (SharedMemoryFD >= 0)
    {
        close(SharedMemoryFD);
        shm_unlink(TCHAR_TO_UTF8(*SharedMemoryName));
        SharedMemoryFD = -1;
    }

    bIsInitialized = false;
}

FSharedImageSlot* FSharedMemoryImageTransport::GetSlot(uint32 Index)
{
    if (Index >= MaxSlots)
    {
        return nullptr;
    }

    uint8* SlotPtr = SlotDataStart + (Index * SingleSlotSize);
    return reinterpret_cast<FSharedImageSlot*>(SlotPtr);
}

uint32 FSharedMemoryImageTransport::GetNextWriteIndex()
{
    // Atomic increment with wrap-around using proper CAS loop
    uint32 CurrentIndex, NextIndex;
    do
    {
        CurrentIndex = Header->WriteIndex;
        NextIndex = (CurrentIndex + 1) % MaxSlots;
        // Retry until CAS succeeds (returns the old value == CurrentIndex)
    } while (FPlatformAtomics::InterlockedCompareExchange((volatile int32*)&Header->WriteIndex, NextIndex, CurrentIndex) != CurrentIndex);

    return CurrentIndex;
}

void* FSharedMemoryImageTransport::MapSharedMemory(size_t Size)
{
    return mmap(nullptr, Size, PROT_READ | PROT_WRITE, MAP_SHARED, SharedMemoryFD, 0);
}

void FSharedMemoryImageTransport::UnmapSharedMemory()
{
    if (SharedMemoryPtr != MAP_FAILED && TotalSize > 0)
    {
        munmap(SharedMemoryPtr, TotalSize);
        SharedMemoryPtr = MAP_FAILED;
    }
}
