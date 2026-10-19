using UnrealBuildTool;

/// <summary>
/// The standalone application: builds RenderStudio.exe with its own copy of every engine module
/// (RenderStudio-*.dll next to the exe), so it does not depend on, or interfere with, a UE4Editor
/// build. It opens straight into Render Studio. The level editor never appears.
///
/// Building it compiles the engine once for this target (expect the time of a full editor build).
/// Package it with Scripts\Package.bat.
/// </summary>
public class RenderStudioTarget : TargetRules
{
	public RenderStudioTarget(TargetInfo Target) : base(Target)
	{
		// The node based material editor, shader compilation for new materials and asset import
		// are editor-only engine features, so the application is built as an editor-type target.
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		BuildEnvironment = TargetBuildEnvironment.Unique;
		ExtraModuleNames.Add("RenderStudioGame");

		// Always start in application mode (no -RenderStudio switch needed)
		GlobalDefinitions.Add("RENDERSTUDIO_STANDALONE=1");

		// No RenderStudio-Cmd.exe console twin
		bBuildAdditionalConsoleApp = false;

		// Features a look-dev tool does not need
		bCompileCEF3 = false;
		bUsesSteam = false;
	}
}
