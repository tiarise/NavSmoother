#include "NavSmootherToolsController.h"
#include "Selection.h"

/*
	########################################################################################
	#                      ANavSmootherControllerTransientActor                            #
	########################################################################################
*/

ANavSmootherVisProxyActor::ANavSmootherVisProxyActor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ANavSmootherVisProxyActor::InitializeTransientActor(class UNavSmootherToolsController* Controller)
{
	m_Controller = Controller;
}

UNavSmootherToolsController* ANavSmootherVisProxyActor::GetController() const
{
	return m_Controller.IsValid() ? m_Controller.Get() : nullptr;
}

void ANavSmootherVisProxyActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	
	/*
		Make sure we update the Setting's start position and rotation.
		The end position update is handled by the FNavSmootherComponentVisualizer::HandleInputDelta.
		So the UI, the VisProxyActor and the ComponentVisualizer is always in sync.
	*/
	if ( m_Controller.IsValid() )
	{
		m_Controller->GetSettings()->TestPositionStart = GetActorLocation();
		m_Controller->GetSettings()->TestPositionStartRotation = GetActorRotation();
	}
}

/*
	########################################################################################
	#                          UNavSmootherToolsController                                 #
	########################################################################################
*/

void UNavSmootherToolsController::PostInitProperties()
{
	Super::PostInitProperties();
	
	GetSettings()->StringPulledPathDrawData.LineColor = FColor::Red;
	GetSettings()->StringPulledPathDrawData.bShowData = false;
	
	GetSettings()->BasePathDrawData.LineColor = FColor::Orange;
	GetSettings()->BasePathDrawData.bShowData = false;
	
	GetSettings()->SmoothedPathDrawData.LineColor = FColor::Blue;
}

void UNavSmootherToolsController::Tick(float DeltaTime)
{
	if ( m_PathData.State == ENavSmootherState::UnInitialized )
	{
		//Wait for the simulation timer to reach zero!
		m_TimeRemainTillNextSimulation = FMath::Max(m_TimeRemainTillNextSimulation-DeltaTime,0.0f);
		if ( m_TimeRemainTillNextSimulation == 0.0 )
			InitializeTestPath(); //We need to initialize the test path. We don't do anything else in this frame.
	}
	else if ( m_PathData.State == ENavSmootherState::Finished )
	{
		/*
			If the path find and smoothing process has finished, we do the draw,
			then resetting the PathDataState.
		*/
		float CalculatedDrawTime = GetSimulationTimer();
		if ( GetMode() == ENavCalculationMode::Auto )
			m_TimeRemainTillNextSimulation = CalculatedDrawTime - 0.1f; //Set the timer till the next recalculation a bit less than the draw!
		else
			ToggleTicking(false); //Since we are on Manual mode there won't be an automatic new simulation.
		
		//Draw the requested paths, so the user can see the results.
		if ( GetSettings()->StringPulledPathDrawData.bShowData )
			DebugDrawPath(GetSettings()->StringPulledPathDrawData,m_PathData.StringPulledPath,CalculatedDrawTime);
		if ( GetSettings()->BasePathDrawData.bShowData )
			DebugDrawPath(GetSettings()->BasePathDrawData,m_PathData.BasePath,CalculatedDrawTime);
		if ( GetSettings()->SmoothedPathDrawData.bShowData )
			DebugDrawPath(GetSettings()->SmoothedPathDrawData,m_PathData.ProcessedPath,CalculatedDrawTime);
				
		m_PathData.State = ENavSmootherState::UnInitialized; //Resetting state for new calculation.		
	}
	else
		IterateOnPathCalculation();
}

bool UNavSmootherToolsController::IsTickable() const
{
	return m_bIsTicking;
}

TStatId UNavSmootherToolsController::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UNavSmootherToolsController, STATGROUP_Tickables);
}

void UNavSmootherToolsController::ToggleTicking(bool bEnable)
{
	m_bIsTicking = bEnable;
}

bool UNavSmootherToolsController::IsTicking() const
{
	return m_bIsTicking;
}

UNavSmootherToolsSettings* UNavSmootherToolsController::GetSettings()
{
	if ( m_ToolsSettings )
		return m_ToolsSettings;
		
	m_ToolsSettings = NewObject<UNavSmootherToolsSettings>();
	return m_ToolsSettings;
}

ENavCalculationMode UNavSmootherToolsController::GetMode() const
{
	return m_CurrentMode;
}

void UNavSmootherToolsController::SetMode(ENavCalculationMode Mode)
{
	m_CurrentMode = Mode;
}

bool UNavSmootherToolsController::GetShowStats() const
{
	return m_bShowStats;
}

void UNavSmootherToolsController::SetShowStats(bool ShowData)
{
	m_bShowStats = ShowData;
	GEditor->Exec(nullptr, TEXT("stat NavSmoother")); //Stat declared in NavSmootherCore.cpp
}

float UNavSmootherToolsController::GetSimulationTimer() const
{
	return m_SimulationTimer;
}

void UNavSmootherToolsController::SetSimulationTimer(float Time)
{
	m_SimulationTimer = Time;
}

void UNavSmootherToolsController::SpawnVisProxyActor()
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if ( !World )
		return;
	
	/*
		We will create an actor that only lives in the editor, never saved into the level.
		Never dirties the level and doesn't appear in the outliner. These are just for the
		Tool while it's active. This is used by the FNavSmootherComponentVisualizer.
	*/
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	if ( !m_VisProxyActor )
	{
		m_VisProxyActor = World->SpawnActor<ANavSmootherVisProxyActor>(ANavSmootherVisProxyActor::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator, Params);
		m_VisProxyActor->bIsEditorOnlyActor = true; //Never cooks into the game.
		m_VisProxyActor->InitializeTransientActor(this);
	}
}

void UNavSmootherToolsController::StartSimulation()
{
	SelectVisProxyActor();
	ToggleTicking(true);
}

void UNavSmootherToolsController::StopSimulation()
{
	if ( GetMode() != ENavCalculationMode::Auto )
		return;
	
	//Clean selected actors
	GEditor->SelectNone(false,true,false);
	
	//Turn off ticking if on auto mode.
	if ( GetMode() == ENavCalculationMode::Auto )
		ToggleTicking(false);
	
	//Clear Path Data
	m_PathData.State = ENavSmootherState::UnInitialized;
}

void UNavSmootherToolsController::SelectVisProxyActor()
{
	if ( !GEditor )
		return;	
	
	//Spawn the VisProxyActor if not present.
	if ( !m_VisProxyActor )
	{
		SpawnVisProxyActor();
	}
	
	//If already selected, no need to select again.
	USelection* Selection = GEditor->GetSelectedActors();
	if ( Selection->IsSelected(m_VisProxyActor) )
		return;
	
	//Clean selected actors
	GEditor->SelectNone(false,true,false);
	
	//Select the VisProxy actor so the user will be able to visualize the path.
	GEditor->SelectActor(m_VisProxyActor, true, true, true);
}

void UNavSmootherToolsController::AddTranslationDeltaForVisProxyActor(FVector PositionDelta, FRotator RotationDelta)
{
	if ( !m_VisProxyActor )
		return;
	
	m_VisProxyActor->SetActorLocation(m_VisProxyActor->GetActorLocation() + PositionDelta);
	m_VisProxyActor->SetActorRotation(m_VisProxyActor->GetActorRotation() + RotationDelta);
}

void UNavSmootherToolsController::SetTranslationForVisProxyActor(FVector Position, FRotator Rotation)
{
	if ( !m_VisProxyActor )
		return;
	
	m_VisProxyActor->SetActorLocation(Position);
	m_VisProxyActor->SetActorRotation(Rotation);	
}

void UNavSmootherToolsController::InitializeTestPath()
{
	/*
		This function initializes the string pulled path. We are using the UNavigationSystemV1::FindPathSync() underneath.
		At some point we should create the async version too! Our path smoothing solution is time sliced by us so that will not be
		heavy on the game thread.
	*/
	
	//We are using FindPathSync underneath, at some point we could do the Async version!
	const FPathFindingResult Result = UNavSmootherCore::FindPathSync(m_VisProxyActor,GetSettings()->TestPositionStart,GetSettings()->TestPositionEnd,
		GetSettings()->TestActorExtent,GetSettings()->PathSettings.MaximumAdjustExtentFromEndPoint);
	
	if ( Result.Result == ENavigationQueryResult::Success )
	{
		//Convert string pulled path to Vector array.
		TArray<FVector> StringPulledPath;
		for ( const FNavPathPoint& NavPathPoint : Result.Path->GetPathPoints() )
			StringPulledPath.Add(NavPathPoint.Location);

		//Initialize the PathData.
		UNavSmootherCore::InitializeNavMeshPathData(m_PathData,GetSettings()->PathSettings,StringPulledPath,
			UNavSmootherCore::GetPortalEdgesCrossed(Result.Path),m_VisProxyActor);
	}		
}

void UNavSmootherToolsController::IterateOnPathCalculation()
{
	if ( m_PathData.State == ENavSmootherState::UnInitialized )
		return; //Likely InitializeTestPath() was not called.
		
	/*
		We iterate on the current pathfinding state.
		In each tick the pathfind cycles are limited(look for OperationCyclesPerCall),
		so we won't overwhelm the game thread.
	*/
	UNavSmootherCore::IteratePathfind(m_PathData,GetSettings()->PathSettings);
}

void UNavSmootherToolsController::DebugDrawPath(FNavSmootherDrawSettings& DrawSettings, const TArray<FVector>& Path, float DrawDuration)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if ( !World || Path.Num() == 0 )
		return;	
	
	for ( int i = 0; i < Path.Num(); ++i )
	{
		if ( DrawSettings.bShowWaypoints )
		{
			DrawDebugSphere(World,Path[i] + FVector::UpVector*DrawSettings.WaypointsZOffset,
				10.0f, 16,DrawSettings.WaypointColor,false,DrawDuration);
		}
		
		if ( i < Path.Num()-1 )
		{
			DrawDebugLine(World,Path[i] + FVector::UpVector*DrawSettings.WaypointsZOffset,Path[i+1]
				+ FVector::UpVector*DrawSettings.WaypointsZOffset, DrawSettings.LineColor,false,
				DrawDuration,0,10.0f);
		}
	}	
}
