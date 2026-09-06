#include "NavSmootherToolsWindow.h"
#include "LevelEditor.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "NavSmootherEditorTools/ToolsController/NavSmootherToolsController.h"


static const FText SimulateButtonActivateManual = INVTEXT("Trigger Simulation");
static const FText SimulateButtonActivateAuto = INVTEXT("Activate Simulation");
static const FText SimulateButtonDeactivateAuto = INVTEXT("Deactivate Simulation");

void SNavSmootherToolsWindow::Construct(const FArguments& InArgs)
{
    //Cache Controller.
    m_Controller = InArgs._Controller;
    
    //Subscribe to when an actor is selected in the editor.
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    OnActorSelectionChangedHandle = LevelEditorModule.OnActorSelectionChanged().AddRaw(this,&SNavSmootherToolsWindow::OnSelectedActorChanged);
    
    //Create the ComboBox for the dropdown.
    m_ComboBox = SNew(SComboBox<TSharedPtr<FString>>)
    .OptionsSource(&GetComboBoxOptions())
    .OnGenerateWidget(this, &SNavSmootherToolsWindow::GenerateComboItem)
    .OnSelectionChanged(this, &SNavSmootherToolsWindow::OnComboBoxSelectionChanged)
    [
        SNew(STextBlock)
        .Text_Lambda([this]()
        {
            const TSharedPtr<FString> SelectedItem = m_ComboBox->GetSelectedItem();
            if ( !SelectedItem.IsValid() && GetComboBoxOptions().Num() > 0 )
                m_ComboBox->SetSelectedItem(GetComboBoxOptions()[0]);
            
            return SelectedItem.IsValid() ? FText::FromString(*SelectedItem.Get()) : INVTEXT("Missing elements!");
        })
    ];
    
    //Create the Simulation button.
    m_StartSimulationButton = SNew(SButton)
    .HAlign(HAlign_Center)
    .VAlign(VAlign_Center)
    .ButtonColorAndOpacity_Lambda([this]()
    {
        const FSlateColor Green = FSlateColor(FLinearColor(0.1f, 0.9f, 0.4f, 1.0f));
        const FSlateColor Red =  FSlateColor(FLinearColor(0.9f, 0.1f, 0.3f, 1.0f));
        
        if ( IsInManualMode() )
        {
            return Green;
        }
        else
        {
            if ( m_SimulateToggled )
                return Red;
            else
                return Green;
        }
    })    
    .OnClicked(this, &SNavSmootherToolsWindow::OnSimulateClicked)
    [
        SNew(STextBlock)
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
        .Justification(ETextJustify::Center)
        .Text_Lambda([this]()
        {
            if ( IsInManualMode() )
            {
                return SimulateButtonActivateManual;
            }
            else
            {
                if ( m_SimulateToggled )
                    return SimulateButtonDeactivateAuto;
                else
                    return SimulateButtonActivateAuto;
            }
        })
    ];
    
    m_SimulationTimer = SNew(SNumericEntryBox<float>)
    .Value_Lambda([this]()
    {
        return m_Controller->GetSimulationTimer();
    })
    .OnValueChanged_Lambda([this](float NewValue)
    {
        m_Controller->SetSimulationTimer(FMath::Max(NewValue, 0.1f));
    })
    .MinValue(0.1f)
    .MinSliderValue(0.1f);
    
    m_StatsToggle = SNew(SCheckBox)
    .IsChecked(this, &SNavSmootherToolsWindow::GetStatsToggleState)
    .OnCheckStateChanged(this, &SNavSmootherToolsWindow::OnStatsToggleChanged)
    [
        SNew(STextBlock)
        .Text(INVTEXT("Show Performance Stats"))
    ];    
    
    //Create the details view from the Controller settings.
    FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
    FDetailsViewArgs Args;
    Args.bHideSelectionTip = true; //Hide a pesky automatically created text on the UI.
    m_DetailsView = PropertyModule.CreateDetailView(Args);
    m_DetailsView->SetObject( m_Controller->GetSettings());
    m_DetailsView->OnFinishedChangingProperties().AddSP(this,&SNavSmootherToolsWindow::OnDetailsChanged);
    
    ChildSlot 
    [
        SNew(SVerticalBox)
        
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SBox)
            .HeightOverride(40.0f)
            [
                m_StartSimulationButton.ToSharedRef()
            ]
        ]
        
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SVerticalBox)
            
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 5.0f)
            .HAlign(HAlign_Center)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .VAlign(VAlign_Center)
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(INVTEXT("Simulation Mode Selection:"))                    
                ]
                + SHorizontalBox::Slot()
                .VAlign(VAlign_Center)
                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                .AutoWidth()
                [
                    m_ComboBox.ToSharedRef()
                ]
            ]
            
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 5.0f)
            .HAlign(HAlign_Center)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .VAlign(VAlign_Center)
                .AutoWidth()
                [
                    SNew(STextBlock)
                    .Text(INVTEXT("Simulation Interval:"))                    
                ]
                + SHorizontalBox::Slot()
                .VAlign(VAlign_Center)
                .Padding(5.0f, 0.0f, 0.0f, 0.0f)
                .AutoWidth()
                [
                    m_SimulationTimer.ToSharedRef()
                ]
            ]
            
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 5.0f)
            .HAlign(HAlign_Center)
            [
                m_StatsToggle.ToSharedRef()
            ]
        ]
        
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        [
            m_DetailsView.ToSharedRef()
        ]
    ];
}

SNavSmootherToolsWindow::~SNavSmootherToolsWindow()
{
    //Unregister if the OnActorSelectionChanged is valid.
    if ( OnActorSelectionChangedHandle.IsValid() )
    {
        OnActorSelectionChangedHandle.Reset();
    }
}

TSharedRef<SWidget> SNavSmootherToolsWindow::GenerateComboItem(TSharedPtr<FString> Item)
{
    return SNew(STextBlock).Text(FText::FromString(*Item));   
}

void SNavSmootherToolsWindow::OnComboBoxSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
    //If something fishy is happening do an early exit.
    if ( !NewSelection.IsValid() )
    {
        return;
    }
    
    //Update underlying controller data.
    for ( ENavCalculationMode Mode : TEnumRange<ENavCalculationMode>() )
    {
        if ( UEnum::GetDisplayValueAsText(Mode).ToString() == *NewSelection )
        {
            m_Controller->SetMode(Mode);
            break;
        }
    }
    
    //If we switch from auto to manual during simulation we turn that off.
    if ( IsInManualMode() )
        m_SimulateToggled = false;
}

bool SNavSmootherToolsWindow::IsInManualMode() const
{
   if ( !m_Controller )
       return false;
    
    return m_Controller->GetMode() == ENavCalculationMode::Manual;
}

FReply SNavSmootherToolsWindow::OnSimulateClicked()
{
    if ( !m_Controller )
        return FReply::Unhandled();
    
    if ( IsInManualMode() )
        m_Controller->StartSimulation();
    else
    {
        m_SimulateToggled = !m_SimulateToggled;
        
        if ( m_SimulateToggled )
            m_Controller->StartSimulation();
        else
            m_Controller->StopSimulation();
    }
    
    return FReply::Handled();   
}

void SNavSmootherToolsWindow::OnSelectedActorChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
    /*
        The idea is, since the ANavSmootherVisProxyActor is hidden, it can only be selected by the controller.
        If the controller selected it, that means we are in simulation mode.
        If the player selects a different actor or just adds one to the selection, or we have nothing selected we stop the simulation!
    */
    if ( NewSelection.Num() != 1 )
    {
        m_Controller->StopSimulation();
        m_SimulateToggled = false;
        return;
    }
        
    const ANavSmootherVisProxyActor* VisProxyActor = Cast<ANavSmootherVisProxyActor>(NewSelection[0]);
    if ( !VisProxyActor )
    {
        m_Controller->StopSimulation();
        m_SimulateToggled = false;
    }
}

ECheckBoxState SNavSmootherToolsWindow::GetStatsToggleState() const
{
    if ( !m_Controller )
        return ECheckBoxState::Unchecked;
    
    return m_Controller->GetShowStats() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SNavSmootherToolsWindow::OnStatsToggleChanged(ECheckBoxState NewState)
{
    if ( !m_Controller )
        return;

    m_Controller->SetShowStats(NewState == ECheckBoxState::Checked);
}

void SNavSmootherToolsWindow::OnDetailsChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
    //This setup with the Property will make sure if you change the X component of the FVector we still check the vector name not X.
    const FProperty* Property = PropertyChangedEvent.MemberProperty ? PropertyChangedEvent.MemberProperty : PropertyChangedEvent.Property;
    if ( !Property || !m_Controller )
        return;

    const FName PropertyName = Property->GetFName();
    if ( PropertyName == GET_MEMBER_NAME_CHECKED(UNavSmootherToolsSettings, TestPositionStart) || 
        PropertyName == GET_MEMBER_NAME_CHECKED(UNavSmootherToolsSettings, TestPositionStartRotation) )
    {
        m_Controller->SetTranslationForVisProxyActor(m_Controller->GetSettings()->TestPositionStart, m_Controller->GetSettings()->TestPositionStartRotation);
    }
}

TArray<TSharedPtr<FString>>& SNavSmootherToolsWindow::GetComboBoxOptions()
{
    //Only populate if it's empty.
    if (  m_ComboBoxOptions.Num() > 0 )
    {
        return  m_ComboBoxOptions;
    }

    //Generate values into the list.
    for ( ENavCalculationMode Mode : TEnumRange<ENavCalculationMode>() )
    {
        if ( Mode == ENavCalculationMode::Count )
            continue;
        
         m_ComboBoxOptions.Add(MakeShared<FString>(UEnum::GetDisplayValueAsText(Mode).ToString()));
    }
    
    return  m_ComboBoxOptions;
}
