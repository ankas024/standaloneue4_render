#include "SRenderStudioMainFrame.h"

#include "Components/MeshComponent.h"
#include "DesktopPlatformModule.h"
#include "DynamicRHI.h"
#include "EditorStyleSet.h"
#include "Engine/TextureCube.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "IDetailsView.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/CommandLine.h"
#include "Misc/MessageDialog.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "RenderStudioCommands.h"
#include "RenderStudioScene.h"
#include "RenderStudioSettings.h"
#include "RenderStudioStyle.h"
#include "RenderStudioViewportClient.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "SRenderStudioMaterialPanel.h"
#include "SRenderStudioOutliner.h"
#include "SRenderStudioPanel.h"
#include "SRenderStudioRenderDialog.h"
#include "SRenderStudioViewport.h"
#include "UnrealWidget.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioMainFrame"

const FName SRenderStudioMainFrame::ViewportTabId(TEXT("RenderStudio_Viewport"));
const FName SRenderStudioMainFrame::OutlinerTabId(TEXT("RenderStudio_Outliner"));
const FName SRenderStudioMainFrame::PropertiesTabId(TEXT("RenderStudio_Properties"));
const FName SRenderStudioMainFrame::MaterialsTabId(TEXT("RenderStudio_Materials"));
const FName SRenderStudioMainFrame::EnvironmentTabId(TEXT("RenderStudio_Environment"));

namespace RenderStudioMainFrame
{
	static const TCHAR* SceneFileTypes = TEXT("Render Studio scene (*.rss)|*.rss");

	/** Categories that only matter for games; hidden to keep the properties panel artist friendly. */
	static bool IsHiddenCategory(const FString& Category)
	{
		static const TCHAR* HiddenCategories[] =
		{
			TEXT("Physics"), TEXT("Collision"), TEXT("Tags"), TEXT("Cooking"), TEXT("Activation"),
			TEXT("Events"), TEXT("Replication"), TEXT("ComponentReplication"), TEXT("AssetUserData"),
			TEXT("Navigation"), TEXT("VirtualTexture"), TEXT("HLOD"), TEXT("Mobile"), TEXT("Variable"),
			TEXT("Sockets"), TEXT("ComponentTick"), TEXT("Tick"), TEXT("Input"), TEXT("Actor"),
			TEXT("LOD"), TEXT("Lightmass"), TEXT("Lighting Channels"),
		};

		FString TopLevel = Category;
		int32 SeparatorIndex = INDEX_NONE;
		if (TopLevel.FindChar(TEXT('|'), SeparatorIndex))
		{
			TopLevel = TopLevel.Left(SeparatorIndex);
		}
		TopLevel.TrimStartAndEndInline();

		for (const TCHAR* Hidden : HiddenCategories)
		{
			if (TopLevel.Equals(Hidden, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}

SRenderStudioMainFrame::~SRenderStudioMainFrame()
{
	if (Scene.IsValid())
	{
		Scene->OnSelectionChanged().RemoveAll(this);
		Scene->OnDocumentChanged().RemoveAll(this);
		Scene->OnSceneLoaded().RemoveAll(this);
	}

	if (TabManager.IsValid())
	{
		TabManager->SavePersistentLayout();
		TabManager->CloseAllAreas();
		TabManager->UnregisterAllTabSpawners();
	}
}

void SRenderStudioMainFrame::Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InOwnerTab, const TSharedPtr<SWindow>& InOwnerWindow)
{
	OwnerTab = InOwnerTab;
	OwnerWindow = InOwnerWindow;
	OnExitRequested = InArgs._OnExitRequested;
	StatusText = LOCTEXT("Ready", "Ready");

	Scene = MakeShared<FRenderStudioScene>();
	Scene->Initialize();
	Scene->OnSelectionChanged().AddSP(this, &SRenderStudioMainFrame::OnSceneSelectionChanged);
	Scene->OnDocumentChanged().AddSP(this, &SRenderStudioMainFrame::UpdateTitle);
	Scene->OnSceneLoaded().AddSP(this, &SRenderStudioMainFrame::OnSceneLoaded);

	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	// Properties view is created once and re-targeted when the selection changes
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowPropertyMatrixButton = false;
	DetailsArgs.bUpdatesFromSelection = false;
	DetailsArgs.bLockable = false;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	PropertiesView = PropertyEditorModule.CreateDetailView(DetailsArgs);
	PropertiesView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
	{
		const FProperty* RootProperty = PropertyAndParent.ParentProperties.Num() > 0 ? PropertyAndParent.ParentProperties.Last() : &PropertyAndParent.Property;
		return !RenderStudioMainFrame::IsHiddenCategory(RootProperty->GetMetaData(TEXT("Category")));
	}));

	TabManager = FGlobalTabmanager::Get()->NewTabManager(InOwnerTab);
	TabManager->SetOnPersistLayout(FTabManager::FOnPersistLayout::CreateLambda([](const TSharedRef<FTabManager::FLayout>& Layout)
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, Layout);
	}));
	RegisterTabSpawners();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.PanelDark"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeMenuBar()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeToolbar()
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(FMargin(2.0f))
			[
				SAssignNew(DockingAreaHost, SBox)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MakeStatusBar()
			]
		]
	];

	RestoreLayout(FLayoutSaveRestore::LoadFromConfig(GEditorLayoutIni, MakeDefaultLayout()));
	OnSceneSelectionChanged();
	UpdateTitle();

	// RenderStudio.exe -Scene="C:/Scenes/Watch.rss"
	FString StartupScene;
	if (FParse::Value(FCommandLine::Get(), TEXT("Scene="), StartupScene) && !StartupScene.IsEmpty())
	{
		OpenSceneFile(StartupScene);
	}
}

FReply SRenderStudioMainFrame::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

//////////////////////////////////////////////////////////////////////////
// Layout

void SRenderStudioMainFrame::RegisterTabSpawners()
{
	TabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &SRenderStudioMainFrame::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	TabManager->RegisterTabSpawner(OutlinerTabId, FOnSpawnTab::CreateSP(this, &SRenderStudioMainFrame::SpawnOutlinerTab))
		.SetDisplayName(LOCTEXT("OutlinerTab", "Scene"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Outliner"));

	TabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &SRenderStudioMainFrame::SpawnPropertiesTab))
		.SetDisplayName(LOCTEXT("PropertiesTab", "Properties"))
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	TabManager->RegisterTabSpawner(MaterialsTabId, FOnSpawnTab::CreateSP(this, &SRenderStudioMainFrame::SpawnMaterialsTab))
		.SetDisplayName(LOCTEXT("MaterialsTab", "Materials"))
		.SetIcon(FSlateIcon(FRenderStudioStyle::GetStyleSetName(), "RenderStudio.Icon.Material"));

	TabManager->RegisterTabSpawner(EnvironmentTabId, FOnSpawnTab::CreateSP(this, &SRenderStudioMainFrame::SpawnEnvironmentTab))
		.SetDisplayName(LOCTEXT("EnvironmentTab", "Environment & Render"))
		.SetIcon(FSlateIcon(FRenderStudioStyle::GetStyleSetName(), "RenderStudio.Icon.Sky"));
}

TSharedRef<FTabManager::FLayout> SRenderStudioMainFrame::MakeDefaultLayout() const
{
	return FTabManager::NewLayout("RenderStudio_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(OutlinerTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.6f)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.55f)
				->SetHideTabWell(true)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.25f)
				->AddTab(MaterialsTabId, ETabState::OpenedTab)
				->AddTab(EnvironmentTabId, ETabState::OpenedTab)
				->SetForegroundTab(MaterialsTabId)
			)
		);
}

void SRenderStudioMainFrame::RestoreLayout(const TSharedRef<FTabManager::FLayout>& Layout)
{
	TSharedPtr<SWidget> DockingArea = TabManager->RestoreFrom(Layout, OwnerWindow.Pin());
	DockingAreaHost->SetContent(DockingArea.IsValid() ? DockingArea.ToSharedRef() : SNullWidget::NullWidget);
}

TSharedRef<SDockTab> SRenderStudioMainFrame::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SRenderStudioViewport> Viewport = SNew(SRenderStudioViewport, Scene.ToSharedRef());
	ViewportWidget = Viewport;

	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab", "Viewport"))
		.TabRole(ETabRole::PanelTab)
		[
			Viewport
		];
}

TSharedRef<SDockTab> SRenderStudioMainFrame::SpawnOutlinerTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("OutlinerTab", "Scene"))
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SRenderStudioOutliner, Scene.ToSharedRef())
			.CommandList(CommandList)
		];
}

TSharedRef<SDockTab> SRenderStudioMainFrame::SpawnPropertiesTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PropertiesTab", "Properties"))
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SRenderStudioPanel)
			.Title(LOCTEXT("PropertiesTitle", "Object Properties"))
			[
				PropertiesView.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> SRenderStudioMainFrame::SpawnMaterialsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("MaterialsTab", "Materials"))
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SRenderStudioMaterialPanel, Scene.ToSharedRef())
			.CommandList(CommandList)
		];
}

TSharedRef<SDockTab> SRenderStudioMainFrame::SpawnEnvironmentTab(const FSpawnTabArgs& Args)
{
	// Sky HDRI, sun, sky light and post-processing come from the asset viewer profile system,
	// the application settings (camera FOV, grid, render and screenshot options) are shown underneath.
	TArray<FAdvancedPreviewSceneModule::FDetailCustomizationInfo> DetailCustomizations;
	TArray<FAdvancedPreviewSceneModule::FPropertyTypeCustomizationInfo> PropertyTypeCustomizations;
	TArray<FAdvancedPreviewSceneModule::FDetailDelegates> Delegates;

	return SNew(SDockTab)
		.Label(LOCTEXT("EnvironmentTab", "Environment & Render"))
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SRenderStudioPanel)
			.Title(LOCTEXT("EnvironmentTitle", "Environment, Lighting & Post FX"))
			[
				SNew(SAdvancedPreviewDetailsTab, Scene->GetPreviewScene())
				.AdditionalSettings(Scene->GetSettings())
				.DetailCustomizations(DetailCustomizations)
				.PropertyTypeCustomizations(PropertyTypeCustomizations)
				.Delegates(Delegates)
			]
		];
}

//////////////////////////////////////////////////////////////////////////
// Menus / toolbar / status bar

TSharedRef<SWidget> SRenderStudioMainFrame::MakeMenuBar()
{
	FMenuBarBuilder MenuBar(CommandList);
	MenuBar.AddPullDownMenu(LOCTEXT("FileMenu", "File"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillFileMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("EditMenu", "Edit"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillEditMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("CreateMenu", "Create"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillCreateMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("ViewMenu", "View"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillViewMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("EnvironmentMenu", "Environment"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillEnvironmentMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("MaterialMenu", "Material"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillMaterialMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("WindowMenu", "Window"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillWindowMenu));
	MenuBar.AddPullDownMenu(LOCTEXT("HelpMenu", "Help"), FText::GetEmpty(), FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillHelpMenu));

	return SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.MenuBar"))
		.Padding(FMargin(2.0f, 0.0f))
		[
			MenuBar.MakeWidget()
		];
}

void SRenderStudioMainFrame::FillFileMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Scene", LOCTEXT("SceneSection", "Scene"));
	MenuBuilder.AddMenuEntry(Commands.NewScene);
	MenuBuilder.AddMenuEntry(Commands.OpenScene);
	MenuBuilder.AddSubMenu(LOCTEXT("RecentScenes", "Open Recent"), LOCTEXT("RecentScenesTooltip", "Scenes you opened or saved recently."),
		FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillRecentScenesMenu));
	MenuBuilder.AddMenuEntry(Commands.SaveScene);
	MenuBuilder.AddMenuEntry(Commands.SaveSceneAs);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Import", LOCTEXT("ImportSection", "Import"));
	MenuBuilder.AddMenuEntry(Commands.ImportMesh);
	MenuBuilder.AddMenuEntry(Commands.ImportEnvironment);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Output", LOCTEXT("OutputSection", "Output"));
	MenuBuilder.AddMenuEntry(Commands.RenderImage);
	MenuBuilder.AddMenuEntry(Commands.SaveScreenshot);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Exit", FText::GetEmpty());
	MenuBuilder.AddMenuEntry(Commands.Exit);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillRecentScenesMenu(FMenuBuilder& MenuBuilder)
{
	const TArray<FString>& RecentScenes = GetDefault<URenderStudioSettings>()->RecentScenes;
	if (RecentScenes.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoRecentScenes", "(none)"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		return;
	}

	for (const FString& Filename : RecentScenes)
	{
		MenuBuilder.AddMenuEntry(
			FText::FromString(FPaths::GetBaseFilename(Filename)),
			FText::FromString(Filename),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Filename]()
			{
				if (ConfirmCloseDocument())
				{
					OpenSceneFile(Filename);
				}
			})));
	}
}

void SRenderStudioMainFrame::FillEditMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("History", LOCTEXT("HistorySection", "History"));
	MenuBuilder.AddMenuEntry(Commands.Undo, NAME_None, TAttribute<FText>(this, &SRenderStudioMainFrame::GetUndoLabel));
	MenuBuilder.AddMenuEntry(Commands.Redo, NAME_None, TAttribute<FText>(this, &SRenderStudioMainFrame::GetRedoLabel));
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Object", LOCTEXT("ObjectSection", "Object"));
	MenuBuilder.AddMenuEntry(Commands.DuplicateSelected);
	MenuBuilder.AddMenuEntry(Commands.DeleteSelected);
	MenuBuilder.AddMenuEntry(Commands.FocusSelected);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Tools", LOCTEXT("ToolsSection", "Transform Tool"));
	MenuBuilder.AddMenuEntry(Commands.SelectMode);
	MenuBuilder.AddMenuEntry(Commands.TranslateMode);
	MenuBuilder.AddMenuEntry(Commands.RotateMode);
	MenuBuilder.AddMenuEntry(Commands.ScaleMode);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillCreateMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Primitives", LOCTEXT("PrimitivesSection", "Primitives"));
	MenuBuilder.AddMenuEntry(Commands.AddCube);
	MenuBuilder.AddMenuEntry(Commands.AddSphere);
	MenuBuilder.AddMenuEntry(Commands.AddCylinder);
	MenuBuilder.AddMenuEntry(Commands.AddCone);
	MenuBuilder.AddMenuEntry(Commands.AddPlane);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Lights", LOCTEXT("LightsSection", "Lights"));
	MenuBuilder.AddMenuEntry(Commands.AddPointLight);
	MenuBuilder.AddMenuEntry(Commands.AddSpotLight);
	MenuBuilder.AddMenuEntry(Commands.AddRectLight);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Meshes", LOCTEXT("MeshesSection", "Meshes"));
	MenuBuilder.AddMenuEntry(Commands.ImportMesh);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillViewMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Viewport", LOCTEXT("ViewportSection", "Viewport"));
	MenuBuilder.AddMenuEntry(Commands.ToggleGrid);
	MenuBuilder.AddMenuEntry(Commands.ToggleFloor);
	MenuBuilder.AddMenuEntry(Commands.ToggleEnvironment);
	MenuBuilder.AddMenuEntry(Commands.ToggleTurntable);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Views", LOCTEXT("ViewsSection", "Camera Views"));
	MenuBuilder.AddMenuEntry(Commands.ViewPerspective);
	MenuBuilder.AddMenuEntry(Commands.ViewFront);
	MenuBuilder.AddMenuEntry(Commands.ViewBack);
	MenuBuilder.AddMenuEntry(Commands.ViewLeft);
	MenuBuilder.AddMenuEntry(Commands.ViewRight);
	MenuBuilder.AddMenuEntry(Commands.ViewTop);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Camera", LOCTEXT("CameraSection", "Camera"));
	MenuBuilder.AddMenuEntry(Commands.FocusSelected);
	MenuBuilder.AddMenuEntry(Commands.ResetCamera);
	MenuBuilder.AddSubMenu(LOCTEXT("CameraBookmarks", "Camera Bookmarks"), LOCTEXT("CameraBookmarksTooltip", "Store and recall up to four cameras. They are saved with the scene."),
		FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillCameraBookmarksMenu));
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillCameraBookmarksMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Recall", LOCTEXT("RecallSection", "Jump To"));
	for (int32 Index = 0; Index < FRenderStudioScene::NumBookmarks; ++Index)
	{
		MenuBuilder.AddMenuEntry(Commands.RecallBookmark[Index]);
	}
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Store", LOCTEXT("StoreSection", "Store Current Camera"));
	for (int32 Index = 0; Index < FRenderStudioScene::NumBookmarks; ++Index)
	{
		MenuBuilder.AddMenuEntry(Commands.SaveBookmark[Index]);
	}
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillEnvironmentMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Sky", LOCTEXT("SkySection", "Sky"));
	MenuBuilder.AddSubMenu(LOCTEXT("SkyMaps", "HDRI"), LOCTEXT("SkyMapsTooltip", "Pick the panorama used for the backdrop and image based lighting."),
		FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillSkyMenu));
	MenuBuilder.AddMenuEntry(Commands.ImportEnvironment);
	MenuBuilder.AddMenuEntry(Commands.ToggleEnvironment);
	MenuBuilder.AddMenuEntry(Commands.ToggleTurntable);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Ground", LOCTEXT("GroundSection", "Ground"));
	MenuBuilder.AddMenuEntry(Commands.ToggleFloor);
	MenuBuilder.AddMenuEntry(Commands.ToggleGrid);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillSkyMenu(FMenuBuilder& MenuBuilder)
{
	const TArray<UTextureCube*> Maps = Scene->GetEnvironmentMaps();
	if (Maps.Num() == 0)
	{
		MenuBuilder.AddMenuEntry(LOCTEXT("NoSkyMaps", "(no HDRIs found - use Import HDRI)"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([]() { return false; })));
		return;
	}

	for (UTextureCube* Map : Maps)
	{
		TWeakObjectPtr<UTextureCube> WeakMap = Map;
		MenuBuilder.AddMenuEntry(
			FText::FromString(Map->GetName()),
			FText::FromString(Map->GetPathName()),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this, WeakMap]()
				{
					if (UTextureCube* Cubemap = WeakMap.Get())
					{
						Scene->SetEnvironmentMap(Cubemap);
						SetStatus(FText::Format(LOCTEXT("SkyChanged", "Sky: {0}"), FText::FromString(Cubemap->GetName())));
					}
				}),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this, WeakMap]() { return WeakMap.IsValid() && Scene->GetEnvironmentMap() == WeakMap.Get(); })),
			NAME_None,
			EUserInterfaceActionType::RadioButton);
	}
}

void SRenderStudioMainFrame::FillMaterialMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.BeginSection("Create", LOCTEXT("MaterialCreateSection", "Create"));
	MenuBuilder.AddMenuEntry(Commands.NewMaterial);
	MenuBuilder.AddSubMenu(LOCTEXT("NewFromPreset", "New From Preset"), LOCTEXT("NewFromPresetTooltip", "Create a material with a ready-made look and assign it to the selection."),
		FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillPresetMenu, false));
	MenuBuilder.AddMenuEntry(Commands.NewMaterialGraph);
	MenuBuilder.AddMenuEntry(Commands.NewMaterialInstance);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Edit", LOCTEXT("MaterialEditSection", "Edit"));
	MenuBuilder.AddMenuEntry(Commands.AssignMaterial);
	MenuBuilder.AddSubMenu(LOCTEXT("ApplyPreset", "Apply Preset"), LOCTEXT("ApplyPresetTooltip", "Set the selected material instance's colour, metalness, roughness and specular from a preset."),
		FNewMenuDelegate::CreateSP(this, &SRenderStudioMainFrame::FillPresetMenu, true));
	MenuBuilder.AddMenuEntry(Commands.EditMaterial);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillPresetMenu(FMenuBuilder& MenuBuilder, bool bApplyToSelected)
{
	const TArray<FRenderStudioMaterialPreset>& Presets = FRenderStudioScene::GetMaterialPresets();
	for (int32 Index = 0; Index < Presets.Num(); ++Index)
	{
		FUIAction Action = bApplyToSelected
			? FUIAction(FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnApplyPreset, Index), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedInstance))
			: FUIAction(FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterialFromPreset, Index));
		MenuBuilder.AddMenuEntry(FText::FromString(Presets[Index].Name), FText::GetEmpty(), FSlateIcon(), Action);
	}
}

void SRenderStudioMainFrame::FillWindowMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection("Panels", LOCTEXT("PanelsSection", "Panels"));
	TabManager->PopulateLocalTabSpawnerMenu(MenuBuilder);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Layout", LOCTEXT("LayoutSection", "Layout"));
	MenuBuilder.AddMenuEntry(FRenderStudioCommands::Get().ResetLayout);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillHelpMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddMenuEntry(LOCTEXT("Controls", "Controls..."), LOCTEXT("ControlsTooltip", "Mouse and keyboard reference."), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnShowControls)));
	MenuBuilder.AddMenuEntry(FRenderStudioCommands::Get().About);
}

TSharedRef<SWidget> SRenderStudioMainFrame::MakeToolbar()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	const FName EditorStyle = FEditorStyle::GetStyleSetName();
	const FName StudioStyle = FRenderStudioStyle::GetStyleSetName();

	FToolBarBuilder Toolbar(CommandList, FMultiBoxCustomization::None);
	Toolbar.SetStyle(&FEditorStyle::Get(), "ToolBar");

	Toolbar.BeginSection("File");
	Toolbar.AddToolBarButton(Commands.NewScene, NAME_None, LOCTEXT("TB_New", "New"), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.NewLevel"));
	Toolbar.AddToolBarButton(Commands.OpenScene, NAME_None, LOCTEXT("TB_Open", "Open"), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.OpenLevel"));
	Toolbar.AddToolBarButton(Commands.SaveScene, NAME_None, LOCTEXT("TB_Save", "Save"), TAttribute<FText>(), FSlateIcon(EditorStyle, "AssetEditor.SaveAsset"));
	Toolbar.AddToolBarButton(Commands.ImportMesh, NAME_None, LOCTEXT("TB_Import", "Import"), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.ImportScene"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("History");
	Toolbar.AddToolBarButton(Commands.Undo, NAME_None, TAttribute<FText>(), TAttribute<FText>(this, &SRenderStudioMainFrame::GetUndoLabel), FSlateIcon(EditorStyle, "GenericCommands.Undo"));
	Toolbar.AddToolBarButton(Commands.Redo, NAME_None, TAttribute<FText>(), TAttribute<FText>(this, &SRenderStudioMainFrame::GetRedoLabel), FSlateIcon(EditorStyle, "GenericCommands.Redo"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("Transform");
	Toolbar.AddToolBarButton(Commands.SelectMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.SelectMode"));
	Toolbar.AddToolBarButton(Commands.TranslateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "EditorViewport.TranslateMode"));
	Toolbar.AddToolBarButton(Commands.RotateMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "EditorViewport.RotateMode"));
	Toolbar.AddToolBarButton(Commands.ScaleMode, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "EditorViewport.ScaleMode"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("Create");
	Toolbar.AddToolBarButton(Commands.AddCube, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Mesh"));
	Toolbar.AddToolBarButton(Commands.AddSphere, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Sphere"));
	Toolbar.AddToolBarButton(Commands.AddPointLight, NAME_None, LOCTEXT("TB_Light", "Light"), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Light"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("View");
	Toolbar.AddToolBarButton(Commands.ToggleFloor, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "BlueprintEditor.ShowFloor"));
	Toolbar.AddToolBarButton(Commands.ToggleGrid, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "BlueprintEditor.ShowGrid"));
	Toolbar.AddToolBarButton(Commands.ToggleEnvironment, NAME_None, LOCTEXT("TB_Sky", "Sky"), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Sky"));
	Toolbar.AddToolBarButton(Commands.ToggleTurntable, NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(EditorStyle, "EditorViewport.ToggleRealTime"));
	Toolbar.AddToolBarButton(Commands.ResetCamera, NAME_None, LOCTEXT("TB_Camera", "Camera"), TAttribute<FText>(), FSlateIcon(EditorStyle, "BlueprintEditor.ResetCamera"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("Output");
	Toolbar.AddToolBarButton(Commands.RenderImage, NAME_None, LOCTEXT("TB_Render", "Render"), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Camera"));
	Toolbar.AddToolBarButton(Commands.SaveScreenshot, NAME_None, LOCTEXT("TB_Shot", "Screenshot"), TAttribute<FText>(), FSlateIcon(EditorStyle, "HighresScreenshot.Capture"));
	Toolbar.EndSection();
	Toolbar.AddSeparator();

	Toolbar.BeginSection("Material");
	Toolbar.AddToolBarButton(Commands.NewMaterial, NAME_None, LOCTEXT("TB_NewMaterial", "Material"), TAttribute<FText>(), FSlateIcon(StudioStyle, "RenderStudio.Icon.Material"));
	Toolbar.AddToolBarButton(Commands.EditMaterial, NAME_None, LOCTEXT("TB_EditMaterial", "Node Editor"), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.Tabs.EditorModes"));
	Toolbar.EndSection();

	return SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.Toolbar"))
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				Toolbar.MakeWidget()
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.0f, 0.0f))
			[
				SNew(STextBlock)
				.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Title"))
				.Text(LOCTEXT("AppName", "RENDER STUDIO"))
			]
		];
}

TSharedRef<SWidget> SRenderStudioMainFrame::MakeStatusBar()
{
	return SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.StatusBar"))
		.Padding(FMargin(8.0f, 3.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text"))
				.Text(this, &SRenderStudioMainFrame::GetStatusText)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
				.Text(this, &SRenderStudioMainFrame::GetSceneInfoText)
			]
		];
}

FText SRenderStudioMainFrame::GetSceneInfoText() const
{
	const FString RHIName = GDynamicRHI ? FString(GDynamicRHI->GetName()) : FString(TEXT("RHI"));
	AActor* Selected = Scene->GetSelectedObject();
	return FText::Format(LOCTEXT("SceneInfo", "Objects: {0}    Selected: {1}    Unreal Engine 4.27 / {2}"),
		FText::AsNumber(Scene->GetObjects().Num()),
		Selected ? FText::FromString(Scene->GetObjectLabel(Selected)) : LOCTEXT("SelectedNone", "-"),
		FText::FromString(RHIName));
}

void SRenderStudioMainFrame::SetStatus(const FText& Message)
{
	StatusText = Message;
}

void SRenderStudioMainFrame::UpdateTitle()
{
	const FText Title = FText::Format(LOCTEXT("WindowTitle", "{0}{1} - Render Studio"),
		Scene->GetDisplayName(),
		Scene->IsDirty() ? FText::FromString(TEXT("*")) : FText::GetEmpty());

	if (TSharedPtr<SWindow> Window = OwnerWindow.Pin())
	{
		Window->SetTitle(Title);
	}
	if (TSharedPtr<SDockTab> Tab = OwnerTab.Pin())
	{
		Tab->SetLabel(Title);
	}
}

//////////////////////////////////////////////////////////////////////////
// Commands

void SRenderStudioMainFrame::BindCommands()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	FUICommandList& List = *CommandList;

	List.MapAction(Commands.NewScene, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewScene));
	List.MapAction(Commands.OpenScene, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnOpenScene));
	List.MapAction(Commands.SaveScene, FExecuteAction::CreateLambda([this]() { OnSaveScene(); }));
	List.MapAction(Commands.SaveSceneAs, FExecuteAction::CreateLambda([this]() { OnSaveSceneAs(); }));
	List.MapAction(Commands.ImportMesh, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnImportMesh));
	List.MapAction(Commands.ImportEnvironment, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnImportEnvironment));
	List.MapAction(Commands.RenderImage, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnRenderImage));
	List.MapAction(Commands.SaveScreenshot, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnSaveScreenshot));
	List.MapAction(Commands.Exit, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnExit));

	List.MapAction(Commands.Undo, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnUndo), FCanExecuteAction::CreateLambda([this]() { return Scene->CanUndo(); }));
	List.MapAction(Commands.Redo, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnRedo), FCanExecuteAction::CreateLambda([this]() { return Scene->CanRedo(); }));
	List.MapAction(Commands.DuplicateSelected, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnDuplicate), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedObject));
	List.MapAction(Commands.DeleteSelected, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnDelete), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedObject));
	List.MapAction(Commands.FocusSelected, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnFocusSelected));

	auto MapAdd = [this, &List](const TSharedPtr<FUICommandInfo>& Command, TFunction<void()> Action)
	{
		List.MapAction(Command, FExecuteAction::CreateLambda([this, Action]()
		{
			Action();
			SetStatus(FText::Format(LOCTEXT("Added", "Added {0}"), FText::FromString(Scene->GetObjectLabel(Scene->GetSelectedObject()))));
		}));
	};
	MapAdd(Commands.AddCube, [this]() { Scene->AddPrimitive(ERenderStudioPrimitive::Cube); });
	MapAdd(Commands.AddSphere, [this]() { Scene->AddPrimitive(ERenderStudioPrimitive::Sphere); });
	MapAdd(Commands.AddCylinder, [this]() { Scene->AddPrimitive(ERenderStudioPrimitive::Cylinder); });
	MapAdd(Commands.AddCone, [this]() { Scene->AddPrimitive(ERenderStudioPrimitive::Cone); });
	MapAdd(Commands.AddPlane, [this]() { Scene->AddPrimitive(ERenderStudioPrimitive::Plane); });
	MapAdd(Commands.AddPointLight, [this]() { Scene->AddLight(ERenderStudioLight::Point); });
	MapAdd(Commands.AddSpotLight, [this]() { Scene->AddLight(ERenderStudioLight::Spot); });
	MapAdd(Commands.AddRectLight, [this]() { Scene->AddLight(ERenderStudioLight::Rect); });

	List.MapAction(Commands.SelectMode,
		FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::SetWidgetMode, (int32)FWidget::WM_None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SRenderStudioMainFrame::IsWidgetMode, (int32)FWidget::WM_None));
	List.MapAction(Commands.TranslateMode,
		FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::SetWidgetMode, (int32)FWidget::WM_Translate),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SRenderStudioMainFrame::IsWidgetMode, (int32)FWidget::WM_Translate));
	List.MapAction(Commands.RotateMode,
		FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::SetWidgetMode, (int32)FWidget::WM_Rotate),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SRenderStudioMainFrame::IsWidgetMode, (int32)FWidget::WM_Rotate));
	List.MapAction(Commands.ScaleMode,
		FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::SetWidgetMode, (int32)FWidget::WM_Scale),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SRenderStudioMainFrame::IsWidgetMode, (int32)FWidget::WM_Scale));

	List.MapAction(Commands.ToggleGrid,
		FExecuteAction::CreateLambda([]()
		{
			URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();
			Settings->bShowGrid = !Settings->bShowGrid;
			Settings->SaveConfig();
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]() { return GetDefault<URenderStudioSettings>()->bShowGrid; }));
	List.MapAction(Commands.ToggleFloor,
		FExecuteAction::CreateLambda([this]() { Scene->SetFloorVisible(!Scene->IsFloorVisible()); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return Scene->IsFloorVisible(); }));
	List.MapAction(Commands.ToggleEnvironment,
		FExecuteAction::CreateLambda([this]() { Scene->SetEnvironmentVisible(!Scene->IsEnvironmentVisible()); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return Scene->IsEnvironmentVisible(); }));
	List.MapAction(Commands.ToggleTurntable,
		FExecuteAction::CreateLambda([this]() { Scene->SetTurntableEnabled(!Scene->IsTurntableEnabled()); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return Scene->IsTurntableEnabled(); }));
	List.MapAction(Commands.ResetCamera, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnResetCamera));
	List.MapAction(Commands.ResetLayout, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnResetLayout));

	List.MapAction(Commands.ViewPerspective, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Perspective));
	List.MapAction(Commands.ViewFront, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Front));
	List.MapAction(Commands.ViewBack, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Back));
	List.MapAction(Commands.ViewLeft, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Left));
	List.MapAction(Commands.ViewRight, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Right));
	List.MapAction(Commands.ViewTop, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnStandardView, ERenderStudioView::Top));

	for (int32 Index = 0; Index < FRenderStudioScene::NumBookmarks; ++Index)
	{
		List.MapAction(Commands.SaveBookmark[Index], FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnSaveBookmark, Index));
		List.MapAction(Commands.RecallBookmark[Index], FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnRecallBookmark, Index), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasBookmark, Index));
	}

	List.MapAction(Commands.NewMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterial));
	List.MapAction(Commands.NewMaterialGraph, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterialGraph));
	List.MapAction(Commands.NewMaterialInstance, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterialInstance), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedMaterial));
	List.MapAction(Commands.EditMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnEditMaterial), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedMaterial));
	List.MapAction(Commands.AssignMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnAssignMaterial), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::CanAssignMaterial));

	List.MapAction(Commands.About, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnAbout));
}

//////////////////////////////////////////////////////////////////////////
// Document

const void* SRenderStudioMainFrame::GetParentWindowHandle() const
{
	return FSlateApplication::Get().FindBestParentWindowHandleForDialogs(ConstCastSharedRef<SRenderStudioMainFrame>(SharedThis(this)));
}

bool SRenderStudioMainFrame::ConfirmCloseDocument(bool bQuitting)
{
	// Materials edited in the node editor are packages of their own: include them in the question.
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

	if (!Scene->IsDirty() && DirtyPackages.Num() == 0)
	{
		return true;
	}

	const FText Title = LOCTEXT("SaveChangesTitle", "Render Studio");
	const FText Question = Scene->IsDirty()
		? FText::Format(LOCTEXT("SaveChangesPrompt", "Save changes to {0}?"), Scene->GetDisplayName())
		: LOCTEXT("SaveAssetsPrompt", "Save modified materials and imported assets?");
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel, Question, &Title);

	if (Answer == EAppReturnType::Cancel)
	{
		return false;
	}
	if (Answer == EAppReturnType::Yes)
	{
		if (Scene->IsDirty())
		{
			return OnSaveScene();
		}
		SaveModifiedAssets();
		return true;
	}

	if (bQuitting)
	{
		// The editor would otherwise ask a second time while shutting down
		for (UPackage* Package : DirtyPackages)
		{
			Package->SetDirtyFlag(false);
		}
	}
	return true;
}

void SRenderStudioMainFrame::OnNewScene()
{
	if (ConfirmCloseDocument())
	{
		Scene->NewScene();
		SetStatus(LOCTEXT("NewSceneDone", "New scene"));
	}
}

void SRenderStudioMainFrame::OnOpenScene()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform || !ConfirmCloseDocument())
	{
		return;
	}

	TArray<FString> Files;
	const FString& Directory = GetDefault<URenderStudioSettings>()->LastSceneDirectory;
	if (DesktopPlatform->OpenFileDialog(GetParentWindowHandle(), LOCTEXT("OpenSceneTitle", "Open Scene").ToString(), Directory, TEXT(""),
		RenderStudioMainFrame::SceneFileTypes, EFileDialogFlags::None, Files) && Files.Num() > 0)
	{
		OpenSceneFile(Files[0]);
	}
}

bool SRenderStudioMainFrame::OpenSceneFile(const FString& Filename)
{
	FText Error;
	if (!Scene->LoadScene(Filename, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		SetStatus(Error);
		return false;
	}

	GetMutableDefault<URenderStudioSettings>()->AddRecentScene(Filename);
	SetStatus(FText::Format(LOCTEXT("OpenedScene", "Opened {0}"), FText::FromString(Filename)));
	return true;
}

bool SRenderStudioMainFrame::OnSaveScene()
{
	if (Scene->GetFilename().IsEmpty())
	{
		return OnSaveSceneAs();
	}

	// Materials, instances and imported assets the scene points at must exist on disk too
	SaveModifiedAssets();

	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Scene->SetSceneCamera(Client->GetCamera());
	}

	FText Error;
	if (!Scene->SaveScene(Scene->GetFilename(), Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		SetStatus(Error);
		return false;
	}

	GetMutableDefault<URenderStudioSettings>()->AddRecentScene(Scene->GetFilename());
	SetStatus(FText::Format(LOCTEXT("SavedScene", "Saved {0}"), FText::FromString(Scene->GetFilename())));
	return true;
}

bool SRenderStudioMainFrame::OnSaveSceneAs()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return false;
	}

	const URenderStudioSettings* Settings = GetDefault<URenderStudioSettings>();
	const FString Directory = Scene->GetFilename().IsEmpty() ? Settings->LastSceneDirectory : FPaths::GetPath(Scene->GetFilename());
	const FString DefaultName = Scene->GetFilename().IsEmpty() ? FString(TEXT("Scene.rss")) : FPaths::GetCleanFilename(Scene->GetFilename());

	TArray<FString> Files;
	if (!DesktopPlatform->SaveFileDialog(GetParentWindowHandle(), LOCTEXT("SaveSceneTitle", "Save Scene As").ToString(), Directory, DefaultName,
		RenderStudioMainFrame::SceneFileTypes, EFileDialogFlags::None, Files) || Files.Num() == 0)
	{
		return false;
	}

	FString Filename = Files[0];
	if (FPaths::GetExtension(Filename).IsEmpty())
	{
		Filename += TEXT(".rss");
	}

	SaveModifiedAssets();
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Scene->SetSceneCamera(Client->GetCamera());
	}

	FText Error;
	if (!Scene->SaveScene(Filename, Error))
	{
		FMessageDialog::Open(EAppMsgType::Ok, Error);
		SetStatus(Error);
		return false;
	}

	GetMutableDefault<URenderStudioSettings>()->AddRecentScene(Filename);
	SetStatus(FText::Format(LOCTEXT("SavedScene", "Saved {0}"), FText::FromString(Filename)));
	return true;
}

void SRenderStudioMainFrame::SaveModifiedAssets()
{
	const bool bPromptUserToSave = false;
	const bool bSaveMapPackages = false;
	const bool bSaveContentPackages = true;
	FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages);
}

void SRenderStudioMainFrame::OnSceneLoaded()
{
	FRenderStudioViewportClient* Client = GetViewportClient();
	if (!Client)
	{
		return;
	}

	if (Scene->GetSceneCamera().bIsSet)
	{
		Client->SetCamera(Scene->GetSceneCamera());
	}
	else
	{
		Client->FrameScene(true);
	}
}

//////////////////////////////////////////////////////////////////////////
// Import / output

void SRenderStudioMainFrame::OnImportMesh()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> Files;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		GetParentWindowHandle(),
		LOCTEXT("ImportDialogTitle", "Import Mesh").ToString(),
		FPaths::GetPath(FPaths::ProjectDir()),
		TEXT(""),
		TEXT("Mesh files (*.fbx;*.obj)|*.fbx;*.obj|FBX (*.fbx)|*.fbx|Wavefront OBJ (*.obj)|*.obj"),
		EFileDialogFlags::Multiple,
		Files);

	if (bOpened && Files.Num() > 0)
	{
		const int32 NumMeshes = Scene->ImportMeshes(Files);
		OnResetCamera();
		SetStatus(FText::Format(LOCTEXT("ImportDone", "Imported {0} mesh(es) from {1} file(s)"), FText::AsNumber(NumMeshes), FText::AsNumber(Files.Num())));
	}
}

void SRenderStudioMainFrame::OnImportEnvironment()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> Files;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		GetParentWindowHandle(),
		LOCTEXT("ImportHDRITitle", "Import HDRI").ToString(),
		FPaths::GetPath(FPaths::ProjectDir()),
		TEXT(""),
		TEXT("Radiance HDR panorama (*.hdr)|*.hdr"),
		EFileDialogFlags::Multiple,
		Files);

	if (bOpened && Files.Num() > 0)
	{
		const int32 NumImported = Scene->ImportEnvironmentMaps(Files);
		SetStatus(NumImported > 0
			? FText::Format(LOCTEXT("HDRIImported", "Imported {0} HDRI(s); the sky now uses {1}"), FText::AsNumber(NumImported), FText::FromString(FPaths::GetBaseFilename(Files[0])))
			: LOCTEXT("HDRIImportFailed", "No cubemap could be created from the selected file(s). Use long-lat (equirectangular) .hdr panoramas."));
	}
}

void SRenderStudioMainFrame::OnRenderImage()
{
	FRenderStudioViewportClient* Client = GetViewportClient();
	if (!Client)
	{
		return;
	}

	TSharedRef<SWindow> DialogWindow = SNew(SWindow)
		.Title(LOCTEXT("RenderDialogTitle", "Render"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SRenderStudioRenderDialog> Dialog = SNew(SRenderStudioRenderDialog, Scene.ToSharedRef(), Client)
		.ParentWindow(DialogWindow);
	DialogWindow->SetContent(Dialog);

	FSlateApplication::Get().AddModalWindow(DialogWindow, SharedThis(this));

	const FString Output = Dialog->GetLastOutput();
	if (!Output.IsEmpty())
	{
		SetStatus(FText::Format(LOCTEXT("RenderDone", "Rendered {0}"), FText::FromString(Output)));
	}
}

void SRenderStudioMainFrame::OnSaveScreenshot()
{
	FRenderStudioViewportClient* Client = GetViewportClient();
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!Client || !DesktopPlatform)
	{
		return;
	}

	const FString Directory = GetDefault<URenderStudioSettings>()->GetScreenshotDirectory();
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString DefaultName = FString::Printf(TEXT("RenderStudio_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));

	TArray<FString> Files;
	if (!DesktopPlatform->SaveFileDialog(GetParentWindowHandle(), LOCTEXT("ScreenshotDialogTitle", "Save Screenshot").ToString(), Directory, DefaultName, TEXT("PNG image (*.png)|*.png"), EFileDialogFlags::None, Files) || Files.Num() == 0)
	{
		return;
	}

	const FString Written = Client->SaveScreenshot(Files[0]);
	SetStatus(Written.IsEmpty()
		? LOCTEXT("ScreenshotFailed", "Screenshot failed")
		: FText::Format(LOCTEXT("ScreenshotDone", "Screenshot saved to {0}"), FText::FromString(Written)));
}

void SRenderStudioMainFrame::OnExit()
{
	if (!ConfirmCloseDocument(OnExitRequested.IsBound()))
	{
		return;
	}

	if (OnExitRequested.IsBound())
	{
		OnExitRequested.Execute();
	}
	else if (TSharedPtr<SDockTab> Tab = OwnerTab.Pin())
	{
		Tab->RequestCloseTab();
	}
}

//////////////////////////////////////////////////////////////////////////
// Edit

void SRenderStudioMainFrame::OnUndo()
{
	const FText Description = Scene->GetUndoDescription();
	Scene->Undo();
	SetStatus(FText::Format(LOCTEXT("Undone", "Undo: {0}"), Description));
}

void SRenderStudioMainFrame::OnRedo()
{
	const FText Description = Scene->GetRedoDescription();
	Scene->Redo();
	SetStatus(FText::Format(LOCTEXT("Redone", "Redo: {0}"), Description));
}

FText SRenderStudioMainFrame::GetUndoLabel() const
{
	return Scene->CanUndo() ? FText::Format(LOCTEXT("UndoLabel", "Undo {0}"), Scene->GetUndoDescription()) : LOCTEXT("UndoLabelEmpty", "Undo");
}

FText SRenderStudioMainFrame::GetRedoLabel() const
{
	return Scene->CanRedo() ? FText::Format(LOCTEXT("RedoLabel", "Redo {0}"), Scene->GetRedoDescription()) : LOCTEXT("RedoLabelEmpty", "Redo");
}

void SRenderStudioMainFrame::OnDuplicate()
{
	if (AActor* NewActor = Scene->DuplicateObject(Scene->GetSelectedObject()))
	{
		SetStatus(FText::Format(LOCTEXT("Duplicated", "Duplicated as {0}"), FText::FromString(Scene->GetObjectLabel(NewActor))));
	}
}

void SRenderStudioMainFrame::OnDelete()
{
	if (AActor* Actor = Scene->GetSelectedObject())
	{
		const FString Label = Scene->GetObjectLabel(Actor);
		Scene->DeleteObject(Actor);
		SetStatus(FText::Format(LOCTEXT("Deleted", "Deleted {0}"), FText::FromString(Label)));
	}
}

//////////////////////////////////////////////////////////////////////////
// Camera

void SRenderStudioMainFrame::OnFocusSelected()
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Client->FrameSelection();
	}
}

void SRenderStudioMainFrame::OnResetCamera()
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Client->FrameScene();
	}
}

void SRenderStudioMainFrame::OnStandardView(ERenderStudioView View)
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Client->SetStandardView(View);
	}
}

void SRenderStudioMainFrame::OnSaveBookmark(int32 Index)
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Scene->SetBookmark(Index, Client->GetCamera());
		SetStatus(FText::Format(LOCTEXT("BookmarkStored", "Camera {0} stored (press {0} to jump back)"), FText::AsNumber(Index + 1)));
	}
}

void SRenderStudioMainFrame::OnRecallBookmark(int32 Index)
{
	FRenderStudioViewportClient* Client = GetViewportClient();
	if (Client && HasBookmark(Index))
	{
		Client->SetCamera(Scene->GetBookmark(Index));
		SetStatus(FText::Format(LOCTEXT("BookmarkRecalled", "Camera {0}"), FText::AsNumber(Index + 1)));
	}
}

bool SRenderStudioMainFrame::HasBookmark(int32 Index) const
{
	return Scene->GetBookmark(Index).bIsSet;
}

void SRenderStudioMainFrame::OnResetLayout()
{
	TabManager->CloseAllAreas();
	RestoreLayout(MakeDefaultLayout());
	SetStatus(LOCTEXT("LayoutReset", "Layout reset"));
}

//////////////////////////////////////////////////////////////////////////
// Materials

void SRenderStudioMainFrame::AssignToSelection(UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}

	if (AActor* Actor = Scene->GetSelectedObject())
	{
		Scene->AssignMaterial(Actor, Material);
	}
	Scene->SetSelectedMaterial(Material);
}

void SRenderStudioMainFrame::OnNewMaterial()
{
	// New materials are instances of the shared Render Studio PBR material: live slider feedback,
	// no shader compile per material. "New Material Graph" gives a material its own node graph.
	UMaterial* Master = Scene->GetOrCreateMasterMaterial();
	UMaterialInstanceConstant* Instance = Master ? Scene->CreateMaterialInstance(Master, TEXT("Material")) : nullptr;
	if (!Instance)
	{
		SetStatus(LOCTEXT("NewMaterialFailed", "Could not create the material (is the content folder writable?)"));
		return;
	}

	AssignToSelection(Instance);
	SetStatus(FText::Format(LOCTEXT("NewMaterialDone", "Created {0}"), FText::FromString(Instance->GetName())));
}

void SRenderStudioMainFrame::OnNewMaterialGraph()
{
	UMaterial* BaseMaterial = Scene->CreateMaterial();
	if (!BaseMaterial)
	{
		SetStatus(LOCTEXT("NewMaterialFailed", "Could not create the material (is the content folder writable?)"));
		return;
	}

	UMaterialInterface* ToAssign = BaseMaterial;
	if (UMaterialInstanceConstant* Instance = Scene->CreateMaterialInstance(BaseMaterial))
	{
		ToAssign = Instance;
	}

	AssignToSelection(ToAssign);
	SetStatus(FText::Format(LOCTEXT("NewMaterialGraphDone", "Created {0} with its own graph ({1})"), FText::FromString(ToAssign->GetName()), FText::FromString(BaseMaterial->GetName())));
}

void SRenderStudioMainFrame::OnNewMaterialFromPreset(int32 PresetIndex)
{
	const TArray<FRenderStudioMaterialPreset>& Presets = FRenderStudioScene::GetMaterialPresets();
	if (!Presets.IsValidIndex(PresetIndex))
	{
		return;
	}

	if (UMaterialInstanceConstant* Instance = Scene->CreateMaterialFromPreset(Presets[PresetIndex]))
	{
		AssignToSelection(Instance);
		SetStatus(FText::Format(LOCTEXT("PresetMaterialDone", "Created {0}"), FText::FromString(Instance->GetName())));
	}
}

void SRenderStudioMainFrame::OnApplyPreset(int32 PresetIndex)
{
	const TArray<FRenderStudioMaterialPreset>& Presets = FRenderStudioScene::GetMaterialPresets();
	UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Scene->GetSelectedMaterial());
	if (Instance && Presets.IsValidIndex(PresetIndex))
	{
		Scene->ApplyPreset(Instance, Presets[PresetIndex]);
		Scene->MarkDirty();
		SetStatus(FText::Format(LOCTEXT("PresetApplied", "Applied {0} to {1}"), FText::FromString(Presets[PresetIndex].Name), FText::FromString(Instance->GetName())));
	}
}

void SRenderStudioMainFrame::OnNewMaterialInstance()
{
	if (UMaterialInstanceConstant* Instance = Scene->CreateMaterialInstance(Scene->GetSelectedMaterial()))
	{
		SetStatus(FText::Format(LOCTEXT("NewInstanceDone", "Created {0}"), FText::FromString(Instance->GetName())));
	}
}

void SRenderStudioMainFrame::OnEditMaterial()
{
	UMaterialInterface* Material = Scene->GetSelectedMaterial();
	if (!Material)
	{
		return;
	}

	// Always open the node graph of the base material, which is what artists expect from "edit"
	UMaterial* BaseMaterial = Material->GetMaterial();
	Scene->OpenMaterialEditor(BaseMaterial ? static_cast<UMaterialInterface*>(BaseMaterial) : Material);

	FText Hint = LOCTEXT("EditMaterialHint", "press Apply there to see changes here");
	if (BaseMaterial && BaseMaterial->GetName() == TEXT("M_StudioPBR"))
	{
		Hint = LOCTEXT("EditSharedMaterialHint", "this graph is shared by every Render Studio material; use New Material Graph for a private one");
	}
	SetStatus(FText::Format(LOCTEXT("EditMaterialDone", "Opened {0} in the material editor ({1})"), FText::FromString(BaseMaterial ? BaseMaterial->GetName() : Material->GetName()), Hint));
}

void SRenderStudioMainFrame::OnAssignMaterial()
{
	Scene->AssignMaterial(Scene->GetSelectedObject(), Scene->GetSelectedMaterial());
	SetStatus(FText::Format(LOCTEXT("AssignDone", "Assigned {0} to {1}"),
		FText::FromString(Scene->GetSelectedMaterial()->GetName()),
		FText::FromString(Scene->GetObjectLabel(Scene->GetSelectedObject()))));
}

//////////////////////////////////////////////////////////////////////////
// Help

void SRenderStudioMainFrame::OnShowControls()
{
	const FText Title = LOCTEXT("ControlsTitle", "Render Studio Controls");
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("ControlsText",
		"Viewport\n"
		"  Left mouse: orbit    Middle mouse: pan    Right mouse / wheel: zoom\n"
		"  Click: select    F: frame selection    Home: frame scene\n"
		"  Q / W / E / R: select / move / rotate / scale\n"
		"  L + drag: move the sun    K + drag: spin the sky\n"
		"  Numpad 0 / 1 / 3 / 7: perspective / front / right / top\n"
		"  Ctrl+1..4: store camera    1..4: recall camera\n\n"
		"Drag a material from the Materials panel onto an object to assign it.\n"
		"Drop .fbx / .obj files on the viewport to import them, .hdr files to use them as the sky.\n\n"
		"Ctrl+Z / Ctrl+Y: undo / redo    Ctrl+S: save    F9: render    F12: screenshot"),
		&Title);
}

void SRenderStudioMainFrame::OnAbout()
{
	const FText Title = LOCTEXT("AboutTitle", "About Render Studio");
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AboutText",
		"Render Studio 0.2\n\n"
		"A look-dev and presentation tool built on the Unreal Engine 4.27 renderer and material editor."),
		&Title);
}

bool SRenderStudioMainFrame::HasSelectedObject() const
{
	return Scene->GetSelectedObject() != nullptr;
}

bool SRenderStudioMainFrame::HasSelectedMaterial() const
{
	return Scene->GetSelectedMaterial() != nullptr;
}

bool SRenderStudioMainFrame::HasSelectedInstance() const
{
	UMaterialInterface* Material = Scene->GetSelectedMaterial();
	return Material && Material->IsA<UMaterialInstanceConstant>() && Material->GetPathName().StartsWith(TEXT("/Game/"));
}

bool SRenderStudioMainFrame::CanAssignMaterial() const
{
	AActor* Actor = Scene->GetSelectedObject();
	return Actor && Scene->GetSelectedMaterial() && Actor->FindComponentByClass<UMeshComponent>() != nullptr;
}

void SRenderStudioMainFrame::SetWidgetMode(int32 Mode)
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		Client->SetWidgetMode(static_cast<FWidget::EWidgetMode>(Mode));
		Client->Invalidate();
	}
}

bool SRenderStudioMainFrame::IsWidgetMode(int32 Mode) const
{
	if (FRenderStudioViewportClient* Client = GetViewportClient())
	{
		// Ask the base class: our override reports "none" while nothing is selected
		return Client->FEditorViewportClient::GetWidgetMode() == static_cast<FWidget::EWidgetMode>(Mode);
	}
	return false;
}

void SRenderStudioMainFrame::OnSceneSelectionChanged()
{
	AActor* Actor = Scene->GetSelectedObject();
	if (PropertiesView.IsValid())
	{
		PropertiesView->SetObject(Actor ? static_cast<UObject*>(Actor->GetRootComponent()) : nullptr);
	}
}

FRenderStudioViewportClient* SRenderStudioMainFrame::GetViewportClient() const
{
	TSharedPtr<SRenderStudioViewport> Viewport = ViewportWidget.Pin();
	return Viewport.IsValid() ? Viewport->GetRenderStudioClient().Get() : nullptr;
}

#undef LOCTEXT_NAMESPACE
