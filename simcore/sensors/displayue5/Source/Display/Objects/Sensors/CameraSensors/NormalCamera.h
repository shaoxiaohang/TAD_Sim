// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CameraSensor.h"
#include "NormalCamera.generated.h"

/**
 *
 */
UCLASS()
class DISPLAY_API ANormalCamera : public ACameraSensor
{
    GENERATED_BODY()
public:
    ANormalCamera();
    ~ANormalCamera();

    virtual bool Init(const FSensorConfig& _Config);
};
