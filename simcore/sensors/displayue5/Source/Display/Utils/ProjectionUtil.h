#pragma once

#include "CoreMinimal.h"

namespace util
{
	FMatrix CalcProjectionMatrix(FVector2f horizonFovRangle, FVector2f verticalFovRange, float near_plane);

	FMatrix CalcProjectionMatrix(float horizonFov, FVector2f verticalFovRange, float near_plane);
	
	FMatrix CalcProjectionMatrix(float FovHorizon, float FovVertical, float NearPlane);

	// using infinite as far
	// see also Runtime/Core/Public/Math/PerspectiveMatrix.h
	FMatrix CalcProjectionMatrix(float left, float right, float top, float bottom, float near_plane);

	FMatrix CalcProjectionMatrix(float fx, float fy, float cx, float cy, int width, int height, float near_plane);

}; // namespace util