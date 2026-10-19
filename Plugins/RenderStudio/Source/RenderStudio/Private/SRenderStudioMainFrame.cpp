#include "SRenderStudioMainFrame.h"

#include "Components/MeshComponent.h"
#include "DesktopPlatformModule.h"
#include "DynamicRHI.h"
#include "EditorStyleSet.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/LayoutService.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "IDesktopPlatform.h"
#include "IDetailsView.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
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
	}

	if (TabManager.IsValid())
	{
		FLayoutSaveRestore::SaveToConfig(GEditorLayoutIni, TabManager->PersistLayout());
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
	return FTabManager::NewLayout("RenderStudio_Layout_v1")
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
	// the application settings (camera FOV, grid, screenshots) are shown underneath.
	return SNew(SDockTab)
		.Label(LOCTEXT("EnvironmentTab", "Environment & Render"))
		.TabRole(ETabRole::PanelTab)
		[
			SNew(SRenderStudioPanel)
			.Title(LOCTEXT("EnvironmentTitle", "Environment, Lighting & Post FX"))
			[
				SNew(SAdvancedPreviewDetailsTab, Scene->GetPreviewScene())
				.AdditionalSettings(Scene->GetSettings())
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
	MenuBuilder.AddMenuEntry(Commands.ImportMesh);
	MenuBuilder.AddMenuEntry(Commands.SaveAll);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Output", LOCTEXT("OutputSection", "Output"));
	MenuBuilder.AddMenuEntry(Commands.SaveScreenshot);
	MenuBuilder.EndSection();
	MenuBuilder.BeginSection("Exit", FText::GetEmpty());
	MenuBuilder.AddMenuEntry(Commands.Exit);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillEditMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
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
	MenuBuilder.BeginSection("Camera", LOCTEXT("CameraSection", "Camera"));
	MenuBuilder.AddMenuEntry(Commands.ResetCamera);
	MenuBuilder.AddMenuEntry(Commands.FocusSelected);
	MenuBuilder.EndSection();
}

void SRenderStudioMainFrame::FillMaterialMenu(FMenuBuilder& MenuBuilder)
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	MenuBuilder.AddMenuEntry(Commands.NewMaterial);
	MenuBuilder.AddMenuEntry(Commands.NewMaterialInstance);
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(Commands.AssignMaterial);
	MenuBuilder.AddMenuEntry(Commands.EditMaterial);
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
	Toolbar.AddToolBarButton(Commands.ImportMesh, NAME_None, LOCTEXT("TB_Import", "Import"), TAttribute<FText>(), FSlateIcon(EditorStyle, "LevelEditor.OpenLevel"));
	Toolbar.AddToolBarButton(Commands.SaveAll, NAME_None, LOCTEXT("TB_Save", "Save"), TAttribute<FText>(), FSlateIcon(EditorStyle, "AssetEditor.SaveAsset"));
	Toolbar.AddToolBarButton(Commands.SaveScreenshot, NAME_None, LOCTEXT("TB_Shot", "Screenshot"), TAttribute<FText>(), FSlateIcon(EditorStyle, "HighresScreenshot.Capture"));
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
	return FText::Format(LOCTEXT("SceneInfo", "Objects: {0}    Materials: {1}    Unreal Engine 4.27 / {2}"),
		FText::AsNumber(Scene->GetObjects().Num()),
		FText::AsNumber(Scene->GetMaterials().Num()),
		FText::FromString(RHIName));
}

void SRenderStudioMainFrame::SetStatus(const FText& Message)
{
	StatusText = Message;
}

//////////////////////////////////////////////////////////////////////////
// Commands

void SRenderStudioMainFrame::BindCommands()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	FUICommandList& List = *CommandList;

	List.MapAction(Commands.NewScene, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewScene));
	List.MapAction(Commands.ImportMesh, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnImportMesh));
	List.MapAction(Commands.SaveAll, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnSaveAll));
	List.MapAction(Commands.SaveScreenshot, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnSaveScreenshot));
	List.MapAction(Commands.Exit, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnExit));

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

	List.MapAction(Commands.NewMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterial));
	List.MapAction(Commands.NewMaterialInstance, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnNewMaterialInstance), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedMaterial));
	List.MapAction(Commands.EditMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnEditMaterial), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::HasSelectedMaterial));
	List.MapAction(Commands.AssignMaterial, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnAssignMaterial), FCanExecuteAction::CreateSP(this, &SRenderStudioMainFrame::CanAssignMaterial));

	List.MapAction(Commands.About, FExecuteAction::CreateSP(this, &SRenderStudioMainFrame::OnAbout));
}

void SRenderStudioMainFrame::OnNewScene()
{
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("ConfirmNewScene", "Clear the scene and start again from the shader ball?\n\nMaterials you created stay in the library."));
	if (Answer == EAppReturnType::Yes)
	{
		Scene->ResetToDefault();
		OnResetCamera();
		SetStatus(LOCTEXT("NewSceneDone", "New scene"));
	}
}

void SRenderStudioMainFrame::OnImportMesh()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform)
	{
		return;
	}

	TArray<FString> Files;
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
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

void SRenderStudioMainFrame::OnSaveAll()
{
	const bool bPromptUserToSave = true;
	const bool bSaveMapPackages = false;
	const bool bSaveContentPackages = true;
	FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages);
	SetStatus(LOCTEXT("Saved", "Saved"));
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
	const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
	if (!DesktopPlatform->SaveFileDialog(ParentWindowHandle, LOCTEXT("ScreenshotDialogTitle", "Save Screenshot").ToString(), Directory, DefaultName, TEXT("PNG image (*.png)|*.png"), EFileDialogFlags::None, Files) || Files.Num() == 0)
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
	if (OnExitRequested.IsBound())
	{
		OnExitRequested.Execute();
	}
	else if (TSharedPtr<SDockTab> Tab = OwnerTab.Pin())
	{
		Tab->RequestCloseTab();
	}
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

void SRenderStudioMainFrame::OnResetLayout()
{
	TabManager->CloseAllAreas();
	RestoreLayout(MakeDefaultLayout());
	SetStatus(LOCTEXT("LayoutReset", "Layout reset"));
}

void SRenderStudioMainFrame::OnNewMaterial()
{
	// New materials come as a base graph + an instance: the instance gives live slider
	// feedback, the base graph is what "Node Editor" opens.
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

	if (AActor* Actor = Scene->GetSelectedObject())
	{
		Scene->AssignMaterial(Actor, ToAssign);
	}
	Scene->SetSelectedMaterial(ToAssign);

	SetStatus(FText::Format(LOCTEXT("NewMaterialDone", "Created {0}"), FText::FromString(ToAssign->GetName())));
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
	SetStatus(FText::Format(LOCTEXT("EditMaterialDone", "Opened {0} in the material editor (press Apply there to see changes here)"), FText::FromString(BaseMaterial ? BaseMaterial->GetName() : Material->GetName())));
}

void SRenderStudioMainFrame::OnAssignMaterial()
{
	Scene->AssignMaterial(Scene->GetSelectedObject(), Scene->GetSelectedMaterial());
	SetStatus(FText::Format(LOCTEXT("AssignDone", "Assigned {0} to {1}"),
		FText::FromString(Scene->GetSelectedMaterial()->GetName()),
		FText::FromString(Scene->GetObjectLabel(Scene->GetSelectedObject()))));
}

void SRenderStudioMainFrame::OnAbout()
{
	FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("AboutText",
		"Render Studio 0.1\n\n"
		"A look-dev and presentation tool built on the Unreal Engine 4.27 renderer and material editor.\n\n"
		"Navigation: LMB orbit, MMB pan, RMB / wheel zoom, F frame selection.\n"
		"Hold L and drag to move the sun, K and drag to spin the sky."));
}

bool SRenderStudioMainFrame::HasSelectedObject() const
{
	return Scene->GetSelectedObject() != nullptr;
}

bool SRenderStudioMainFrame::HasSelectedMaterial() const
{
	return Scene->GetSelectedMaterial() != nullptr;
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
