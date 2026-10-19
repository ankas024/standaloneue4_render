using UnrealBuildTool;

/// <summary>
/// Development target: builds the project's modules for the regular UE4Editor, so the plugin can be
/// iterated on inside the editor (Window > Render Studio) or run with UE4Editor ... -RenderStudio.
/// </summary>
public class RenderStudioEditorTarget : TargetRules
{
	public RenderStudioEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		ExtraModuleNames.Add("RenderStudioGame");
	}
}
