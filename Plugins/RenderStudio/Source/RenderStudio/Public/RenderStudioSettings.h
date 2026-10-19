#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/Object.h"
#include "RenderStudioSettings.generated.h"

UENUM()
enum class ERenderStudioImageFormat : uint8
{
	PNG,
	JPEG,
	BMP,
};

/** Per-user application settings shown next to the lighting / environment profile. */
UCLASS(config = EditorPerProjectUserSettings)
class RENDERSTUDIO_API URenderStudioSettings : public UObject
{
	GENERATED_BODY()

public:
	URenderStudioSettings();

	/** Horizontal field of view of the viewport camera, in degrees */
	UPROPERTY(EditAnywhere, config, Category = "Camera", meta = (ClampMin = "5.0", ClampMax = "170.0", UIMin = "10.0", UIMax = "120.0"))
	float FieldOfView;

	/** Draw the reference grid on the ground plane */
	UPROPERTY(EditAnywhere, config, Category = "Viewport")
	bool bShowGrid;

	/** Size of one major grid cell, in centimetres */
	UPROPERTY(EditAnywhere, config, Category = "Viewport", meta = (ClampMin = "1.0", EditCondition = "bShowGrid"))
	float GridCellSize;

	/** Multiplier applied to the viewport resolution when saving screenshots (F12) */
	UPROPERTY(EditAnywhere, config, Category = "Screenshot", meta = (ClampMin = "1", ClampMax = "4"))
	int32 ScreenshotResolutionMultiplier;

	/** Folder screenshots and renders are written to (defaults to Documents/Render Studio/Renders) */
	UPROPERTY(EditAnywhere, config, Category = "Screenshot")
	FDirectoryPath ScreenshotDirectory;

	/** Width of final renders, in pixels */
	UPROPERTY(EditAnywhere, config, Category = "Render", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 RenderWidth;

	/** Height of final renders, in pixels */
	UPROPERTY(EditAnywhere, config, Category = "Render", meta = (ClampMin = "16", ClampMax = "8192"))
	int32 RenderHeight;

	/** Renders at N times the resolution and filters down, for clean edges and fine detail */
	UPROPERTY(EditAnywhere, config, Category = "Render", meta = (ClampMin = "1", ClampMax = "4"))
	int32 RenderSupersampling;

	/** Frames accumulated by temporal anti-aliasing before the image is captured */
	UPROPERTY(EditAnywhere, config, Category = "Render", meta = (ClampMin = "1", ClampMax = "64"))
	int32 RenderSamples;

	UPROPERTY(EditAnywhere, config, Category = "Render")
	ERenderStudioImageFormat RenderFormat;

	/** Number of frames in a turntable sequence (one full turn of the camera) */
	UPROPERTY(EditAnywhere, config, Category = "Render", meta = (ClampMin = "2", ClampMax = "720"))
	int32 TurntableFrames;

	/** Content folder that new materials, imported meshes and HDRIs are created in */
	UPROPERTY(EditAnywhere, config, Category = "Content", meta = (ContentDir))
	FDirectoryPath ContentFolder;

	/** Most recently opened scene files, newest first */
	UPROPERTY(config)
	TArray<FString> RecentScenes;

	/** Folder of the last scene opened or saved */
	UPROPERTY(config)
	FString LastSceneDirectory;

	FString GetContentFolder() const;
	FString GetScreenshotDirectory() const;
	/** Where the open / save dialogs start (last used folder, else Documents/Render Studio/Scenes) */
	FString GetSceneDirectory() const;
	FString GetImageExtension() const;
	void AddRecentScene(const FString& Filename);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
