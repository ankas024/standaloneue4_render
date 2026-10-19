#include "RenderStudioViewportClient.h"

#include "AdvancedPreviewScene.h"
#include "CanvasTypes.h"
#include "EngineModule.h"
#include "EngineUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LegacyScreenPercentageDriver.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RendererInterface.h"
#include "RenderingThread.h"
#include "RenderStudioSettings.h"
#include "SceneView.h"
#include "SRenderStudioViewport.h"
#include "UnrealWidget.h"

#define LOCTEXT_NAMESPACE "RenderStudioViewportClient"

namespace RenderStudioViewportClient
{
	static const FRotator HeroRotation(-18.0f, -34.0f, 0.0f);
	static const int32 MaxRenderSize = 16384;

	static FBox GetFramingBounds(const FRenderStudioScene& Scene, bool bSelectionOnly)
	{
		FBox Bounds = Scene.GetBounds(bSelectionOnly);
		if (!Bounds.IsValid)
		{
			Bounds = FBox(FVector(-100.0f, -100.0f, 0.0f), FVector(100.0f, 100.0f, 150.0f));
		}
		return Bounds;
	}

	/** Box filter in linear space, so supersampled edges keep their brightness. */
	static void Downsample(const TArray<FColor>& Source, int32 SourceWidth, int32 Factor, int32 Width, int32 Height, TArray<FColor>& Out)
	{
		Out.SetNumUninitialized(Width * Height);
		const float Weight = 1.0f / (Factor * Factor);
		for (int32 Y = 0; Y < Height; ++Y)
		{
			for (int32 X = 0; X < Width; ++X)
			{
				FLinearColor Sum = FLinearColor::Transparent;
				for (int32 SubY = 0; SubY < Factor; ++SubY)
				{
					const FColor* Row = &Source[(Y * Factor + SubY) * SourceWidth + X * Factor];
					for (int32 SubX = 0; SubX < Factor; ++SubX)
					{
						Sum += FLinearColor(Row[SubX]);
					}
				}
				Out[Y * Width + X] = (Sum * Weight).ToFColor(true);
			}
		}
	}
}

FRenderStudioViewportClient::FRenderStudioViewportClient(const TSharedRef<FRenderStudioScene>& InScene, const TSharedRef<SRenderStudioViewport>& InViewportWidget)
	: FEditorViewportClient(nullptr, &InScene->GetPreviewScene().Get(), StaticCastSharedRef<SEditorViewport>(InViewportWidget))
	, SceneWeak(InScene)
	, AdvancedPreviewScene(&InScene->GetPreviewScene().Get())
	, bManipulating(false)
	, bMovedDuringDrag(false)
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
	SetViewRotation(RenderStudioViewportClient::HeroRotation);
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

	bMovedDuringDrag = true;

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

	if (bManipulating)
	{
		// One undo step per gizmo drag (dropped again if the gizmo was only clicked)
		bMovedDuringDrag = false;
		Scene->BeginChange(LOCTEXT("TransformObject", "Transform"));
	}
}

void FRenderStudioViewportClient::TrackingStopped()
{
	if (bManipulating)
	{
		if (TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin())
		{
			Scene->EndChange(!bMovedDuringDrag);
		}
	}
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

//////////////////////////////////////////////////////////////////////////
// Camera

void FRenderStudioViewportClient::FrameSelection(bool bInstant)
{
	if (TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin())
	{
		FocusViewportOnBox(RenderStudioViewportClient::GetFramingBounds(*Scene, true), bInstant);
		Invalidate();
	}
}

void FRenderStudioViewportClient::FrameScene(bool bInstant)
{
	SetStandardView(ERenderStudioView::Perspective);
}

void FRenderStudioViewportClient::SetStandardView(ERenderStudioView View)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Scene.IsValid())
	{
		return;
	}

	FRotator Rotation = RenderStudioViewportClient::HeroRotation;
	switch (View)
	{
	case ERenderStudioView::Front:	Rotation = FRotator(0.0f, 0.0f, 0.0f); break;
	case ERenderStudioView::Back:	Rotation = FRotator(0.0f, 180.0f, 0.0f); break;
	case ERenderStudioView::Left:	Rotation = FRotator(0.0f, 90.0f, 0.0f); break;
	case ERenderStudioView::Right:	Rotation = FRotator(0.0f, -90.0f, 0.0f); break;
	case ERenderStudioView::Top:	Rotation = FRotator(-89.9f, 0.0f, 0.0f); break;
	default: break;
	}

	SetViewRotation(Rotation);
	FocusViewportOnBox(RenderStudioViewportClient::GetFramingBounds(*Scene, false), true);
	Invalidate();
}

FRenderStudioCamera FRenderStudioViewportClient::GetCamera() const
{
	FRenderStudioCamera Camera;
	Camera.Location = GetViewLocation();
	Camera.Rotation = GetViewRotation();
	Camera.FieldOfView = GetDefault<URenderStudioSettings>()->FieldOfView;
	Camera.bIsSet = true;
	return Camera;
}

void FRenderStudioViewportClient::SetCamera(const FRenderStudioCamera& Camera)
{
	if (!Camera.bIsSet)
	{
		return;
	}

	SetViewLocation(Camera.Location);
	SetViewRotation(Camera.Rotation);

	// Re-centre the orbit pivot in front of the camera so orbiting keeps working naturally
	const float PivotDistance = 300.0f;
	SetLookAtLocation(Camera.Location + Camera.Rotation.Vector() * PivotDistance);

	URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();
	Settings->FieldOfView = Camera.FieldOfView;
	ViewFOV = Camera.FieldOfView;
	Invalidate();
}

AActor* FRenderStudioViewportClient::GetObjectAt(int32 X, int32 Y)
{
	TSharedPtr<FRenderStudioScene> Scene = SceneWeak.Pin();
	if (!Viewport || !Scene.IsValid())
	{
		return nullptr;
	}

	HHitProxy* HitProxy = Viewport->GetHitProxy(X, Y);
	if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
	{
		AActor* Actor = static_cast<HActor*>(HitProxy)->Actor;
		return Scene->IsSceneObject(Actor) ? Actor : nullptr;
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
// Output

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
		// Supersampled: render the viewport's camera off screen at N x and filter down
		FRenderStudioRenderSettings Settings;
		Settings.Width = Viewport->GetSizeXY().X;
		Settings.Height = Viewport->GetSizeXY().Y;
		Settings.Supersampling = Multiplier;
		Settings.Samples = 8;

		TArray<FColor> SupersampledPixels;
		return RenderImage(Settings, SupersampledPixels) && SaveImage(Filename, SupersampledPixels, Settings.Width, Settings.Height) ? Filename : FString();
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

	return SaveImage(Filename, Pixels, Size.X, Size.Y) ? Filename : FString();
}

bool FRenderStudioViewportClient::RenderImage(const FRenderStudioRenderSettings& Settings, TArray<FColor>& OutPixels)
{
	return RenderImage(Settings, GetCamera(), OutPixels);
}

bool FRenderStudioViewportClient::RenderImage(const FRenderStudioRenderSettings& Settings, const FRenderStudioCamera& Camera, TArray<FColor>& OutPixels)
{
	UWorld* World = GetWorld();
	if (!World || !World->Scene || Settings.Width <= 0 || Settings.Height <= 0)
	{
		return false;
	}

	const int32 Factor = FMath::Clamp(Settings.Supersampling, 1, 4);
	const int32 RenderWidth = Settings.Width * Factor;
	const int32 RenderHeight = Settings.Height * Factor;
	if (RenderWidth > RenderStudioViewportClient::MaxRenderSize || RenderHeight > RenderStudioViewportClient::MaxRenderSize)
	{
		return false;
	}

	// 8-bit, non-sRGB target: the tonemapper already writes display-encoded values. Render
	// targets default to a display gamma of 1.0, which would make the tonemapper apply an extra
	// 2.2 power curve (much darker than the viewport), so use the viewport's display gamma.
	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->TargetGamma = GEngine ? GEngine->GetDisplayGamma() : 2.2f;
	RenderTarget->ClearColor = FLinearColor::Black;
	RenderTarget->InitAutoFormat(RenderWidth, RenderHeight);
	RenderTarget->UpdateResourceImmediate(true);
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();

	// A dedicated view state lets temporal AA (and SSR / AO history) accumulate over the samples.
	FSceneViewStateReference ViewState;
	ViewState.Allocate();

	FEngineShowFlags ShowFlags = EngineShowFlags;
	ShowFlags.SetSelectionOutline(false);
	ShowFlags.SetSelection(false);
	ShowFlags.SetCompositeEditorPrimitives(false);
	ShowFlags.SetBillboardSprites(false);
	ShowFlags.SetGrid(false);
	ShowFlags.SetModeWidgets(false);
	ShowFlags.SetLightRadius(false);
	ShowFlags.SetMotionBlur(false);
	ShowFlags.SetScreenPercentage(false);

	const float HalfFOVRadians = FMath::DegreesToRadians(FMath::Clamp(Camera.FieldOfView, 5.0f, 170.0f)) * 0.5f;
	const FMatrix ViewRotationMatrix = FInverseRotationMatrix(Camera.Rotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));
	// Horizontal field of view is preserved, like the viewport
	const FMatrix ProjectionMatrix = FReversedZPerspectiveMatrix(HalfFOVRadians, HalfFOVRadians, 1.0f, RenderWidth / (float)RenderHeight, GetNearClipPlane(), GetNearClipPlane());

	const int32 NumSamples = FMath::Clamp(Settings.Samples, 1, 64);
	const float FrameTime = 1.0f / 30.0f;
	float Time = World->GetTimeSeconds();

	for (int32 Sample = 0; Sample < NumSamples; ++Sample)
	{
		FCanvas Canvas(Resource, nullptr, World, World->FeatureLevel);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Resource, World->Scene, ShowFlags)
			.SetWorldTimes(Time, FrameTime, Time)
			.SetRealtimeUpdate(true));
		ViewFamily.ViewMode = GetViewMode();
		EngineShowFlagOverride(ESFIM_Editor, ViewFamily.ViewMode, ViewFamily.EngineShowFlags, false);
		ViewFamily.ExposureSettings = ExposureSettings;
		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f, false));

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, RenderWidth, RenderHeight));
		ViewInitOptions.ViewOrigin = Camera.Location;
		ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		ViewInitOptions.FOV = Camera.FieldOfView;

		FSceneView* View = new FSceneView(ViewInitOptions);
		View->ViewLocation = Camera.Location;
		View->ViewRotation = Camera.Rotation;
		View->bCameraCut = (Sample == 0);
		ViewFamily.Views.Add(View);

		View->StartFinalPostprocessSettings(Camera.Location);
		View->EndFinalPostprocessSettings(ViewInitOptions);

		GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
		Time += FrameTime;
	}

	FlushRenderingCommands();

	TArray<FColor> Pixels;
	FReadSurfaceDataFlags ReadFlags(RCM_UNorm, CubeFace_MAX);
	ReadFlags.SetLinearToGamma(false);
	const bool bRead = Resource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == RenderWidth * RenderHeight;

	ViewState.Destroy();
	RenderTarget->ReleaseResource();
	RenderTarget->MarkPendingKill();

	if (!bRead)
	{
		return false;
	}

	if (Factor > 1)
	{
		RenderStudioViewportClient::Downsample(Pixels, RenderWidth, Factor, Settings.Width, Settings.Height, OutPixels);
	}
	else
	{
		OutPixels = MoveTemp(Pixels);
	}

	for (FColor& Pixel : OutPixels)
	{
		Pixel.A = 255;
	}
	return true;
}

bool FRenderStudioViewportClient::SaveImage(const FString& Filename, const TArray<FColor>& Pixels, int32 Width, int32 Height)
{
	if (Pixels.Num() != Width * Height)
	{
		return false;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);

	const FString Extension = FPaths::GetExtension(Filename).ToLower();
	if (Extension == TEXT("bmp"))
	{
		// The BMP image wrapper can only decode in 4.27 (compressing asserts): use the bitmap writer
		return FFileHelper::CreateBitmap(*FPaths::ChangeExtension(Filename, TEXT("bmp")), Width, Height, Pixels.GetData());
	}

	const EImageFormat Format = (Extension == TEXT("jpg") || Extension == TEXT("jpeg")) ? EImageFormat::JPEG : EImageFormat::PNG;

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> Writer = ImageWrapperModule.CreateImageWrapper(Format);
	if (!Writer.IsValid() || !Writer->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
	{
		return false;
	}

	const TArray64<uint8>& Compressed = Writer->GetCompressed(Format == EImageFormat::JPEG ? 95 : 0);
	return FFileHelper::SaveArrayToFile(Compressed, *Filename);
}

#undef LOCTEXT_NAMESPACE
