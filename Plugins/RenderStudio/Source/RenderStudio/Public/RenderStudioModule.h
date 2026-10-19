#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FSpawnTabArgs;
class SDockTab;
class SRenderStudioMainFrame;
class SWindow;

/**
 * Starts Render Studio.
 *
 * Two ways to run it:
 *  - as the standalone application (RenderStudio.exe, or any editor build started with
 *    -RenderStudio): the level editor never appears, Render Studio opens in its own top-level
 *    window and closing that window quits the process.
 *  - inside the regular editor while developing the plugin: Window > Render Studio.
 */
class FRenderStudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName AppTabId;

	/** True for the standalone application. */
	static bool IsAppMode();

private:
	TSharedRef<SDockTab> SpawnAppTab(const FSpawnTabArgs& Args);
	void OnMainFrameCreated(TSharedPtr<SWindow> RootWindow, bool bIsNewProjectWindow);
	void OpenAppWindow();
	void OnAppWindowCloseRequested(const TSharedRef<SWindow>& Window);
	void OnExitRequested();
	void RequestQuit();

	FDelegateHandle MainFrameCreatedHandle;
	TSharedPtr<SWindow> AppWindow;
	TSharedPtr<SRenderStudioMainFrame> AppMainFrame;
};
