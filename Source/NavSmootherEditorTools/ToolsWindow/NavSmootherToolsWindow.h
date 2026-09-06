#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UNavSmootherToolsController;

class NAVSMOOTHEREDITORTOOLS_API SNavSmootherToolsWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SNavSmootherToolsWindow) : _Controller(nullptr) {}
	    SLATE_ARGUMENT(UNavSmootherToolsController*, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
	virtual ~SNavSmootherToolsWindow() override;
	
	//virtual void OnVisualSchedulingNotification(EVisualSchedulingNotification Notification) override;
	
private:
	void OnComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateComboItem(TSharedPtr<FString> Item);
	
	bool IsInManualMode() const;
	FReply OnSimulateClicked();
	
	void OnSelectedActorChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh);
	
	ECheckBoxState GetStatsToggleState() const;
	void OnStatsToggleChanged(ECheckBoxState NewState);
	
	void OnDetailsChanged(const FPropertyChangedEvent& PropertyChangedEvent);
	
	TArray<TSharedPtr<FString>>& GetComboBoxOptions();
	
	//Controller related data
	TObjectPtr<UNavSmootherToolsController> m_Controller;
	TSharedPtr<IDetailsView> m_DetailsView;
	
	//Combo box related data
	TSharedPtr<SComboBox<TSharedPtr<FString>>> m_ComboBox;
	TArray<TSharedPtr<FString>> m_ComboBoxOptions;
	
	//Simulation button related data
	TSharedPtr<SButton> m_StartSimulationButton;
	bool m_SimulateToggled = false;
	
	//Stats toggle related data
	TSharedPtr<SCheckBox> m_StatsToggle;
	
	TSharedPtr<SNumericEntryBox<float>> m_SimulationTimer;
	
	//Handle for the event OnActorSelectionChanged
	FDelegateHandle OnActorSelectionChangedHandle;
};
