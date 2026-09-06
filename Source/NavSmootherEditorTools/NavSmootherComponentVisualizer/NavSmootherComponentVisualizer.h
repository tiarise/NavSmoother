#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

enum class ESmootherPointType
{
	None,
	StartPoint,
	EndPoint,
};

class NAVSMOOTHEREDITORTOOLS_API FNavSmootherComponentVisualizer : public FComponentVisualizer
{
public:
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) override;
	virtual bool VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click) override;
	virtual void EndEditing() override;
	virtual bool GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const override;
	virtual bool HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale) override;
	
private:
	ESmootherPointType m_SelectedPointType = ESmootherPointType::None;
	FComponentPropertyPath m_PropertyPath;
};
