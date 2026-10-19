using UnrealBuildTool;

public class RenderStudioEditorTarget : TargetRules
{
	public RenderStudioEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("RenderStudioGame");
	}
}
