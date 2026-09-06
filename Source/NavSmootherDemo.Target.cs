using UnrealBuildTool;

public class NavSmootherDemoTarget : TargetRules
{
	public NavSmootherDemoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		NativePointerMemberBehaviorOverride = PointerMemberBehavior.Disallow;
		ExtraModuleNames.AddRange(new string[]
		{
			"NavSmootherDemo", 
			"NavSmoother", 
			"NXCoreStatics"
		});
	}
}