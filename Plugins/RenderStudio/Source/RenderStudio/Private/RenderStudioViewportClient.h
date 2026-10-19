#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "RenderStudioScene.h"

class FAdvancedPreviewScene;
class SRenderStudioViewport;

enum class ERenderStudioView : uint8
{
	Perspective,
	Front,
	Back,
	Left,
	Right,
	Top,
};

/** What to produce with FRenderStudioViewportClient::RenderImage. */
struct FRenderStudioRenderSettings
{
	int32 Width = 1920;
	int32 Height = 1080;
	/** Render at N x resolution and filter down */
	int32 Supersampling = 1;
	/** Frames of temporal anti-aliasing accumulated before capture */
	int32 Samples = 16;
};

/**
 * Camera, picking and gizmo handling for the main viewport, plus final-quality rendering.
 * Navigation follows the classic look-dev convention: LMB orbit, MMB pan, RMB / wheel zoom.
 */
class FRenderStudioViewportClient : public FEditorViewportClient
{
public:
	FRenderStudioViewportClient(const TSharedRef<FRenderStudioScene>& InScene, const TSharedRef<SRenderStudioViewport>& InViewportWidget);

	// FEditorViewportClient
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.0f, bool bGamepad = false) override;
	virtual bool InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual bool InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale) override;
	virtual void TrackingStarted(const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge) override;
	virtual void TrackingStopped() override;
	virtual FWidget::EWidgetMode GetWidgetMode() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual bool ShouldOrbitCamera() const override;

	/** Frames the selected object (or the whole scene if nothing is selected). */
	void FrameSelection(bool bInstant = false);
	/** Frames every visible object from the default three-quarter angle. */
	void FrameScene(bool bInstant = false);
	/** Looks at the scene along a world axis (or from the default three-quarter angle). */
	void SetStandardView(ERenderStudioView View);

	FRenderStudioCamera GetCamera() const;
	void SetCamera(const FRenderStudioCamera& Camera);

	/** Scene object under a viewport pixel (drag & drop target), or null. */
	AActor* GetObjectAt(int32 X, int32 Y);

	/** Saves the current view to a PNG. Returns the written path (empty on failure). */
	FString SaveScreenshot(const FString& InFilename);

	/**
	 * Renders the current camera off screen at any resolution, with supersampling and
	 * accumulated temporal anti-aliasing. Editor helpers (grid, gizmo, selection) are not drawn.
	 */
	bool RenderImage(const FRenderStudioRenderSettings& Settings, TArray<FColor>& OutPixels);
	/** Same, from an explicit camera (turntables). */
	bool RenderImage(const FRenderStudioRenderSettings& Settings, const FRenderStudioCamera& Camera, TArray<FColor>& OutPixels);

	/** Writes pixels as PNG / JPEG / BMP depending on the file extension. */
	static bool SaveImage(const FString& Filename, const TArray<FColor>& Pixels, int32 Width, int32 Height);

private:
	TWeakPtr<FRenderStudioScene> SceneWeak;
	FAdvancedPreviewScene* AdvancedPreviewScene;
	bool bManipulating;
	bool bMovedDuringDrag;
};
