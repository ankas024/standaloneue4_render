#pragma once

#include "CoreMinimal.h"

class FRenderStudioScene;
class FRenderStudioViewportClient;

namespace RenderStudioSelfTest
{
	/**
	 * Exercises the application end to end on the live scene: objects, undo / redo, labels,
	 * visibility, material presets, HDRIs, camera bookmarks, scene save / load, standard views,
	 * final renders, a short turntable and a viewport screenshot.
	 *
	 * Writes SelfTest.log, a test scene and the rendered images to OutputDirectory.
	 * Run it from Help > Run Self Test, or start the application with -RenderStudioSelfTest
	 * (it then quits when done, which makes it usable from build machines).
	 *
	 * @return true if every check passed. One line per check is appended to OutLog.
	 */
	bool Run(FRenderStudioScene& Scene, FRenderStudioViewportClient& Viewport, const FString& OutputDirectory, TArray<FString>& OutLog);
}
