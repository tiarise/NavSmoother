#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class UNavSmootherToolsController;

class FNavSmootherEditorToolsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    
    void OpenNavSmootherToolsTab();
    
private:
    void RegisterNavSmootherToolsTab();
    TSharedRef<SDockTab> CreateNavSmootherToolsTab(const FSpawnTabArgs& Args);
    
    //This will keep the controller alive. It won't be GC-ed.
    TStrongObjectPtr<UNavSmootherToolsController> m_Controller;
};
