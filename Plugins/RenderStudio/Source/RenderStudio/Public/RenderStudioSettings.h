#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RenderStudioSettings.generated.h"

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

	/** Multiplier applied to the viewport resolution when saving screenshots */
	UPROPERTY(EditAnywhere, config, Category = "Screenshot", meta = (ClampMin = "1", ClampMax = "4"))
	int32 ScreenshotResolutionMultiplier;

	/** Folder screenshots are written to (defaults to Saved/Screenshots/RenderStudio) */
	UPROPERTY(EditAnywhere, config, Category = "Screenshot")
	FDirectoryPath ScreenshotDirectory;

	/** Content folder that new materials and imported meshes are created in */
	UPROPERTY(EditAnywhere, config, Category = "Content", meta = (ContentDir))
	FDirectoryPath ContentFolder;

	FString GetContentFolder() const;
	FString GetScreenshotDirectory() const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
};
