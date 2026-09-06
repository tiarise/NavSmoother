#include "NavSmootherComponentVisualizer.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Font.h"
#include "NavSmootherEditorTools/ToolsController/NavSmootherToolsController.h"

struct HNavSmootherTestLocationHitProxy : HComponentVisProxy
{
	DECLARE_HIT_PROXY();
	
	HNavSmootherTestLocationHitProxy(const UActorComponent* InComponent, ESmootherPointType InPointSelected) : HComponentVisProxy{InComponent}
	{
		SelectedPointType = InPointSelected;
	}
	
	ESmootherPointType SelectedPointType;
}; 

IMPLEMENT_HIT_PROXY(HNavSmootherTestLocationHitProxy,HComponentVisProxy);

void FNavSmootherComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	FComponentVisualizer::DrawVisualization(Component, View, PDI);
	
	if ( !Component || !Component->GetOwner() )
		return;
	
	const ANavSmootherVisProxyActor* VisProxyActor = Cast<ANavSmootherVisProxyActor>(Component->GetOwner());
	if ( !VisProxyActor )
		return;
	
	UNavSmootherToolsController* ToolController = VisProxyActor->GetController();
	if ( !ToolController )
		return;
	
	/*
		Draw start point and add hit proxy for click detection.
		The HitProxy is a separate gizmo from anything on the actor. So if we move this we need to update the
		VisProxy RootComponent location/rotation also! As that is separate! And also the UI properties what are
		also separate. These three things must be in sync. See HandleInputDelta, 
	*/
	PDI->SetHitProxy(new HNavSmootherTestLocationHitProxy(Component,ESmootherPointType::StartPoint));
    PDI->DrawPoint(ToolController->GetSettings()->TestPositionStart, FLinearColor::Green, 30, SDPG_Foreground);
	
	//Draw the rotation indicator for the start point.
	FTransform ArrowTransform;
	ArrowTransform.SetLocation(ToolController->GetSettings()->TestPositionStart);
	ArrowTransform.SetRotation(ToolController->GetSettings()->TestPositionStartRotation.Quaternion());
	DrawDirectionalArrow(PDI,ArrowTransform.ToMatrixNoScale(),FColor(0.0f,250.0f,0.0f),150.0f,25.0f,0,10);
	
	//Draw the end point. We register a hit proxy which will be added as a separate gizmo. Which we need for the end point!
	PDI->SetHitProxy(new HNavSmootherTestLocationHitProxy(Component,ESmootherPointType::EndPoint));
    PDI->DrawPoint(ToolController->GetSettings()->TestPositionEnd, FLinearColor::Red, 30, SDPG_Foreground);	
	
	PDI->SetHitProxy(nullptr);
}

void FNavSmootherComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	FComponentVisualizer::DrawVisualizationHUD(Component, Viewport, View, Canvas);
	
	if ( !Component )
		return;
	
	const ANavSmootherVisProxyActor* VisProxyActor = Cast<ANavSmootherVisProxyActor>(Component->GetOwner());
	if ( !VisProxyActor )
		return;
	
	UNavSmootherToolsController* ToolController = VisProxyActor->GetController();
	if ( !ToolController )
		return;
	
	UFont* Font = GEngine->GetLargeFont();
	
	/*
		Resolution is the pixel count for a screen. DPI is dots per inch, so the density of the pixels.
		We could have a high resolution but if the physical size of the screen is big, it would still look weird.
		We take this into consideration here.
	*/
	const float DPIScale = Canvas->GetDPIScale();
	
	FVector2D PositionText;
	if ( View->WorldToPixel(ToolController->GetSettings()->TestPositionStart,PositionText) )
	{
		int32 TextWidth = Font->GetStringSize(TEXT("Start Point"));
		FCanvasTextItem TextItem = FCanvasTextItem((PositionText/DPIScale) - FVector2D(TextWidth,TextWidth), FText::FromString("Start Point for test path."), Font, FLinearColor::Green);
		TextItem.EnableShadow(FLinearColor::Black);
		TextItem.Draw(Canvas);
	}
	if ( View->WorldToPixel(ToolController->GetSettings()->TestPositionEnd,PositionText) )
	{
		int32 TextWidth = Font->GetStringSize(TEXT("End Point"));
		FCanvasTextItem TextItem = FCanvasTextItem((PositionText/DPIScale) - FVector2D(TextWidth,TextWidth), FText::FromString("End Point for test path."), Font, FLinearColor::Red);
		TextItem.EnableShadow(FLinearColor::Black);
		TextItem.Draw(Canvas);		
	}
}

bool FNavSmootherComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	//Here you could set conditions on accepting a click or not.
	if ( !VisProxy || !VisProxy->Component.IsValid() || !VisProxy->IsA(HNavSmootherTestLocationHitProxy::StaticGetType()) )
		return false;
	
	const USceneComponent* RootComponent = Cast<USceneComponent>(VisProxy->Component.Get());
	if ( !RootComponent )
		return false;
	
	//Store accessor for the RootComponent. (Needed in GetWidgetLocation.)
	m_PropertyPath = FComponentPropertyPath(RootComponent);
	
	//Identify which point we clicked.
	HNavSmootherTestLocationHitProxy* Proxy = static_cast<HNavSmootherTestLocationHitProxy*>(VisProxy);
	m_SelectedPointType = Proxy->SelectedPointType;
	
	return true;
}

void FNavSmootherComponentVisualizer::EndEditing()
{
	FComponentVisualizer::EndEditing();
	m_SelectedPointType = ESmootherPointType::None;
	m_PropertyPath.Reset();
}

bool FNavSmootherComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	const USceneComponent* Component = Cast<USceneComponent>(m_PropertyPath.GetComponent());
	if ( !Component )
		return false;
	
	const ANavSmootherVisProxyActor* VisProxyActor = Cast<ANavSmootherVisProxyActor>(Component->GetOwner());
	if ( !VisProxyActor )
		return false;
	
	UNavSmootherToolsController* ToolController = VisProxyActor->GetController();
	if ( !ToolController )
		return false;
	
	if ( m_SelectedPointType == ESmootherPointType::StartPoint )
	{
		OutLocation = ToolController->GetSettings()->TestPositionStart;
	}
	
	if ( m_SelectedPointType == ESmootherPointType::EndPoint )
	{
		OutLocation = ToolController->GetSettings()->TestPositionEnd;
	}
	
	return true;
}

bool FNavSmootherComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate,
	FVector& DeltaScale)
{
	USceneComponent* Component = Cast<USceneComponent>(m_PropertyPath.GetComponent());
	if ( !Component )
		return false;
	
	const ANavSmootherVisProxyActor* VisProxyActor = Cast<ANavSmootherVisProxyActor>(Component->GetOwner());
	if ( !VisProxyActor )
		return false;
	
	UNavSmootherToolsController* ToolController = VisProxyActor->GetController();
	if ( !ToolController )
		return false;
	
	if ( m_SelectedPointType == ESmootherPointType::StartPoint )
	{
		/*
			If you update the VisProxy StartPoint gizmo, we need to update the UI properties,
			the VisProxy RootComponent location and rotation too!
		*/
		ToolController->GetSettings()->TestPositionStart += DeltaTranslate;
		ToolController->GetSettings()->TestPositionStartRotation += DeltaRotate;
		ToolController->AddTranslationDeltaForVisProxyActor(DeltaTranslate, DeltaRotate);
	}	
	if ( m_SelectedPointType == ESmootherPointType::EndPoint )
	{
		ToolController->GetSettings()->TestPositionEnd += DeltaTranslate;
	}
	else
	{
		return false;
	}
	
	return true;
}
