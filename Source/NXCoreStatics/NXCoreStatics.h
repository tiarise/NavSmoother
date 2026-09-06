#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/*
    A minimal stripped version of the original NXCoreStatics.
    We don't need the whole module for NavSmoother.
*/
class FNXCoreStaticsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
