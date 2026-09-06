#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "NavigationData.h" //FPathFindingResult
#include "NavSmootherCore.generated.h"

UENUM()
enum class ESmoothingMethod : uint8
{
	CRSplineSmoothing, //Catmull-Rom spline interpolation.
};

/*
	Catmull-Rom spline variant
	selector.
*/
UENUM()
enum class ECRSplineVariant : uint8
{
	Uniform,
	Centripetal,
	Chordal
};

/*
	The state of the NavMeshPath we are building.
	With this, we keep track of the time sliced building process state.

*/
UENUM()
enum class ENavSmootherState : uint8
{
	UnInitialized,
	Initialized,
	InitialTurn,
	SplineSmoothing,
	Finished
};

/*
	Structure for storing the state of the
	path and current settings for the build process.
*/
USTRUCT()
struct FNavSmootherPathData
{
	GENERATED_BODY()

	//General Data
	ENavSmootherState State = ENavSmootherState::UnInitialized;
	TWeakObjectPtr<AActor> NavAgent = nullptr;

	TArray<FNavigationPortalEdge> PortalEdges;
	TArray<FVector> StringPulledPath;
	TArray<FVector> BasePath;
	int32 NextBPIdx = -1;
	
	TArray<FVector> ProcessedPath;
	TArray<FVector> SubSegmentCache;
	
	//Initial Turn State Data
	float CurrentTurnAngle = 0.0f;
	float TurnAngleDone = 0.0f;
	int32 SidesChecked = 1;

	//CRSplineSmoothing Data
	bool SmoothedInitialTurnEnd = false;
	float CurrentTension = 0.0f;
	float CRVariant = 0.0f;
};

/*
	Structure for user settings in the details panel.
	With this the user can fine-tune the pathfind.
*/
USTRUCT()
struct FNavSmootherPathSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Base Settings", meta = (ClampMin = "1"))
	int32 OperationCyclesPerCall = 5;
	UPROPERTY(EditAnywhere, Category = "Base Settings")
	ESmoothingMethod SmoothingMethod;	
	UPROPERTY(EditAnywhere, Category = "Base Settings", meta = (ClampMin = "0"))
	float MaximumDistanceWithoutWaypoint = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Base Settings", meta = (ClampMin = "0.1"))
	float MaximumDistanceAllowedFromOriginalWaypoint = 200.0f;
	UPROPERTY(EditAnywhere, Category = "Base Settings", meta = (ClampMin = "0"))
	FVector MaximumAdjustExtentFromEndPoint = FVector(1000.0f,1000.0f,500.0f);
	
	//Settings for InitialTurn turn phase.
	UPROPERTY(EditAnywhere, Category = "InitialTurn", meta = (ClampMin = "5"))
	float InitialTurnStepDistance = 25.0f;
	UPROPERTY(EditAnywhere, Category = "InitialTurn", meta = (ClampMin = "5"))
	float InitialTurnMinimumAngle = 10.0f;
	UPROPERTY(EditAnywhere, Category = "InitialTurn", meta = (ClampMin = "1", ClampMax = "90"))
	float InitialTurnMaximumAngle = 35.0f;
	UPROPERTY(EditAnywhere, Category = "InitialTurn", meta = (ClampMin = "5"))
	float InitialTurnAngleStep = 5.0f;
	UPROPERTY(EditAnywhere, Category = "InitialTurn", meta = (ClampMax = "90"))
	float InitialTurnArrivalAngleMaximum = 90.0f;

	//Settings for the CRSplineSmoothing phase.
	UPROPERTY(EditAnywhere, Category = "CRSplineSmoothing", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseTension = 0.0f;
	UPROPERTY(EditAnywhere, Category = "CRSplineSmoothing", meta = (ClampMin = "0", ClampMax = "1"))
	float TensionIncPerFailure = 0.2f;
	UPROPERTY(EditAnywhere, Category = "CRSplineSmoothing")
	ECRSplineVariant CRSplineVariant = ECRSplineVariant::Centripetal;	
};

/*
	A static class, that holds the core functions for the NavigationMesh based
	pathfinding and the smoothing system.
*/
class NAVSMOOTHER_API UNavSmootherCore
{
public:
	static TOptional<FVector> ProjectPointToNavigation(const AActor* WorldContextObject, const FVector& Point, const FVector& AgentExtent,
		const FVector& AdjustExtent = FVector::ZeroVector);
	static FPathFindingResult FindPathSync(const AActor* NavAgent, const FVector& StartPoint, const FVector& EndPoint, const FVector& AgentExtent,
		const FVector& AdjustExtent = FVector::ZeroVector);

	static TArray<FNavigationPortalEdge> GetPortalEdgesCrossed(const FNavPathSharedPtr& Path);
	static bool NavPathRaycast(AActor* WorldContextObject, const FVector& StartPoint, const FVector& EndPoint, FVector* OutHitLocation = nullptr);
	
	static void InitializeNavMeshPathData(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings, const TArray<FVector>& StringPulledPath,
		const TArray<FNavigationPortalEdge>& PortalEdges, AActor* NavAgent);
	static void IteratePathfind(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings);
	
	static void CreateBasePath(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings);
	static void ApplyInitialTurn(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings);
	static void ApplySplineSmoothing(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings);
	
	static float GetTurnRequiredToTargetPoint(const FVector& ForwardVector, const FVector& OriginPoint, const FVector& TargetPoint);
	static int32 GetNextWaypointBasedOnLOS(const FVector& StartPoint, FNavSmootherPathData& Data);	
};



