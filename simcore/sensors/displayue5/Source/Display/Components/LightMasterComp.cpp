#include "LightMasterComp.h"
#include "Components/PointLightComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Components/SpotLightComponent.h"
#include "Components/RectLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LightMasterComponent, Log, All);

ULightMasterComp::ULightMasterComp()
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these
    // features off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = false;

    // ...
}

#if WITH_EDITOR
void ULightMasterComp::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    for (auto& Elem : lampMap)
    {
        SwitchLight(Elem.Key, Elem.Value.bActive);
    }
}
#endif    // WITH_EDITOR

bool ULightMasterComp::SwitchLightByGroup(FString _Name, bool _TurnOn)
{
    TArray<FString>* Group = groupLampMap.Find(_Name);
    if (Group)
    {
        for (auto& Elem : *Group)
        {
            SwitchLightByName(Elem, _TurnOn);
        }
        return true;
    }
    UE_LOG(LightMasterComponent, Error, TEXT("The name is not group name!(ActorName: %s, GroupName: %s)"),
        *this->GetOwner()->GetName(), *_Name);
    return false;
}

void ULightMasterComp::SwitchLightByName(FString _Name, bool _TurnOn)
{
    for (auto& Elem : meshLampMap)
    {
        if (Elem.Value.Find(_Name))
        {
            SwitchLight(Elem.Key, _Name, _TurnOn);
            return;
        }
    }
    UE_LOG(LightMasterComponent, Warning,
        TEXT("Switch light failed! Can`t find lamp name!(ActorName: %s, LampName: %s)"), *this->GetOwner()->GetName(),
        *_Name);
    return;
}

bool ULightMasterComp::SwitchLight(UMeshComponent * _Mesh, FString _Name, bool _TurnOn)
{
    TMap< FString, TTuple<int, FString, class ULocalLightComponent*, bool> >* LampMap = meshLampMap.Find(_Mesh);
    if (LampMap)
    {
        TTuple<int, FString, class ULocalLightComponent*, bool>* Lamp = LampMap->Find(_Name);
        if (Lamp)
        {
            if (Lamp->Get<3>() != _TurnOn)
            {
                UMaterialInterface* Mat = _Mesh->GetMaterial(Lamp->Get<0>());
                if (Mat)
                {
                    UMaterialInstanceDynamic* MatInsDynamic = _Mesh->CreateDynamicMaterialInstance(Lamp->Get<0>(),
                    Mat); if (MatInsDynamic)
                    {
                        MatInsDynamic->SetScalarParameterValue(FName(*Lamp->Get<1>()), _TurnOn ? 1 : 0);
                        if (Lamp->Get<2>())
                        {
                            Lamp->Get<2>()->SetVisibility(_TurnOn);
                        }
                    }
                }
                Lamp->Get<3>() = _TurnOn;
            }
            return true;
        }
    }
    UE_LOG(LightMasterComponent, Warning, 
    TEXT("Switch light failed Can`t find lamp name!(ActorName: %s, LampName:%s)"),
     *this->GetOwner()->GetName(), *_Name); 
    return false;
}

bool ULightMasterComp::CreateLamp(
    const FString& _LampName, const FString& _MatSlot, const FString& _VarName, UMeshComponent* _TransportMesh)
{
    if (!_TransportMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Mesh is null!(ActorName: %s, LampName: %s)"), *this->GetOwner()->GetName(),
            *_LampName);
        return false;
    }
    if (IsLampNameExist(_LampName))
    {
        UE_LOG(LogTemp, Error, TEXT("Lamp name is exist!(ActorName: %s, LampName: %s)"), *this->GetOwner()->GetName(),
            *_LampName);
        return false;
    }
    // Set material
    TTuple<int, FString, class ULocalLightComponent*, bool> NewLamp;
    int32 MatIndex = _TransportMesh->GetMaterialIndex(*_MatSlot);
    if (MatIndex != INDEX_NONE)
    {
        UMaterialInterface* MatInterface = _TransportMesh->GetMaterial(MatIndex);
        ULocalLightComponent* Light = NULL;
        if (MatInterface)
        {
            UMaterialInstanceDynamic* MatInsDynamic =
                _TransportMesh->CreateDynamicMaterialInstance(MatIndex, MatInterface);
            float DefaultValue = 0;
            if (MatInsDynamic && MatInsDynamic->GetScalarParameterValue(*_VarName, DefaultValue))
            {
                MatInsDynamic->SetScalarParameterValue(*_VarName, 0.f);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Can`t get lamp material parameter!(ActorName: %s, LampName: %s)"),
                    *this->GetOwner()->GetName(), *_LampName);
                return false;
            }
            NewLamp.Get<0>() = MatIndex;
            NewLamp.Get<1>() = _VarName;
            NewLamp.Get<2>() = NULL;
            NewLamp.Get<3>() = false;
            // Add to map
            TMap<FString, TTuple<int, FString, class ULocalLightComponent*, bool>>* ExistLampMap =
                meshLampMap.Find(_TransportMesh);
            if (ExistLampMap)
            {
                TTuple<int, FString, class ULocalLightComponent*, bool>* ExistLamp = ExistLampMap->Find(_LampName);
                if (ExistLamp)
                {
                    *ExistLamp = NewLamp;
                }
                else
                {
                    ExistLampMap->Add(_LampName, NewLamp);
                }
            }
            else
            {
                TMap<FString, TTuple<int, FString, class ULocalLightComponent*, bool>> NewLampMap;
                NewLampMap.Add(_LampName, NewLamp);
                meshLampMap.Add(_TransportMesh, NewLampMap);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Can`t get lamp material!(ActorName: %s, LampName: %s)"),
                *this->GetOwner()->GetName(), *_LampName);
            return false;
        }
    }
    return true;
}

bool ULightMasterComp::CreateLamp(const FString& _LampName, const FLampInfo& _LampData)
{
    // Check valid
    if (_LampData.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Lamp`s data is empty!(ActorName: %s, LampName: %s)"), *this->GetOwner()->GetName(),
            *_LampName);
        return false;
    }
    // Check name exist
    if (lampMap.Find(_LampName))
    {
#if WITH_EDITOR
#else
        UE_LOG(LogTemp, Error, TEXT("Lamp name is exist!(ActorName: %s, LampName: %s)"), *this->GetOwner()->GetName(),
            *_LampName);
#endif    // WITH_EDITOR
        return false;
    }
    FLampInfo NewLampInfo;
    // Check materials legal
    for (auto& Elem : _LampData.materialArry)
    {
        if (Elem.matInstance)
        {
            float Value = 0;
            if (Elem.matInstance->GetScalarParameterValue(*Elem.paramName, Value))
            {
                NewLampInfo.materialArry.Add(Elem);
            }
            else
            {
                UE_LOG(LogTemp, Error,
                    TEXT("Can`t get lamp`s MaterialParameterName!(ActorName: %s, ParameterName: %s)"),
                    *this->GetOwner()->GetName(), *Elem.paramName);
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Lamp`s MaterialDynamicInstance is null!(ActorName: %s, LampName: %s)"),
                *this->GetOwner()->GetName(), *_LampName);
        }
    }
    // Set lights legal
    for (auto& Elem : _LampData.lightArry)
    {
        if (Elem)
        {
            NewLampInfo.lightArry.Add(Elem);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Lamp`s LightComponent is null!(ActorName: %s, LampName: %s)"),
                *this->GetOwner()->GetName(), *_LampName);
        }
    }
    // Check valid
    if (NewLampInfo.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Lamp`s data of valid is empty!(ActorName: %s, LampName: %s)"),
            *this->GetOwner()->GetName(), *_LampName);
        return true;
    }
    else
    {
        lampMap.Add(_LampName, NewLampInfo);
        return false;
    }
}

void ULightMasterComp::SwitchLight(FString _Name, bool _TurnOn)
{
    FLampInfo* Lamp = lampMap.Find(_Name);
    if (Lamp)
    {
        // Switch material
        for (auto& Elem : Lamp->materialArry)
        {
            if (Elem.matInstance)
            {
                Elem.matInstance->SetScalarParameterValue(*Elem.paramName, _TurnOn ? 1 : 0);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Lamp`s MaterialInstanceDynamic is null!(ActorName: %s, LampName: %s)"),
                    *this->GetOwner()->GetName(), *_Name);
            }
        }
        // Switch lightcomponent
        for (auto& Elem : Lamp->lightArry)
        {
            if (Elem)
            {
                Elem->SetVisibility(_TurnOn);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Lamp`s LightComponent is null!(ActorName: %s, LampName: %s)"),
                    *this->GetOwner()->GetName(), *_Name);
            }
        }
        Lamp->bActive = _TurnOn;
    }
}

bool ULightMasterComp::GroupLamp(const FString& _GroupName, const TArray<FString>& _LampNames)
{
    if (groupLampMap.Find(_GroupName))
    {
        UE_LOG(LogTemp, Error, TEXT("Group name exist!(ActorName: %s, GroupName: %s)"), *this->GetOwner()->GetName(),
            *_GroupName);
        return false;
    }
    TArray<FString> NewGroup;
    for (auto& Elem : _LampNames)
    {
        if (IsLampNameExist(Elem))
        {
            NewGroup.Add(Elem);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("Lamp name to group is not register!(ActorName: %s, LampName: %s)"),
                *this->GetOwner()->GetName(), *Elem);
        }
    }
    if (NewGroup.Num() > 0)
    {
        groupLampMap.Add(_GroupName, NewGroup);
        return true;
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("There is no valid name in group!(ActorName: %s, GroupName: %s)"),
            *this->GetOwner()->GetName(), *_GroupName);
        return false;
    }
}

const TMap<FString, TTuple<int, FString, class ULocalLightComponent*, bool>>* ULightMasterComp::GetLampMapByMesh(
    const UMeshComponent* _Mesh)
{
    TMap<FString, TTuple<int, FString, class ULocalLightComponent*, bool>>* LampMap = meshLampMap.Find(_Mesh);
    return LampMap;
}

TTuple<int, FString, class ULocalLightComponent*, bool>* ULightMasterComp::GetLamp(
    UMeshComponent* _Mesh, const FString& _LampName)
{
    TMap<FString, TTuple<int, FString, class ULocalLightComponent*, bool>>* LampMap = meshLampMap.Find(_Mesh);
    if (LampMap)
    {
        return LampMap->Find(_LampName);
    }
    return nullptr;
}

FLampInfo* ULightMasterComp::GetLamp(const FString& _LampName)
{
    return lampMap.Find(_LampName);
}

void ULightMasterComp::DeleteAllLamp()
{
    lampMap.Empty();
}

bool ULightMasterComp::IsLampNameExist(const FString& _NewLampName)
{
    if (GetLamp(_NewLampName))
    {
        return true;
    }
    return false;
}

bool ULightMasterComp::IsGroupNameExist(const FString& _GroupName)
{
    return groupLampMap.Find(_GroupName) != nullptr;
}