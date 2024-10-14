#pragma once

#include "CoreMinimal.h"
#include "Runtime/Core/Public/HAL/Runnable.h"
#include "Runtime/Core/Public/HAL/RunnableThread.h"
#include "Runtime/Core/Public/HAL/PlatformProcess.h"
#include "Runtime/Core/Public/HAL/ThreadSafeBool.h"
#include "Runtime/ImageWrapper/Public/IImageWrapperModule.h"
#include "Runtime/ImageWrapper/Public/IImageWrapper.h"
#include "Networking.h"

struct FStringData
{
    FString data;
    FString path;
};

struct FJPGData
{
    TArray<uint8> data;
    FString path;
};

struct FBmpData
{
    TArray<FColor> data;
    int32 width;
    int32 height;
    int32 port;
};

class DISPLAY_API SaveDataThread : public FRunnable
{
public:
    SaveDataThread();
    ~SaveDataThread();

    // FRunnable interface.
    virtual bool Init();
    virtual uint32 Run();
    virtual void Stop();

private:
    FRunnableThread* Thread;

    FCriticalSection m_mutex;
    FCriticalSection mutexImage;

    FEvent* m_semaphore;

    // As the name states those members are Thread safe
    FThreadSafeBool m_Kill;
    FThreadSafeBool m_Pause;

    TArray<FStringData> stringDataArry;
    TArray<FJPGData> jpgDataArry;
    TArray<FBmpData> bmpDataArry;

    TSharedPtr<IImageWrapper> ImageWrapper;

    // udp
    FSocket* SenderSocket = nullptr;
    struct InternetAddr
    {
        int frame;
        TSharedPtr<FInternetAddr> addr;
    };
    TMap<int32, InternetAddr> remoteAddr;

    void SaveData();

};