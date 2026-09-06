#pragma once

#include "CoreMinimal.h"

class NXCORESTATICS_API UMathCoreStatics
{
public:
	static float GetAngleBetweenTwoVectors(const FVector& A, const FVector& B);
	static FVector CRSplineInterpolation(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3,
		float IFactor, float Alpha = 0.5f, float Tension = 0.0f);
};
