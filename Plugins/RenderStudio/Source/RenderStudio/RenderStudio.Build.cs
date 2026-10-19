using UnrealBuildTool;

public class RenderStudio : ModuleRules
{
	public RenderStudio(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"AppFramework",
			"ApplicationCore",
			"InputCore",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"UnrealEd",
			"LevelEditor",
			"Projects",
			"PropertyEditor",
			"AdvancedPreviewScene",
			"MaterialEditor",
			"AssetTools",
			"AssetRegistry",
			"ContentBrowser",
			"DesktopPlatform",
			"ImageWrapper",
			"RenderCore",
			"RHI",
			"ToolMenus",
			"MainFrame",
			"WorkspaceMenuStructure",
		});
	}
}
