#include "NavSmootherCore.h"
#include "NavigationSystem.h"
#include "NXCoreStatics/Math/MathCoreStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavMesh/NavMeshPath.h"

DECLARE_STATS_GROUP(TEXT("NavSmoother"), STATGROUP_NavSmoother, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Create Base Path"), STAT_CreateBasePath, STATGROUP_NavSmoother);
DECLARE_CYCLE_STAT(TEXT("Apply Initial Turn"), STAT_ApplyInitialTurn, STATGROUP_NavSmoother);
DECLARE_CYCLE_STAT(TEXT("Apply Spline Smoothing"), STAT_ApplySplineSmoothing, STATGROUP_NavSmoother);

TOptional<FVector> UNavSmootherCore::ProjectPointToNavigation(const AActor* WorldContextObject, const FVector& Point, const FVector& AgentExtent,
	const FVector& AdjustExtent)
{
	TOptional<FVector> ReturnValue;
	if ( !IsValid(WorldContextObject) || !IsValid(WorldContextObject->GetWorld()) )
		return ReturnValue;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(WorldContextObject->GetWorld());
	if ( NavSystem == nullptr )
	    return ReturnValue;

	FNavLocation ProjectedPoint;
	FNavAgentProperties AgentProperties;
	AgentProperties.AgentRadius = FMath::Max(AgentExtent.X,AgentExtent.Y);
	AgentProperties.AgentHeight = AgentExtent.Z;
	if ( NavSystem->ProjectPointToNavigation(Point,ProjectedPoint,AdjustExtent,&AgentProperties) )
		ReturnValue = ProjectedPoint.Location;

	return ReturnValue; 
}

FPathFindingResult UNavSmootherCore::FindPathSync(const AActor* NavAgent, const FVector& StartPoint, const FVector& EndPoint, const FVector& AgentExtent,
	const FVector& AdjustExtent)
{
	FPathFindingResult Result;
	
	if ( !IsValid(NavAgent) ) //Here we have NavAgent and not WorldContextObject on purpose!
		return Result;
	
	//We won't allow start position adjust! So the AdjustExtent is zero.
	const TOptional<FVector> ProjectedStartPoint = ProjectPointToNavigation(NavAgent,StartPoint,AgentExtent,FVector::ZeroVector);
	if ( !ProjectedStartPoint.IsSet() )
		return Result;
	
	const TOptional<FVector> ProjectedEndPoint = ProjectPointToNavigation(NavAgent,EndPoint,AgentExtent,AdjustExtent);
	if ( !ProjectedEndPoint.IsSet() )
		return Result;
	
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(NavAgent->GetWorld());
	if ( NavSystem == nullptr )
		return Result;

	ANavigationData* NavData = nullptr;
	if ( const INavAgentInterface* NavAgentInterface = Cast<INavAgentInterface>(NavAgent) )
	{
		const FNavAgentProperties& AgentProps = NavAgentInterface->GetNavAgentPropertiesRef();
		NavData = NavSystem->GetNavDataForProps(AgentProps,NavAgent->GetActorLocation());
	}
	else
		NavData = NavSystem->GetDefaultNavDataInstance();

	if ( !NavData )
		return Result;
	
	FPathFindingQuery Query(NavAgent, *NavData, ProjectedStartPoint.GetValue(), ProjectedEndPoint.GetValue());
	return NavSystem->FindPathSync(Query,EPathFindingMode::Hierarchical);	
}

TArray<FNavigationPortalEdge> UNavSmootherCore::GetPortalEdgesCrossed(const FNavPathSharedPtr& Path)
{
	TArray<FNavigationPortalEdge> PortalEdges;
	if ( !ensure(Path->IsValid()) )
		return PortalEdges;

	FNavMeshPath* NavMeshPath = static_cast<FNavMeshPath*>(Path.Get());
	if ( !ensure(NavMeshPath) )
		return PortalEdges;

	return NavMeshPath->GetPathCorridorEdges();		
}

bool UNavSmootherCore::NavPathRaycast(AActor* WorldContextObject, const FVector& StartPoint, const FVector& EndPoint, FVector* OutHitLocation)
{
	if ( !IsValid(WorldContextObject) )
		return false;
	
	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(WorldContextObject->GetWorld());
	if ( NavSystem == nullptr )
	    return false;
	
	FVector HitLocation;
	bool  PathIsFree = !NavSystem->NavigationRaycast(WorldContextObject,StartPoint, EndPoint, HitLocation);
	if ( OutHitLocation )
		*OutHitLocation = HitLocation;

	return PathIsFree;	
}

void UNavSmootherCore::InitializeNavMeshPathData(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings, const TArray<FVector>& StringPulledPath,
	const TArray<FNavigationPortalEdge>& PortalEdges, AActor* NavAgent)
{
	//Reset Data
	Data.State = ENavSmootherState::UnInitialized;
	Data.NavAgent = NavAgent;
	Data.StringPulledPath.Empty();
	Data.ProcessedPath.Empty();
	Data.SubSegmentCache.Empty();
	Data.PortalEdges.Empty();
	Data.CurrentTurnAngle = 0.0f; //In EulerAngles.
	Data.TurnAngleDone = 0.0f;    //In EulerAngles.
	Data.SidesChecked = 1;
	Data.SmoothedInitialTurnEnd = false;
	Data.CurrentTension = Settings.BaseTension;
	
	switch ( Settings.CRSplineVariant )
	{
		case ECRSplineVariant::Uniform:
		Data.CRVariant = 0.0f;
		break;
		case ECRSplineVariant::Centripetal:
		Data.CRVariant = 0.5f;	
		break;
		case ECRSplineVariant::Chordal:
		Data.CRVariant = 1.0f;	
		break;
	}
	
	if ( StringPulledPath.Num() < 2 || !IsValid(NavAgent) )
		return;

	Data.NextBPIdx = 1; //Target waypoint index, so the second waypoint. ( This is for the BasePath )
	Data.StringPulledPath = StringPulledPath;
	Data.ProcessedPath.Add(Data.StringPulledPath[0]);
	Data.PortalEdges = PortalEdges;
	Data.State = ENavSmootherState::Initialized;	
}

void UNavSmootherCore::IteratePathfind(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings)
{
	int32 CycleCounter = 0;
	while ( CycleCounter < Settings.OperationCyclesPerCall )
	{
		switch ( Data.State )
		{
			case ENavSmootherState::Initialized:
			{
				CreateBasePath(Data,Settings);
			}
			break;
			case ENavSmootherState::InitialTurn:
			{
				ApplyInitialTurn(Data,Settings);
			}
			break;
			case ENavSmootherState::SplineSmoothing:
			{
				ApplySplineSmoothing(Data,Settings);
			}
			break;
		}
		
		CycleCounter++;
	}	
}

void UNavSmootherCore::CreateBasePath(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(CreateBasePath);
    SCOPE_CYCLE_COUNTER(STAT_CreateBasePath);
	
	//In this case there is no path to work with.
	if ( Data.StringPulledPath.Num() < 2 )
	{
		Data.State = ENavSmootherState::Finished;
		return;
	}

	/*
		The Base path optimisation needs crossed PortalEdges.
		Without them, this phase of optimisation is not possible.
		Go to the next phase.
	*/
	if ( Data.PortalEdges.Num() < 1 )
	{
		Data.BasePath = Data.StringPulledPath;
        Data.State = ENavSmootherState::InitialTurn;
		return;
	}
	
	//Clear the array that will hold the final waypoints.
	Data.BasePath.Empty();
	
	//Prepare the temp array we are going to work with.
	TArray<FVector> TempBasePath;
	TempBasePath.Add(Data.StringPulledPath[0]);
	
	//Check if the last point is on a portal edge, if yes, we ignore the last portal edge.
	bool LastPointPortalEdge = FMath::PointDistToSegment(Data.StringPulledPath.Last(),Data.PortalEdges.Last().Left,
		Data.PortalEdges.Last().Right) == 0.0f;

	/*
		Create an array of the StringPulled waypoints backwards!
		We will remove elements as we encounter them, and removing from the end is more efficient.
		We go from the second to last element to the second. Because we are not interested in the
		start and end points!
	*/
	TOptional<FVector> SPWaypointToCheck;
	TArray<FVector> SPWaypoints;
	for ( int32 i = Data.StringPulledPath.Num()-2; i >= 1; --i )
		SPWaypoints.Add(Data.StringPulledPath[i]);

	/*
		Create the TempBasePath. By default, it consists of each portal edge's middle point.

		Exception A:
		If the last string pulled waypoint is on a portal edge we ignore that last edge.

		Exception B:
		If the PortalEdge has a StringPulled waypoint on it, and the MiddlePoint and the mentioned waypoint
		is over a distance threshold, put the new waypoint on the distance threshold! This way we can avoid
		weird turns if the waypoints are on long portal edges.
	*/
	for ( int32 i = 0; i < Data.PortalEdges.Num(); ++i )
	{
		//Exception A
		if ( i == Data.PortalEdges.Num()-1 && LastPointPortalEdge )
			continue;
		
		/*
			Exception B
			It's important, that you can only remove a point from SPWaypoints when you confirmed that you
			found a new SPWaypoint after it! Because one SPWaypoint may be on multiple portal edges!
			So in here, we check if any of the SPWaypoints are on the PortalEdge currently iterated, and optionally
			remove an SPWaypoint we don't need anymore. With this trick we don't have to iterate on all the SPWaypoints
			all the time just to find what we need.
		*/
		if ( SPWaypoints.Num() > 0 && FMath::PointDistToSegment(SPWaypoints.Last(),Data.PortalEdges[i].Left,
 			Data.PortalEdges[i].Right) == 0.0f )
		{
			SPWaypointToCheck = SPWaypoints.Last();
		}
		else if ( SPWaypoints.Num() > 1 && FMath::PointDistToSegment(SPWaypoints[SPWaypoints.Num()-2],Data.PortalEdges[i].Left,
 			Data.PortalEdges[i].Right) == 0.0f )
		{
			SPWaypoints.RemoveAt(SPWaypoints.Num()-1);
			SPWaypointToCheck = SPWaypoints.Last();
		}
		
		//Add the new waypoint to TempBasePath
		if ( SPWaypointToCheck.IsSet() )
		{
			const FVector VecToMiddle = Data.PortalEdges[i].GetMiddlePoint() - SPWaypointToCheck.GetValue();
			float Distance = VecToMiddle.Size();
 			if ( Distance > Settings.MaximumDistanceAllowedFromOriginalWaypoint )
 				TempBasePath.Add(SPWaypointToCheck.GetValue()+VecToMiddle.GetSafeNormal()*Settings.MaximumDistanceAllowedFromOriginalWaypoint);
		    else
				TempBasePath.Add(Data.PortalEdges[i].GetMiddlePoint());

			SPWaypointToCheck.Reset();
		}
		else
			TempBasePath.Add(Data.PortalEdges[i].GetMiddlePoint());
	}
	
	//Now we add the last point
	TempBasePath.Add(Data.StringPulledPath.Last());
	
	//Add the origin point to valid points.
	Data.BasePath.Add(TempBasePath[0]);
	
	//This is the iterator that we use for LOS checks.
	int32 TempWaypointItr = 0;

	/*
		If requested, the next waypoint will be automatically accepted,
		if that waypoint is further from the last by given distance threshold.
		With this trick, we can avoid long straight sections in the Base path.
		Spline interpolation potentially can be made a bit more curvy with more
		frequent waypoints. More waypoints effectively mean more sampling from
		the path.
	*/
	bool ForceWaypointCreation = false;
	float Distance = 0;
	
	/*
		Create the simplified path based on LOS from the TempBasePath.
		We assume that every direct neighbour portal edge center point can see each other.
		This is how Navigation Meshes are built! Each polygon on the NavMesh is a convex polygon!
	*/
	while ( true )
	{
		for ( int32 i = TempWaypointItr+1; i < TempBasePath.Num(); ++i )
		{
			ForceWaypointCreation = false;
			Distance = (TempBasePath[i] - TempBasePath[TempWaypointItr]).Size();
			
			/*
				If next point is over a given distance, we force the waypoint addition.
				This way we won't lose too much precision for spline interpolation.
				The more point we have the nicer the interpolation will be.
				Important(!): We only apply this if the two waypoint we check are not neighbouring waypoints in the list!
			*/
			if ( Distance >= Settings.MaximumDistanceWithoutWaypoint && TempWaypointItr != i-1 )
				ForceWaypointCreation = true;

			//Check if we can see the currently iterated point from the last valid waypoint.
			if ( !NavPathRaycast(Data.NavAgent.Get(),TempBasePath[TempWaypointItr],TempBasePath[i]) || ForceWaypointCreation )
			{
				//Add the path point to the valid points that we still see from the last valid waypoint.
				Data.BasePath.Add(TempBasePath[i-1]);
				
				//This waypoint will be the new point we check the LOS from.
				TempWaypointItr = i-1;
			}
			
			//If the iteration arrived at the end of the path and sees it, set the TempWaypointItr to the second last, so it will exit below.
			else if ( i == TempBasePath.Num()-1 )
				TempWaypointItr = i-1;
		}

		/*
			If the iteration is on the second last element we can safely add the last waypoint.
			We can end the iteration here. 	
		*/
		if ( TempWaypointItr == TempBasePath.Num() - 2 )
		{
			Data.BasePath.Add(TempBasePath.Last());
			break;
		}
	}
	
	Data.State = ENavSmootherState::InitialTurn;	
}

void UNavSmootherCore::ApplyInitialTurn(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ApplyInitialTurn);
    SCOPE_CYCLE_COUNTER(STAT_ApplyInitialTurn);	
	
	/*
		If the NavAgent is not valid the pathfind has encountered a serious error, reset state and return.
		We can be sure that the Start and Target points are valid, as InitializeNavMeshPathData made sure that the
		BasePath has at least two elements.
	*/
	if ( !Data.NavAgent.IsValid() )
	{
		Data.State = ENavSmootherState::UnInitialized;
		return;
	}
	
	//Gather data for segment start and target points ( This is part of the small subsegments that will add up as a turning path for the NavAgent. )
	const FVector NewSegmentStart = Data.SubSegmentCache.Num() == 0 ? Data.ProcessedPath.Last() : Data.SubSegmentCache.Last();
	const TArray<FVector>& CH = Data.SubSegmentCache;
	FVector CurrentDirection = FVector::ZeroVector;
	if ( CH.Num() == 0 )
		CurrentDirection = Data.NavAgent->GetActorForwardVector();
	else if ( CH.Num() == 1 )
		CurrentDirection = (CH[CH.Num()-1] - Data.ProcessedPath.Last()).GetSafeNormal();
	else //If there are 2 or more	
		CurrentDirection = (CH[CH.Num()-1] - CH[CH.Num()-2]).GetSafeNormal();

	int32 TargetWayPIdx = GetNextWaypointBasedOnLOS(NewSegmentStart,Data);
	if ( TargetWayPIdx == -1 )
		TargetWayPIdx = Data.NextBPIdx; //Sometimes during turning we might not even see the next waypoint, but that's not an issue! In that case default to the next waypoint!

	FVector TargetPoint = Data.BasePath[TargetWayPIdx];
	
	//Check if this is the first turn iteration. If so, we need some basic setup.
	if ( Data.CurrentTurnAngle == 0.0f ) 
	{
		//Calculate the turn angle needed to target and set the turn angle to the MinimumTurn angle, so at first, we will try to turn with maximum radius.
		const float RequiredTurnToTarget = GetTurnRequiredToTargetPoint(CurrentDirection,NewSegmentStart,TargetPoint);
		int32 CurrentTurnDir = FMath::Sign(RequiredTurnToTarget);
		Data.CurrentTurnAngle = CurrentTurnDir*Settings.InitialTurnMinimumAngle;
	
		/*
			Check if the NavActor's forward vector is within acceptable range to the Waypoint we are trying to turn to.
			If yes, then there is no need for this stage of the smoothing, as the actor can turn to target in one step.
		*/
		if ( FMath::Abs(RequiredTurnToTarget) < Settings.InitialTurnMinimumAngle )
		{
			Data.SubSegmentCache.Add(Data.BasePath[TargetWayPIdx]);
			Data.ProcessedPath.Append(Data.SubSegmentCache);
			Data.SubSegmentCache.Empty();
			Data.NextBPIdx = TargetWayPIdx+1;
			Data.CurrentTurnAngle = 0.0f;
			Data.TurnAngleDone = 0.0f;
			Data.SidesChecked = 1;
			Data.State = ENavSmootherState::SplineSmoothing;
			return;
		}
	}
	
	//Calculate new subsegment endpoint.
	bool TurnFailure = false;
	FVector NewSegmentEnd = NewSegmentStart + Settings.InitialTurnStepDistance * CurrentDirection.RotateAngleAxis(Data.CurrentTurnAngle,FVector::UpVector);
	Data.TurnAngleDone += FMath::Abs(Data.CurrentTurnAngle);
	
	/*
		Failsafe for rare situations if the algorithm turns 360 degrees and can't find a solution.
		Should not happen, but never say never, so here it is.
	*/
	if ( Data.TurnAngleDone >= 360.0f )
		TurnFailure = true;
	
	/*
		Check if the new segment is valid as a path, meaning there is no blocker in the path, or not going out of the NavMesh.
	*/
	if ( !TurnFailure && NavPathRaycast(Data.NavAgent.Get(),NewSegmentStart,NewSegmentEnd) )
	{
		/*
			Check all string pulled waypoints and segments up to TargetWayPIdx+1 (The first waypoint we can't see).
			If we can reach the furthest waypoint within the current turn angle, then connect to the given waypoint. If that failed, try to we elongate the current segment, and
			we might have an intersection with that given string pulled segment, if so connect to that if the angle is acceptable.
		*/
		int32 CurrentIndex = Data.BasePath.IsValidIndex(TargetWayPIdx+1) ? TargetWayPIdx+1 : TargetWayPIdx;
		while ( CurrentIndex >= Data.NextBPIdx )
		{
			/*
				Check if we can connect to a visible string pulled waypoint.
				"CurrentIndex != TargetWayPIdx+1" means that that is the waypoint after the last visible waypoint, we start from there if
				possible since even if we can't see that waypoint, we may see a portion of a segment leading there, and then we can
				connect to that at the given intersection.
			*/
			if ( CurrentIndex != TargetWayPIdx+1 )
			{
				//Get the angle between the new segment's dir and the dir to the waypoint we wish to reach.
				FVector DirToWaypoint = (Data.BasePath[TargetWayPIdx]-NewSegmentStart).GetSafeNormal();
				FVector NewSegmentDir = (NewSegmentEnd-NewSegmentStart).GetSafeNormal();
				float Angle = UMathCoreStatics::GetAngleBetweenTwoVectors(DirToWaypoint,NewSegmentDir);

				if ( Angle < FMath::Abs(Data.CurrentTurnAngle) )
				{
					//Check if we can do a raycast from the NewSegmentStart to the Waypoint.
					if ( NavPathRaycast(Data.NavAgent.Get(),NewSegmentStart,Data.BasePath[TargetWayPIdx]) )
					{
						//This means we can directly go to the waypoint from this subsegment, this part of the algorithm is complete!
						Data.SubSegmentCache.Add(Data.BasePath[TargetWayPIdx]);
						Data.ProcessedPath.Append(Data.SubSegmentCache);
						Data.SubSegmentCache.Empty();
						Data.NextBPIdx = CurrentIndex+1;
						Data.CurrentTurnAngle = 0.0f;
						Data.TurnAngleDone = 0.0f;
						Data.SidesChecked = 1;
						Data.State = ENavSmootherState::SplineSmoothing;
						return;
					}
				}				
			}
			
			//Check if the segment elongated as a straight line intersects with the string pulled path we are checking.
			float DistanceToWaypoint = (Data.BasePath[CurrentIndex]-NewSegmentEnd).Size()*2.0f;
			FVector Intersection = FVector::ZeroVector;
			bool Match = FMath::SegmentIntersection2D(Data.BasePath[CurrentIndex],
				Data.BasePath[CurrentIndex-1],NewSegmentStart,
				NewSegmentStart+(NewSegmentEnd-NewSegmentStart).GetSafeNormal()*DistanceToWaypoint,Intersection);
			
			/*
				Check if there is a match and there is at least one segment already created.
				The second part is important, because the first segment created always intersects with the string pulled path's first segment,
				as they have the same origin point.
			*/
			if ( Match && Data.SubSegmentCache.Num() > 1 )
			{
				FVector NewSegmentDir = (NewSegmentEnd-NewSegmentStart).GetSafeNormal();
				
				//Check if the intersection is reachable with raycast from the NewSegmentEnd. 
				if ( NavPathRaycast(Data.NavAgent.Get(),NewSegmentEnd,Intersection) )
				{
					//Check if the segment's dir elongated as an imaginary line arrives at an acceptable angle to the intersection.
					FVector WaypointSegmentDir = (Data.BasePath[CurrentIndex-1]-Intersection).GetSafeNormal();
					NewSegmentDir = (NewSegmentStart-Intersection).GetSafeNormal();
					float AngleOne = UMathCoreStatics::GetAngleBetweenTwoVectors(WaypointSegmentDir,NewSegmentDir);
					
					//Check if the angle in which the segment arrives is within the acceptable range.
					if ( AngleOne <= Settings.InitialTurnArrivalAngleMaximum )
					{
						/*
							This means we can directly go to the intersection, this part of the algorithm is complete!
							Do not remove StringPulled[] here! We did not reach it!
						*/
						Data.SubSegmentCache.Add(Intersection);
						Data.ProcessedPath.Append(Data.SubSegmentCache);
						Data.SubSegmentCache.Empty();
						Data.CurrentTurnAngle = 0.0f;
						Data.TurnAngleDone = 0.0f;
						Data.SidesChecked = 1;
						Data.NextBPIdx = CurrentIndex;
						Data.State = ENavSmootherState::SplineSmoothing;
						return;
					}
				}
			}

			CurrentIndex--;
		}
		Data.SubSegmentCache.Add(NewSegmentEnd);
	}
	else
		TurnFailure = true; //This means there was a blocker during raycast.

	if ( TurnFailure )
	{
		if ( FMath::Abs(Data.CurrentTurnAngle) == Settings.InitialTurnMaximumAngle )
		{
			if ( Data.SidesChecked == 2 )
			{
				//The initial turn smoothing can't be done, with supplied parameters!
				Data.SubSegmentCache.Empty();
				Data.TurnAngleDone = 0.0f;
				Data.State = ENavSmootherState::UnInitialized;
				return;
			}
			
			//Try to turn to the other direction with the path if the first side failed.
			Data.SidesChecked = 2;
			Data.CurrentTurnAngle = FMath::Sign(Data.CurrentTurnAngle)*-1*Settings.InitialTurnMinimumAngle;
			Data.SubSegmentCache.Empty();
			Data.TurnAngleDone = 0.0f;
			return;
		}
		
		//Increase the turn rate and re-try on the same side.
		float AlteredTurnAngle = FMath::Min(FMath::Abs(Data.CurrentTurnAngle)+Settings.InitialTurnAngleStep,Settings.InitialTurnMaximumAngle);
		Data.CurrentTurnAngle = FMath::Sign(Data.CurrentTurnAngle)*AlteredTurnAngle;
		Data.SubSegmentCache.Empty();
		Data.TurnAngleDone = 0.0f;
	}	
}

void UNavSmootherCore::ApplySplineSmoothing(FNavSmootherPathData& Data, const FNavSmootherPathSettings& Settings)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ApplySplineSmoothing);
    SCOPE_CYCLE_COUNTER(STAT_ApplySplineSmoothing);		
	
	if ( !Data.BasePath.IsValidIndex(Data.NextBPIdx) && Data.ProcessedPath.Num() > 1 )
	{
		//This means that the initial turn could finish connecting directly into the end point.
		Data.State = ENavSmootherState::Finished;
		return;
	}

	int32 PPSize = Data.ProcessedPath.Num();
	TArray<FVector> RelevantPoints;
	
	if ( !Data.SmoothedInitialTurnEnd )
	{
		/*
			Case 1:
			Smoothing for the initial turn end.
			If the processed path has to have at lest 3 elements we can use those, if it has only two elements
			we can create one artificial third element, so we can use smoothing.
		*/
		if ( Data.ProcessedPath.Num() > 2 )
			RelevantPoints.Add(Data.ProcessedPath[PPSize-3]);
		else
		{
			FVector AdditionalPoint = Data.ProcessedPath[PPSize-2] + (Data.NavAgent->GetActorForwardVector()*-1);
			RelevantPoints.Add(AdditionalPoint);			
		}
		
		RelevantPoints.Add(Data.ProcessedPath[PPSize-2]);
		RelevantPoints.Add(Data.ProcessedPath[PPSize-1]);
		RelevantPoints.Add(Data.BasePath[Data.NextBPIdx]);
	}
	else
	{
		/*
			Case 2:
			Smoothing for the last waypoint.
			The processed path has to have at lest 2 elements and the next two NextBPIdx has to be valid.
		*/
		if ( Data.NextBPIdx == Data.BasePath.Num()-1 ) 
		{
			RelevantPoints.Add(Data.ProcessedPath[PPSize-2]);
			RelevantPoints.Add(Data.ProcessedPath[PPSize-1]);
			RelevantPoints.Add(Data.BasePath[Data.NextBPIdx]);

			FVector AdditionalPoint = Data.BasePath[Data.NextBPIdx]+(Data.BasePath[Data.NextBPIdx] - Data.BasePath[Data.NextBPIdx-1]).GetSafeNormal();
			RelevantPoints.Add(AdditionalPoint);
		}
		
		/*
			Case 3:
			Smoothing for the points after the initial turn but before the last waypoint.
			The processed path has to have at lest 2 elements and the next two NextBPIdx have to be valid.
		*/
		else if ( Data.BasePath.IsValidIndex(Data.NextBPIdx+1) ) 
		{
			RelevantPoints.Add(Data.ProcessedPath[PPSize-2]);
			RelevantPoints.Add(Data.ProcessedPath[PPSize-1]);
			RelevantPoints.Add(Data.BasePath[Data.NextBPIdx]);
			RelevantPoints.Add(Data.BasePath[Data.NextBPIdx+1]);
		}		
	}
	
	TArray<FVector> SplinePoints;
	for ( int32 i = 1; i <= 10; ++i ) //The first point is already in the Processed path!
	{
		FVector SplinePoint = UMathCoreStatics::CRSplineInterpolation(RelevantPoints[0],RelevantPoints[1],RelevantPoints[2],RelevantPoints[3],
			0.1f*i,Data.CRVariant,Data.CurrentTension);
		SplinePoints.Add(SplinePoint);
	}

	/*
		Make sure that the created spline is Raycast correct on the navmesh.
		If not retry in the next iteration with a bit stricter tension.
	*/
	for ( int32 i = 0; i < SplinePoints.Num()-2; ++i )
	{
		if ( !NavPathRaycast(Data.NavAgent.Get(),SplinePoints[i],SplinePoints[i+1]) )
		{
			Data.CurrentTension = FMath::Clamp(Data.CurrentTension+Settings.TensionIncPerFailure,0.0f,1.0f);
			return;
		}
	}

	Data.ProcessedPath.RemoveAt(Data.ProcessedPath.Num()-1);
	Data.ProcessedPath.Append(SplinePoints);
	Data.CurrentTension = Settings.BaseTension; //Reset tension value.

	//If we arrived at the last waypoint, and we completed the SmoothedInitialTurnEnd, the algorithm can end.
	if ( Data.NextBPIdx == Data.BasePath.Num()-1 && Data.SmoothedInitialTurnEnd )
	{
		Data.State = ENavSmootherState::Finished;
		return;
	}
	
	if ( !Data.SmoothedInitialTurnEnd )
		Data.SmoothedInitialTurnEnd = true;
	else
		Data.NextBPIdx++;	
}

float UNavSmootherCore::GetTurnRequiredToTargetPoint(const FVector& ForwardVector, const FVector& OriginPoint, const FVector& TargetPoint)
{
	FTransform DirTransform;
	DirTransform.SetLocation(OriginPoint);
	DirTransform.SetRotation(ForwardVector.ToOrientationQuat());
	const FRotator Rotation = UKismetMathLibrary::FindRelativeLookAtRotation(DirTransform,TargetPoint);
	return Rotation.Yaw;	
}

int32 UNavSmootherCore::GetNextWaypointBasedOnLOS(const FVector& StartPoint, FNavSmootherPathData& Data)
{
	if ( !Data.NavAgent.IsValid() ) 
		return -1;

	if ( !Data.BasePath.IsValidIndex(Data.NextBPIdx) )
		return -1;

	FVector Origin;
	FVector Extent;
	Data.NavAgent->GetActorBounds(true,Origin,Extent);
	
	const TOptional<FVector> ProjectedStartPoint = ProjectPointToNavigation(Data.NavAgent.Get(),StartPoint,Extent);
	if ( !ProjectedStartPoint.IsSet() )
		return -1;
	
	int32 LastVisibleIndex = -1;

	for ( int32 Index = Data.NextBPIdx; Index < Data.BasePath.Num(); ++Index )
	{
		//We know that all endpoints are already projected so no need to check that.
		const FVector EndPoint = Data.BasePath[Index];

		if ( !NavPathRaycast(Data.NavAgent.Get(),ProjectedStartPoint.GetValue(), EndPoint) )
			return LastVisibleIndex;

		LastVisibleIndex = Index;
	}

	return LastVisibleIndex;	
}

