using UnrealBuildTool;

public class RenderStudioTarget : TargetRules
{
	public RenderStudioTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("RenderStudioGame");
	}
}
