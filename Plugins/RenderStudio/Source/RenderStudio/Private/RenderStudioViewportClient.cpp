#include "RenderStudioViewportClient.h"

#include "AdvancedPreviewScene.h"
#include "EngineUtils.h"
#include "HighResScreenshot.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderStudioScene.h"
#include "RenderStudioSettings.h"
#include "SRenderStudioViewport.h"
#include "UnrealWidget.h"

FRenderStudioViewportClient::FRenderStudioViewportClient(const TSharedRef<FRenderStudioScene>& InScene, const TSharedRef<SRenderStudioViewport>& InViewportWidget)
	: FEditorViewportClient(nullptr, &InScene->GetPreviewScene().Get(), StaticCastSharedRef<SEditorViewport>(InViewportWidget))
	, SceneWeak(InScene)
	, AdvancedPreviewScene(&InScene->GetPreviewScene().Get())
	, bManipulating(false)
{
	SetViewMode(VMI_Lit);
	SetRealtime(true);

	EngineShowFlags.SetSelectionOutline(true);
	EngineShowFlags.SetCompositeEditorPrimitives(true);
	EngineShowFlags.SetSeparateTranslucency(true);
	EngineShowFlags.SetSnap(0);

	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.GridColorAxis = FColor(120, 120, 120);
	DrawHelper.GridColorMajor = FColor(90, 90, 90);
	DrawHelper.GridColorMinor = FColor(70, 70, 70);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;
	bSetListenerPosition = false;

	SetWidgetMode(FWidget::WM_Translate);

	// Three-quarter hero angle; FrameScene() then pulls the camera back along it.
	SetViewLocation(FVector(-320.0f, 220.0f, 200.0f));
	SetViewRotation(FRotator(-18.0f, -34.0f, 0.0f));
}

void FRenderStudioViewportClient::Tick(float DeltaSeconds)
{
	if (const URenderStudioSettings* Settings = GetDefault<URenderStudioSettings>())
	{
		ViewFOV = Settings->FieldOfView;
		DrawHelper.bDrawGrid = Settings->bShowGrid;
		DrawHelper.PerspectiveGridSize = 4096.0f;
		DrawHelper.NumCells = FMath::Clamp(FMath::RoundToInt(DrawHelper.PerspectiveGridSize / (Settings->GridCellSize * 2.0f)), 1, 512);
	}

	FEditorViewportClient::Tick(DeltaSeconds);

	if (!GIntraFrameDebuggingGameThread && PreviewScene)
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

bool FRenderStudioViewportClient::InputKey(FViewport* InViewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed, bool bGamepad)
{
	// K / L + drag spins the sky / sun (handled by the advanced preview scene)
	bool bHandled = AdvancedPreviewScene->HandleInputKey(InViewport, ControllerId, Key, Event, AmountDepressed, bGamepad);
	bHandled |= FEditorViewportClient::InputKey(InViewport, ControllerId, Key, Event, AmountDepressed, bGamepad);
	return bHandled;
}

bool FRenderStudioViewportClient::InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad)
{
	if (AdvancedPreviewScene->HandleViewportInput(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad))
	{
		Invalidate();
		return true;
	}
	return FEditorViewportClient::InputAxis(InViewport, ControllerId, Key, Delta, DeltaTime, NumSamples, bGamepad);
}

void FRenderStudioViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Scene.IsValid())
	{
		return;
	}

	if (HitProxy && HitProxy->IsA(HWidgetAxis::StaticGetType()))
	{
		return;
	}

	AActor* ClickedActor = nullptr;
	if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		ClickedActor = static_cast<HActor*>(HitProxy)->Actor;
	}

	if (Key == EKeys::LeftMouseButton || Key == EKeys::RightMouseButton)
	{
		Scene->SetSelectedObject(Scene->IsSceneObject(ClickedActor) ? ClickedActor : nullptr);
		Invalidate();
	}
}

bool FRenderStudioViewportClient::InputWidgetDelta(FViewport* InViewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale)
{
	if (FEditorViewportClient::InputWidgetDelta(InViewport, CurrentAxis, Drag, Rot, Scale))
	{
		return true;
	}

	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	AActor* Actor = Scene.IsValid() ? Scene->GetSelectedObject() : nullptr;
	if (!bManipulating || !Actor || CurrentAxis == EAxisList::None)
	{
		return false;
	}

	if (!Drag.IsZero())
	{
		Actor->AddActorWorldOffset(Drag);
	}
	if (!Rot.IsZero())
	{
		Actor->SetActorRotation(Rot.Quaternion() * Actor->GetActorQuat());
	}
	if (!Scale.IsZero())
	{
		const FVector NewScale = Actor->GetActorScale3D() + Scale;
		Actor->SetActorScale3D(NewScale.ComponentMax(FVector(0.001f)));
	}

	Invalidate();
	return true;
}

void FRenderStudioViewportClient::TrackingStarted(const FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	bManipulating = bIsDraggingWidget && Scene.IsValid() && Scene->GetSelectedObject() != nullptr;
}

void FRenderStudioViewportClient::TrackingStopped()
{
	bManipulating = false;
}

FWidget::EWidgetMode FRenderStudioViewportClient::GetWidgetMode() const
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Scene.IsValid() || Scene->GetSelectedObject() == nullptr)
	{
		return FWidget::WM_None;
	}
	return FEditorViewportClient::GetWidgetMode();
}

FVector FRenderStudioViewportClient::GetWidgetLocation() const
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (Scene.IsValid() && Scene->GetSelectedObject())
	{
		return Scene->GetSelectedObject()->GetActorLocation();
	}
	return FVector::ZeroVector;
}

FMatrix FRenderStudioViewportClient::GetWidgetCoordSystem() const
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (Scene.IsValid() && Scene->GetSelectedObject() && GetWidgetCoordSystemSpace() == COORD_Local)
	{
		return FQuatRotationMatrix(Scene->GetSelectedObject()->GetActorQuat());
	}
	return FMatrix::Identity;
}

bool FRenderStudioViewportClient::ShouldOrbitCamera() const
{
	// Always orbit: LMB orbit, MMB pan, RMB dolly - no Alt modifier needed.
	return true;
}

void FRenderStudioViewportClient::FrameSelection(bool bInstant)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Scene.IsValid())
	{
		return;
	}

	FBox Bounds = Scene->GetBounds(true);
	if (!Bounds.IsValid)
	{
		Bounds = FBox(FVector(-100.0f, -100.0f, 0.0f), FVector(100.0f, 100.0f, 150.0f));
	}
	FocusViewportOnBox(Bounds, bInstant);
	Invalidate();
}

void FRenderStudioViewportClient::FrameScene(bool bInstant)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Scene.IsValid())
	{
		return;
	}

	SetViewRotation(FRotator(-18.0f, -34.0f, 0.0f));

	FBox Bounds = Scene->GetBounds(false);
	if (!Bounds.IsValid)
	{
		Bounds = FBox(FVector(-100.0f, -100.0f, 0.0f), FVector(100.0f, 100.0f, 150.0f));
	}
	FocusViewportOnBox(Bounds, bInstant);
	Invalidate();
}

FString FRenderStudioViewportClient::SaveScreenshot(const FString& InFilename)
{
	if (!Viewport)
	{
		return FString();
	}

	FString Filename = InFilename;
	if (FPaths::GetExtension(Filename).IsEmpty())
	{
		Filename += TEXT(".png");
	}

	const int32 Multiplier = FMath::Clamp(GetDefault<URenderStudioSettings>()->ScreenshotResolutionMultiplier, 1, 4);
	if (Multiplier > 1)
	{
		// Supersampled path: the engine renders the view again at N x resolution and writes the file itself.
		FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
		Config.SetResolution(Viewport->GetSizeXY().X * Multiplier, Viewport->GetSizeXY().Y * Multiplier);
		Config.SetFilename(FPaths::ChangeExtension(Filename, TEXT("")));
		Config.SetMaskEnabled(false);
		Config.SetHDRCapture(false);
		Viewport->TakeHighResScreenShot();
		return Filename;
	}

	// Make sure the latest frame (with the gizmo hidden) is what we read back.
	const bool bWasShowingWidget = bShowWidget;
	ShowWidget(false);
	Viewport->Draw();
	ShowWidget(bWasShowingWidget);

	TArray<FColor> Pixels;
	const FIntPoint Size = Viewport->GetSizeXY();
	if (!Viewport->ReadPixels(Pixels) || Pixels.Num() != Size.X * Size.Y)
	{
		return FString();
	}

	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> PngWriter = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!PngWriter.IsValid() || !PngWriter->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Size.X, Size.Y, ERGBFormat::BGRA, 8))
	{
		return FString();
	}

	const TArray64<uint8>& Compressed = PngWriter->GetCompressed();
	if (!FFileHelper::SaveArrayToFile(Compressed, *Filename))
	{
		return FString();
	}
	return Filename;
}
