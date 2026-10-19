#include "RenderStudioModule.h"

#include "Classes/EditorStyleSettings.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "RenderStudioCommands.h"
#include "RenderStudioStyle.h"
#include "SRenderStudioMainFrame.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWindow.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#ifndef RENDERSTUDIO_STANDALONE
	#define RENDERSTUDIO_STANDALONE 0
#endif

#define LOCTEXT_NAMESPACE "FRenderStudioModule"

const FName FRenderStudioModule::AppTabId(TEXT("RenderStudioApp"));

void FRenderStudioModule::StartupModule()
{
	FRenderStudioStyle::Initialize();
	FRenderStudioCommands::Register();

	if (!IsAppMode())
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AppTabId, FOnSpawnTab::CreateRaw(this, &FRenderStudioModule::SpawnAppTab))
			.SetDisplayName(LOCTEXT("AppTabTitle", "Render Studio"))
			.SetTooltipText(LOCTEXT("AppTabTooltip", "Open Render Studio, the look-dev / rendering workspace."))
			.SetIcon(FSlateIcon(FRenderStudioStyle::GetStyleSetName(), "RenderStudio.AppIcon.Small"))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
		return;
	}

	// Application mode: the level editor is created (it owns the engine loop, asset tools and the
	// material editor windows) but its window is never shown.
	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrame.IsWindowInitialized())
	{
		OnMainFrameCreated(MainFrame.GetParentWindow(), false);
	}
	else
	{
		MainFrame.EnableDelayedShowMainFrame();
		MainFrameCreatedHandle = MainFrame.OnMainFrameCreationFinished().AddRaw(this, &FRenderStudioModule::OnMainFrameCreated);
	}
}

void FRenderStudioModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame").OnMainFrameCreationFinished().Remove(MainFrameCreatedHandle);
	}

	if (FSlateApplication::IsInitialized() && !IsAppMode())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AppTabId);
	}

	AppMainFrame.Reset();
	AppWindow.Reset();

	FRenderStudioCommands::Unregister();
	FRenderStudioStyle::Shutdown();
}

bool FRenderStudioModule::IsAppMode()
{
	return RENDERSTUDIO_STANDALONE || FParse::Param(FCommandLine::Get(), TEXT("RenderStudio"));
}

void FRenderStudioModule::OnMainFrameCreated(TSharedPtr<SWindow> RootWindow, bool bIsNewProjectWindow)
{
	if (bIsNewProjectWindow || AppWindow.IsValid())
	{
		return;
	}

	if (RootWindow.IsValid() && RootWindow->IsVisible())
	{
		RootWindow->HideWindow();
	}

	// With the main window hidden, the UE4 material editor must open in a window of its own.
	GetMutableDefault<UEditorStyleSettings>()->AssetEditorOpenLocation = EAssetEditorOpenLocation::NewWindow;

	OpenAppWindow();
}

void FRenderStudioModule::OpenAppWindow()
{
	const FSlateRect WorkArea = FSlateApplicationBase::Get().GetPreferredWorkArea();
	const FVector2D ClientSize(
		FMath::Max(1024.0f, (WorkArea.Right - WorkArea.Left) * 0.85f),
		FMath::Max(700.0f, (WorkArea.Bottom - WorkArea.Top) * 0.85f));

	AppWindow = SNew(SWindow)
		.Title(LOCTEXT("AppWindowTitle", "Render Studio"))
		.ClientSize(ClientSize)
		.AutoCenter(EAutoCenter::PreferredWorkArea)
		.SizingRule(ESizingRule::UserSized)
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	AppWindow->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride::CreateRaw(this, &FRenderStudioModule::OnAppWindowCloseRequested));

	// The panels are docked by a tab manager, which needs an owning tab inside this window.
	// It is never shown: it only anchors the docking area (and floating panels) to the window.
	TSharedRef<SDockTab> AnchorTab = SNew(SDockTab).TabRole(ETabRole::MajorTab);

	AppMainFrame = SNew(SRenderStudioMainFrame, AnchorTab, AppWindow)
		.OnExitRequested(FSimpleDelegate::CreateRaw(this, &FRenderStudioModule::OnExitRequested));

	AppWindow->SetContent(
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			AppMainFrame.ToSharedRef()
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(0.0f)
			.HeightOverride(0.0f)
			.Clipping(EWidgetClipping::ClipToBounds)
			[
				AnchorTab
			]
		]);

	FSlateApplication::Get().AddWindow(AppWindow.ToSharedRef());
	FSlateApplication::Get().SetKeyboardFocus(AppMainFrame, EFocusCause::SetDirectly);
}

void FRenderStudioModule::OnAppWindowCloseRequested(const TSharedRef<SWindow>& Window)
{
	// The window stays up until the engine shuts down; closing it means quitting the application.
	if (!AppMainFrame.IsValid() || AppMainFrame->ConfirmCloseDocument(true))
	{
		RequestQuit();
	}
}

void FRenderStudioModule::OnExitRequested()
{
	// File > Exit already asked about unsaved changes
	RequestQuit();
}

TSharedRef<SDockTab> FRenderStudioModule::SpawnAppTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("AppTabTitle", "Render Studio"))
		.Icon(FRenderStudioStyle::GetBrush("RenderStudio.AppIcon.Small"));

	TSharedRef<SRenderStudioMainFrame> MainFrame = SNew(SRenderStudioMainFrame, Tab, Args.GetOwnerWindow());
	Tab->SetContent(MainFrame);

	TWeakPtr<SRenderStudioMainFrame> WeakMainFrame = MainFrame;
	Tab->SetCanCloseTab(SDockTab::FCanCloseTab::CreateLambda([WeakMainFrame]()
	{
		TSharedPtr<SRenderStudioMainFrame> PinnedMainFrame = WeakMainFrame.Pin();
		return !PinnedMainFrame.IsValid() || PinnedMainFrame->ConfirmCloseDocument();
	}));

	return Tab;
}

void FRenderStudioModule::RequestQuit()
{
	// Goes through the normal editor shutdown (layout and settings are saved on the way out).
	if (GEngine)
	{
		GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRenderStudioModule, RenderStudio)
