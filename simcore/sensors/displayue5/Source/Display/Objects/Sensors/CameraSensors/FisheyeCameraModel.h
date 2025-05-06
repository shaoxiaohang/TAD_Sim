// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * @ref https://zhuanlan.zhihu.com/p/356925508
 */
class DISPLAY_API FFisheyeCameraModel
{
public:
	FFisheyeCameraModel(const int& InWidth, const int& InHeight,
		const FVector2d InFocalLength, const FVector2d InPrincipalPoint,
		const double& InK1, const double& InK2, const double& InK3, const double& InK4);
	~FFisheyeCameraModel();
	TArray<FVector> GenerateDirectionMapping();
	
private:
	void Distort(double u, double v, double* du, double* dv) const;
	void IterativeUndistort(const double du, const double dv, double* u,
						double* v) const;
	
	FVector2d ImageToWorld(int i, int j);
	int Width;
	int Height;
	FVector2d FocalLength;
	FVector2d PrincipalPoint; // Center
	double K1;
	double K2;
	double K3;
	double K4;
};
