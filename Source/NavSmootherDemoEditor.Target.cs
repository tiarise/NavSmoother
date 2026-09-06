using UnrealBuildTool;

public class NavSmootherDemoEditorTarget : TargetRules
{
	public NavSmootherDemoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		NativePointerMemberBehaviorOverride = PointerMemberBehavior.Disallow;
		ExtraModuleNames.AddRange(new string[]
		{
			"NavSmootherDemo", 
			"NavSmoother", 
			"NavSmootherEditorTools", 
			"NXCoreStatics"
		});
	}
}