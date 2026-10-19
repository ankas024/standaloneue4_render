#include "RenderStudioModule.h"

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
#include "Widgets/SWindow.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "FRenderStudioModule"

const FName FRenderStudioModule::AppTabId(TEXT("RenderStudioApp"));

void FRenderStudioModule::StartupModule()
{
	FRenderStudioStyle::Initialize();
	FRenderStudioCommands::Register();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(AppTabId, FOnSpawnTab::CreateRaw(this, &FRenderStudioModule::SpawnAppTab))
		.SetDisplayName(LOCTEXT("AppTabTitle", "Render Studio"))
		.SetTooltipText(LOCTEXT("AppTabTooltip", "Open Render Studio, the look-dev / rendering workspace."))
		.SetIcon(FSlateIcon(FRenderStudioStyle::GetStyleSetName(), "RenderStudio.AppIcon.Small"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrame.IsWindowInitialized())
	{
		OnMainFrameCreated(MainFrame.GetParentWindow(), false);
	}
	else
	{
		MainFrameCreatedHandle = MainFrame.OnMainFrameCreationFinished().AddRaw(this, &FRenderStudioModule::OnMainFrameCreated);
	}
}

void FRenderStudioModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		FModuleManager::GetModuleChecked<IMainFrameModule>("MainFrame").OnMainFrameCreationFinished().Remove(MainFrameCreatedHandle);
	}

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(AppTabId);
	}

	FRenderStudioCommands::Unregister();
	FRenderStudioStyle::Shutdown();
}

bool FRenderStudioModule::IsAppMode()
{
	return FParse::Param(FCommandLine::Get(), TEXT("RenderStudio"));
}

void FRenderStudioModule::OpenRenderStudio()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(AppTabId));
}

void FRenderStudioModule::OnMainFrameCreated(TSharedPtr<SWindow> RootWindow, bool bIsNewProjectWindow)
{
	if (bIsNewProjectWindow || !IsAppMode())
	{
		return;
	}

	OpenRenderStudio();

	// Application mode: the level editor stays alive (it owns the engine loop and the
	// material editor windows) but is hidden, so Render Studio is the only visible window.
	if (RootWindow.IsValid())
	{
		RootWindow->HideWindow();
	}
}

TSharedRef<SDockTab> FRenderStudioModule::SpawnAppTab(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::MajorTab)
		.Label(LOCTEXT("AppTabTitle", "Render Studio"))
		.Icon(FRenderStudioStyle::GetBrush("RenderStudio.AppIcon.Small"));

	SRenderStudioMainFrame::FArguments FrameArgs;
	if (IsAppMode())
	{
		FrameArgs.OnExitRequested(FSimpleDelegate::CreateRaw(this, &FRenderStudioModule::RequestQuit));
		Tab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FRenderStudioModule::OnAppTabClosed));
	}

	Tab->SetContent(
		SArgumentNew(FrameArgs, SRenderStudioMainFrame, Tab, Args.GetOwnerWindow())
	);

	return Tab;
}

void FRenderStudioModule::OnAppTabClosed(TSharedRef<SDockTab> Tab)
{
	RequestQuit();
}

void FRenderStudioModule::RequestQuit()
{
	// Goes through the normal editor shutdown, which prompts to save modified materials.
	if (GEngine)
	{
		GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FRenderStudioModule, RenderStudio)
