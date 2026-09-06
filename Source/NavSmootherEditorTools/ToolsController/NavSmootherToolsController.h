#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "NavSmoother/Core/NavSmootherCore.h"
#include "NavSmootherToolsController.generated.h"

/*
	Settings for customizing the debug draw for the path.
	This is part of ToolSettings. But since this section is
	reused, it's extracted.
*/
USTRUCT()
struct FNavSmootherDrawSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere)
	bool bShowData = true;	
	
	UPROPERTY(EditAnywhere)
	float WaypointsZOffset = 5.0f;
	UPROPERTY(EditAnywhere)
	FColor LineColor = FColor::Blue;
	UPROPERTY(EditAnywhere)
	bool bShowWaypoints;
	UPROPERTY(EditAnywhere)
	FColor WaypointColor = FColor::Yellow;
};

/*
	Main object we use to show a details panel for our custom Tools Tab!
	This holds the bulk of the settings data.
*/
UCLASS()
class UNavSmootherToolsSettings : public UObject
{
    GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FVector TestPositionStart = FVector(0.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FVector TestPositionEnd = FVector(1000.0f, 0.0f, 0.0f);
	
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FRotator TestPositionStartRotation = FRotator::ZeroRotator;	
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FVector TestActorExtent = FVector(30.0f, 30.0f, 100.0f);
	
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FNavSmootherPathSettings PathSettings;
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FNavSmootherDrawSettings SmoothedPathDrawData;
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FNavSmootherDrawSettings BasePathDrawData;
	UPROPERTY(EditAnywhere, Category = "Tools Settings")
	FNavSmootherDrawSettings StringPulledPathDrawData;

};

/*
	The modes how the user interacts with the tool.
	- Auto:   The path will be calculated every x seconds.
	- Manual: The path will be calculated on a button press.
*/
UENUM()
enum class ENavCalculationMode : uint8
{
    Auto	UMETA(DisplayName="Auto Calculate"),
    Manual	UMETA(DisplayName="Manual Calculate"),
	Count UMETA(Hidden)
};
ENUM_RANGE_BY_COUNT(ENavCalculationMode, ENavCalculationMode::Count); //So we can iterate on the enum nicely.

/*
	The special transient non-saved actor that is used by the NavSmootherComponentVisualizer
	to display the user movable start & end points for the path testing. And also this is needed
	as a NavAgent for the path calculation!
*/
UCLASS()
class NAVSMOOTHEREDITORTOOLS_API ANavSmootherVisProxyActor : public AActor
{
	GENERATED_BODY()
public:
	ANavSmootherVisProxyActor();
	void InitializeTransientActor(class UNavSmootherToolsController* Controller);
	UNavSmootherToolsController* GetController() const;
	
	virtual void PostEditMove(bool bFinished) override;
	
private:
	//It's not this object's authority to affect the controller's lifetime.
	TWeakObjectPtr<UNavSmootherToolsController> m_Controller;
};

/*
	The main controller for the NavSmoother Testing Tool.
	This follows the Model-View-Controller philosophy.
	
	Once caveat is that for science(!) I've moved a few properties into the controller from the UNavSmootherToolsSettings, 
	like m_CurrentMode and m_bShowStats, and m_TimeBetweenSimulateCalls just so we can see, how it is to build your own property widgets and not have 
	them handled by m_DetailsView = PropertyModule.CreateDetailView(Args); automatically.
*/
UCLASS()
class NAVSMOOTHEREDITORTOOLS_API UNavSmootherToolsController : public UObject, public FTickableEditorObject
{
	GENERATED_BODY()
public:
	virtual void PostInitProperties() override;
	
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	
	void ToggleTicking(bool bEnable);
	bool IsTicking() const;
	
	UNavSmootherToolsSettings* GetSettings();
	
	ENavCalculationMode GetMode() const;
	void SetMode(ENavCalculationMode Mode);
	
	bool GetShowStats() const;
	void SetShowStats(bool ShowData);	
	
	float GetSimulationTimer() const;
	void SetSimulationTimer(float Time);
	
	void SpawnVisProxyActor();
	
	void StartSimulation();
	void StopSimulation();
	
	void SelectVisProxyActor();
	void AddTranslationDeltaForVisProxyActor(FVector PositionDelta, FRotator RotationDelta);
	void SetTranslationForVisProxyActor(FVector Position, FRotator Rotation);
	
private:
	void InitializeTestPath();
	void IterateOnPathCalculation();
	
	void DebugDrawPath(FNavSmootherDrawSettings& DrawSettings, const TArray<FVector>& Path, float DrawDuration);
	
	UPROPERTY()
	TObjectPtr<UNavSmootherToolsSettings> m_ToolsSettings = nullptr;
	
	ENavCalculationMode m_CurrentMode = ENavCalculationMode::Auto;
	bool m_bShowStats = false;
	bool m_bIsTicking = false;
	
	float m_SimulationTimer = 0.3f;
	float m_TimeRemainTillNextSimulation = 0.0f;
	
	FNavSmootherPathData m_PathData;
	
	UPROPERTY()
	TObjectPtr<ANavSmootherVisProxyActor> m_VisProxyActor = nullptr;
};
