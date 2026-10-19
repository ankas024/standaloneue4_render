#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FMenuBuilder;
class FRenderStudioScene;
class FRenderStudioViewportClient;
class FUICommandList;
class IDetailsView;
class SBox;
class SDockTab;
class SRenderStudioViewport;
class SWindow;
struct FRenderStudioMaterialPreset;
enum class ERenderStudioView : uint8;

/**
 * The application window content: menu bar, main toolbar, the dockable panels
 * (Scene, Properties, Viewport, Materials, Environment) and the status bar.
 * Also owns the document workflow (new / open / save, unsaved-changes prompts).
 */
class SRenderStudioMainFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioMainFrame) {}
		/** Called when File > Exit is chosen (after unsaved changes were dealt with) */
		SLATE_EVENT(FSimpleDelegate, OnExitRequested)
	SLATE_END_ARGS()

	virtual ~SRenderStudioMainFrame();

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InOwnerTab, const TSharedPtr<SWindow>& InOwnerWindow);

	/**
	 * Asks to save unsaved changes (the scene and modified materials / imported assets).
	 * Returns false if the user cancelled. When quitting, "Don't Save" also discards modified assets.
	 */
	bool ConfirmCloseDocument(bool bQuitting = false);

	/** Opens a scene file (used for the command line and the recent files list). */
	bool OpenSceneFile(const FString& Filename);

	// SWidget
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	static const FName ViewportTabId;
	static const FName OutlinerTabId;
	static const FName PropertiesTabId;
	static const FName MaterialsTabId;
	static const FName EnvironmentTabId;

private:
	void BindCommands();
	void RegisterTabSpawners();
	TSharedRef<FTabManager::FLayout> MakeDefaultLayout() const;
	void RestoreLayout(const TSharedRef<FTabManager::FLayout>& Layout);

	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnOutlinerTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPropertiesTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnMaterialsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnEnvironmentTab(const FSpawnTabArgs& Args);

	TSharedRef<SWidget> MakeMenuBar();
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeStatusBar();
	void FillFileMenu(FMenuBuilder& MenuBuilder);
	void FillRecentScenesMenu(FMenuBuilder& MenuBuilder);
	void FillEditMenu(FMenuBuilder& MenuBuilder);
	void FillCreateMenu(FMenuBuilder& MenuBuilder);
	void FillViewMenu(FMenuBuilder& MenuBuilder);
	void FillCameraBookmarksMenu(FMenuBuilder& MenuBuilder);
	void FillEnvironmentMenu(FMenuBuilder& MenuBuilder);
	void FillSkyMenu(FMenuBuilder& MenuBuilder);
	void FillMaterialMenu(FMenuBuilder& MenuBuilder);
	void FillPresetMenu(FMenuBuilder& MenuBuilder, bool bApplyToSelected);
	void FillWindowMenu(FMenuBuilder& MenuBuilder);
	void FillHelpMenu(FMenuBuilder& MenuBuilder);

	// Command handlers
	void OnNewScene();
	void OnOpenScene();
	bool OnSaveScene();
	bool OnSaveSceneAs();
	void OnImportMesh();
	void OnImportEnvironment();
	void OnRenderImage();
	void OnSaveScreenshot();
	void OnExit();
	void OnUndo();
	void OnRedo();
	void OnDuplicate();
	void OnDelete();
	void OnFocusSelected();
	void OnResetCamera();
	void OnResetLayout();
	void OnStandardView(ERenderStudioView View);
	void OnSaveBookmark(int32 Index);
	void OnRecallBookmark(int32 Index);
	bool HasBookmark(int32 Index) const;
	void OnNewMaterial();
	void OnNewMaterialGraph();
	void OnNewMaterialFromPreset(int32 PresetIndex);
	void OnApplyPreset(int32 PresetIndex);
	void OnNewMaterialInstance();
	void OnEditMaterial();
	void OnAssignMaterial();
	void OnShowControls();
	void OnRunSelfTest();
	bool OnSelfTestTick(float DeltaTime);
	void OnAbout();
	bool HasSelectedObject() const;
	bool HasSelectedMaterial() const;
	bool HasSelectedInstance() const;
	bool CanAssignMaterial() const;
	FText GetUndoLabel() const;
	FText GetRedoLabel() const;

	void SetWidgetMode(int32 Mode);
	bool IsWidgetMode(int32 Mode) const;

	/** Saves dirty material / mesh / HDRI packages without prompting. */
	void SaveModifiedAssets();
	void AssignToSelection(class UMaterialInterface* Material);

	void OnSceneSelectionChanged();
	void OnSceneLoaded();
	void UpdateTitle();
	void SetStatus(const FText& Message);
	FText GetStatusText() const { return StatusText; }
	FText GetSceneInfoText() const;

	FRenderStudioViewportClient* GetViewportClient() const;
	const void* GetParentWindowHandle() const;

	TSharedPtr<FRenderStudioScene> Scene;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FTabManager> TabManager;
	TWeakPtr<SDockTab> OwnerTab;
	TWeakPtr<SWindow> OwnerWindow;
	TSharedPtr<SBox> DockingAreaHost;
	TWeakPtr<SRenderStudioViewport> ViewportWidget;
	TSharedPtr<IDetailsView> PropertiesView;
	FSimpleDelegate OnExitRequested;
	FText StatusText;
};
