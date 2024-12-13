#include "Utils/ProjectionUtil.h"
#include "Math/UnrealMathUtility.h"
namespace util
{

	FMatrix CalcProjectionMatrix(FVector2f horizonFovRangle, FVector2f verticalFovRange, float near_plane)
	{
		float left = near_plane * FMath::Tan(FMath::DegreesToRadians(horizonFovRangle.X));
		float right = near_plane * FMath::Tan(FMath::DegreesToRadians(horizonFovRangle.Y));

		float top = near_plane * FMath::Tan(FMath::DegreesToRadians(verticalFovRange.Y));
		float bottom = near_plane * FMath::Tan(FMath::DegreesToRadians(verticalFovRange.X));

		return CalcProjectionMatrix(left, right, top, bottom, near_plane);
	}
	
	FMatrix CalcProjectionMatrix(float horizonFov, FVector2f verticalFovRange, float near_plane)
	{
		float halfFovH = FMath::DegreesToRadians(horizonFov * 0.5);
		float left = -near_plane * FMath::Tan(halfFovH);
		float right = near_plane * FMath::Tan(halfFovH);

		float top = near_plane * FMath::Tan(FMath::DegreesToRadians(verticalFovRange.Y));
		float bottom = near_plane * FMath::Tan(FMath::DegreesToRadians(verticalFovRange.X));

		return CalcProjectionMatrix(left, right, top, bottom, near_plane);
	}

	FMatrix CalcProjectionMatrix(float FovHorizon, float FovVertical, float NearPlane)
	{
		float halfFovH = FMath::DegreesToRadians(FovHorizon * 0.5);
		float left = -NearPlane * FMath::Tan(halfFovH);
		float right = NearPlane * FMath::Tan(halfFovH);

		float halfFovV = FMath::DegreesToRadians(FovVertical * 0.5);
		float top = NearPlane * FMath::Tan(halfFovV);
		float bottom = -NearPlane * FMath::Tan(halfFovV);

		return CalcProjectionMatrix(left, right, top, bottom, NearPlane);
	}


  //see Engine\Source\Runtime\Core\Public\Math\PerspectiveMatrix.h
	/*
	FORCEINLINE TReversedZPerspectiveMatrix<T>::TReversedZPerspectiveMatrix(T HalfFOV, T Width, T Height, T MinZ)
	*/
	FMatrix CalcProjectionMatrix(float left, float right, float top, float bottom, float near_plane)
	{
		FMatrix projectionMatrix;
		projectionMatrix.M[0][0] = 2 * near_plane / (right - left); // 2n/(r-l)
		projectionMatrix.M[0][1] = 0;
		projectionMatrix.M[0][2] = 0;
		projectionMatrix.M[0][3] = 0;

		projectionMatrix.M[1][0] = 0;
		projectionMatrix.M[1][1] = 2 * near_plane / (top - bottom); // 2n / (t - b)
		projectionMatrix.M[1][2] = 0;
		projectionMatrix.M[1][3] = 0;

		projectionMatrix.M[2][0] = (left + right) / (left - right);
		projectionMatrix.M[2][1] = (bottom + top) / (bottom - top);
		projectionMatrix.M[2][2] = 0;
		projectionMatrix.M[2][3] = 1.0;

		projectionMatrix.M[3][0] = 0;
		projectionMatrix.M[3][1] = 0;
		projectionMatrix.M[3][2] = near_plane;
		projectionMatrix.M[3][3] = 0;

		return projectionMatrix;
	}

	FMatrix CalcProjectionMatrix(float fx, float fy, float cx, float cy, int width, int height, float near_plane)
	{
		float left = -near_plane * cx / fx;
		float right = near_plane * (width - cx) / fx;

		float top = near_plane * cy / fy;
		float bottom = -near_plane * (height - cy) / fy;

		return CalcProjectionMatrix(left, right, top, bottom, near_plane);
	}
} // namespace util
