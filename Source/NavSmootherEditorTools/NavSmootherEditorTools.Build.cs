using UnrealBuildTool;

public class NavSmootherEditorTools : ModuleRules
{
    public NavSmootherEditorTools(ReadOnlyTargetRules Target) : base(Target)
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
                "Slate",
                "SlateCore",
                "UnrealEd",
                "LevelEditor",
                "ToolMenus",
                "PropertyEditor",
                "InputCore",
                "NavSmoother"
            }
        );
    }
}