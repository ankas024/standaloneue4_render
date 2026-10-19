#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class SDockTab;
class SWindow;
class FSpawnTabArgs;

/**
 * Registers the Render Studio application tab.
 *
 * Two ways to run it:
 *  - inside the editor: Window > Render Studio
 *  - as a standalone app: launch the editor with -RenderStudio. The level editor window is
 *    hidden, Render Studio opens in its own window and closing it quits the process.
 */
class FRenderStudioModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName AppTabId;

	/** True when started with -RenderStudio (application mode). */
	static bool IsAppMode();

	/** Opens (or focuses) the Render Studio window. */
	static void OpenRenderStudio();

private:
	TSharedRef<SDockTab> SpawnAppTab(const FSpawnTabArgs& Args);
	void OnMainFrameCreated(TSharedPtr<SWindow> RootWindow, bool bIsNewProjectWindow);
	void OnAppTabClosed(TSharedRef<SDockTab> Tab);
	void RequestQuit();

	FDelegateHandle MainFrameCreatedHandle;
};
