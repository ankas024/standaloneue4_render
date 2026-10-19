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

/**
 * The application window content: menu bar, main toolbar, the dockable panels
 * (Scene, Properties, Viewport, Materials, Environment) and the status bar.
 */
class SRenderStudioMainFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioMainFrame) {}
		/** Called when File > Exit is chosen */
		SLATE_EVENT(FSimpleDelegate, OnExitRequested)
	SLATE_END_ARGS()

	virtual ~SRenderStudioMainFrame();

	void Construct(const FArguments& InArgs, const TSharedRef<SDockTab>& InOwnerTab, const TSharedPtr<SWindow>& InOwnerWindow);

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
	void FillEditMenu(FMenuBuilder& MenuBuilder);
	void FillCreateMenu(FMenuBuilder& MenuBuilder);
	void FillViewMenu(FMenuBuilder& MenuBuilder);
	void FillMaterialMenu(FMenuBuilder& MenuBuilder);
	void FillWindowMenu(FMenuBuilder& MenuBuilder);
	void FillHelpMenu(FMenuBuilder& MenuBuilder);

	// Command handlers
	void OnNewScene();
	void OnImportMesh();
	void OnSaveAll();
	void OnSaveScreenshot();
	void OnExit();
	void OnDuplicate();
	void OnDelete();
	void OnFocusSelected();
	void OnResetCamera();
	void OnResetLayout();
	void OnNewMaterial();
	void OnNewMaterialInstance();
	void OnEditMaterial();
	void OnAssignMaterial();
	void OnAbout();
	bool HasSelectedObject() const;
	bool HasSelectedMaterial() const;
	bool CanAssignMaterial() const;

	void SetWidgetMode(int32 Mode);
	bool IsWidgetMode(int32 Mode) const;

	void OnSceneSelectionChanged();
	void SetStatus(const FText& Message);
	FText GetStatusText() const { return StatusText; }
	FText GetSceneInfoText() const;

	FRenderStudioViewportClient* GetViewportClient() const;

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
