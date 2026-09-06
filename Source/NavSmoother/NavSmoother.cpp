#include "NavSmoother.h"

#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"

#define LOCTEXT_NAMESPACE "FNavSmootherModule"

void FNavSmootherModule::StartupModule()
{
}

void FNavSmootherModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FNavSmootherModule, NavSmoother)