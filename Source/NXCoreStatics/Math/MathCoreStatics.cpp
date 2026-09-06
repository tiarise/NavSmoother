#include "MathCoreStatics.h"

float UMathCoreStatics::GetAngleBetweenTwoVectors(const FVector& A, const FVector& B)
{
	//Returns the angle between two vectors in the 0-180 range!
	const float DiffInRadians = FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal());
	float EulerAngle = FMath::Acos(DiffInRadians) * (180.0 / PI);
	if ( EulerAngle < 0.05f ) //This method is not precise with very small angles, we correct this here.
		EulerAngle = 0.0f;
	
	return EulerAngle;	
}

FVector UMathCoreStatics::CRSplineInterpolation(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, float IFactor, float Alpha,
                                                float Tension)
{
	/*
		Basic interpolation types:
		- Linear -> Sharp angles ( /\/\/ )
		- Generic B-spline -> Smooths the path but control points are not always hit, those are guides.
		- Catmull–Rom spline -> Smoothed path where control points are always hit.
	*/
	
	//Make sure the InterpolationFactor is between 0-1.
	IFactor = FMath::Clamp(IFactor,0.0f,1.0f);
	
	float T0 = 0.0f;
	float T1 = T0 + FMath::Pow(((P1-P0).Size()),Alpha);
	float T2 = T1 + FMath::Pow(((P2-P1).Size()),Alpha);
	float T3 = T2 + FMath::Pow(((P3-P2).Size()),Alpha);
	
	//Calculate tangents at P1 and P2.
	FVector TangentP1 = (1.0f-Tension) * (T2-T1) * ( (P1-P0)/(T1-T0) - (P2-P0)/(T2-T0) + (P2-P1)/(T2-T1) );
	FVector TangentP2 = (1.0f-Tension) * (T2-T1) * ( (P2-P1)/(T2-T1) - (P3-P1)/(T3-T1) + (P3-P2)/(T3-T2) );

	//Calculate spline segments.
	FVector SplineSegment1 = 2.0f * (P1-P2) + TangentP1 + TangentP2;
	FVector SplineSegment2 = -3.0f * (P1-P2) - TangentP1 - TangentP1 - TangentP2;
	FVector SplineSegment3 = TangentP1;
	FVector SplineSegment4 = P1;
	
	return (SplineSegment1 * FMath::Pow(IFactor,3)) + (SplineSegment2 * FMath::Pow(IFactor,2)) + (SplineSegment3 * IFactor) + SplineSegment4;	
}
