#include "NavSmootherEditorTools.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ToolMenus.h"
#include "NavSmootherEditorTools/NavSmootherComponentVisualizer/NavSmootherComponentVisualizer.h"
#include "NavSmootherEditorTools/ToolsController/NavSmootherToolsController.h"
#include "NavSmootherEditorTools/ToolsWindow/NavSmootherToolsWindow.h"

#define LOCTEXT_NAMESPACE "FNavSmootherEditorToolsModule"

static const FName NavSmootherTabName("NavSmoother Test");

void FNavSmootherEditorToolsModule::StartupModule()
{
    if ( !GUnrealEd ) //Module load needs to be set to -> "LoadingPhase": "PostEngineInit"
        return;
    
    //Create the controller.
    m_Controller = TStrongObjectPtr<UNavSmootherToolsController>(NewObject<UNavSmootherToolsController>());
    
    //Register FNavSmootherComponentVisualizer
    TSharedPtr<FNavSmootherComponentVisualizer> Visualizer = MakeShareable(new FNavSmootherComponentVisualizer());
    GUnrealEd->RegisterComponentVisualizer(USceneComponent::StaticClass()->GetFName(), Visualizer);
    Visualizer->OnRegister();    
    
    //Register the creation logic for the Tools tab.
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        NavSmootherTabName,
        FOnSpawnTab::CreateRaw(this, &FNavSmootherEditorToolsModule::CreateNavSmootherToolsTab))
        .SetDisplayName(FText::FromName(NavSmootherTabName))
        .SetMenuType(ETabSpawnerMenuType::Hidden); //Do not expose it to the Window menu automatically.

    //Register the position of the Tools tab button into the editor menus.
    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, 
        &FNavSmootherEditorToolsModule::RegisterNavSmootherToolsTab));
}

void FNavSmootherEditorToolsModule::ShutdownModule()
{
    if ( !GUnrealEd )
        return;
    
    //Unregister FNavSmootherComponentVisualizer
    GUnrealEd->UnregisterComponentVisualizer(UChildActorComponent::StaticClass()->GetFName());    
    
    //Unregister the Tab from the editor.
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(NavSmootherTabName);
}

void FNavSmootherEditorToolsModule::OpenNavSmootherToolsTab()
{
    FGlobalTabmanager::Get()->TryInvokeTab(NavSmootherTabName);
}

void FNavSmootherEditorToolsModule::RegisterNavSmootherToolsTab()
{
    UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
    FToolMenuSection& Section = Menu->FindOrAddSection("NavSmoother");

    Section.AddMenuEntry(
        "OpenNavSmoother",
        LOCTEXT("OpenNavSmoother_Label", "Nav Smoother Testing"),
        LOCTEXT("OpenNavSmoother_Tooltip", "Open the Nav Smoother Testing window."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateLambda([]()
            {
                FGlobalTabmanager::Get()->TryInvokeTab(NavSmootherTabName);
            })
        )
    );
}

TSharedRef<SDockTab> FNavSmootherEditorToolsModule::CreateNavSmootherToolsTab(const FSpawnTabArgs& Args)
{
    TSharedRef<SDockTab> Tab = SNew(SDockTab)
    .TabRole(ETabRole::NomadTab)
    [
        SNew(SNavSmootherToolsWindow)
        .Controller(m_Controller.Get())
    ];  
    
    return Tab;
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FNavSmootherEditorToolsModule, NavSmootherEditorTools)