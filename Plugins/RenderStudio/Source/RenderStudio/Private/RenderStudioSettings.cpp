#include "RenderStudioSettings.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

URenderStudioSettings::URenderStudioSettings()
	: FieldOfView(45.0f)
	, bShowGrid(true)
	, GridCellSize(50.0f)
	, ScreenshotResolutionMultiplier(1)
	, RenderWidth(1920)
	, RenderHeight(1080)
	, RenderSupersampling(2)
	, RenderSamples(16)
	, RenderFormat(ERenderStudioImageFormat::PNG)
	, TurntableFrames(72)
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
	return FString(FPlatformProcess::UserDir()) / TEXT("Render Studio") / TEXT("Renders");
}

FString URenderStudioSettings::GetSceneDirectory() const
{
	if (!LastSceneDirectory.IsEmpty())
	{
		return LastSceneDirectory;
	}
	return FString(FPlatformProcess::UserDir()) / TEXT("Render Studio") / TEXT("Scenes");
}

FString URenderStudioSettings::GetImageExtension() const
{
	switch (RenderFormat)
	{
	case ERenderStudioImageFormat::JPEG:	return TEXT("jpg");
	case ERenderStudioImageFormat::BMP:		return TEXT("bmp");
	case ERenderStudioImageFormat::PNG:
	default:								return TEXT("png");
	}
}

void URenderStudioSettings::AddRecentScene(const FString& Filename)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
	RecentScenes.Remove(FullPath);
	RecentScenes.Insert(FullPath, 0);
	if (RecentScenes.Num() > 8)
	{
		RecentScenes.SetNum(8);
	}
	LastSceneDirectory = FPaths::GetPath(FullPath);
	SaveConfig();
}

void URenderStudioSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	SaveConfig();
}
