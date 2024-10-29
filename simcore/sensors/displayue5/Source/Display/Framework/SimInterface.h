#pragma once

#include "CoreMinimal.h"
#include <string>
#include "UObject/Interface.h"

#include "SimInterface.generated.h"

UENUM(BlueprintType)
enum class ESimState : uint8
{
    SA_DONE,
    SA_INIT,
    SA_RESET,
    SA_UPDATE
};

// USTRUCT()
struct FSimData
{
    // GENERATED_BODY()
public:
    virtual ~FSimData()
    {
    }
    int datatype = 0;

    double timeStamp = 0.f;
    ESimState state = ESimState::SA_DONE;
    FString name;
    int bIsConsumed = 0;
    double timeStamp_ego = 0.f;
    double timeStamp_tail = 0.f;
};

USTRUCT()
struct FLocalData
{
    GENERATED_BODY()
public:
    // bool bIsSuccess = false;
    double timeStamp = 0.f;
    ESimState state = ESimState::SA_DONE;
    FString name;
    double timeStamp_ego = 0.f;
    double timeStamp_tail = 0.f;
};