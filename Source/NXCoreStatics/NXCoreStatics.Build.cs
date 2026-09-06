using UnrealBuildTool;

public class NXCoreStatics : ModuleRules
{
    public NXCoreStatics(ReadOnlyTargetRules Target) : base(Target)
    {
	    // Disable Unity builds to expose include errors.
        bUseUnity = false;

        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",                
            }
        );
    }
}