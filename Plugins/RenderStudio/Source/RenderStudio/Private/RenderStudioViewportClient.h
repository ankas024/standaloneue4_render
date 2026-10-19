#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"

class FAdvancedPreviewScene;
class FRenderStudioScene;
class SRenderStudioViewport;

/**
 * Camera, picking and gizmo handling for the main viewport.
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

	/** Saves the current view to a PNG. Returns the written path (empty on failure). */
	FString SaveScreenshot(const FString& InFilename);

private:
	TWeakPtr<FRenderStudioScene> SceneWeak;
	FAdvancedPreviewScene* AdvancedPreviewScene;
	bool bManipulating;
};
