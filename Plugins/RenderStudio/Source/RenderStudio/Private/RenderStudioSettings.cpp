#include "RenderStudioSettings.h"

#include "Misc/Paths.h"

URenderStudioSettings::URenderStudioSettings()
	: FieldOfView(45.0f)
	, bShowGrid(true)
	, GridCellSize(50.0f)
	, ScreenshotResolutionMultiplier(1)
{
	ContentFolder.Path = TEXT("/Game/RenderStudio");
}

FString URenderStudioSettings::GetContentFolder() const
{
	FString Folder = ContentFolder.Path;
	if (Folder.IsEmpty() || !Folder.StartsWith(TEXT("/")))
	{
		Folder = TEXT("/Game/RenderStudio");
	}
	Folder.RemoveFromEnd(TEXT("/"));
	return Folder;
}

FString URenderStudioSettings::GetScreenshotDirectory() const
{
	if (!ScreenshotDirectory.Path.IsEmpty())
	{
		return ScreenshotDirectory.Path;
	}
	return FPaths::ScreenShotDir() / TEXT("RenderStudio");
}

void URenderStudioSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
}
