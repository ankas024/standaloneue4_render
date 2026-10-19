#include "RenderStudioSelfTest.h"

#include "Components/MeshComponent.h"
#include "Engine/TextureCube.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderStudioScene.h"
#include "RenderStudioViewportClient.h"
#include "ShaderCompiler.h"

DEFINE_LOG_CATEGORY_STATIC(LogRenderStudioSelfTest, Log, All);

namespace RenderStudioSelfTest
{
	static AActor* FindByLabel(const FRenderStudioScene& Scene, const FString& Label)
	{
		for (AActor* Actor : Scene.GetObjects())
		{
			if (Scene.GetObjectLabel(Actor) == Label)
			{
				return Actor;
			}
		}
		return nullptr;
	}

	/** Mean brightness (0-255) and whether the image has any detail at all. */
	static void Analyse(const TArray<FColor>& Pixels, float& OutMean, bool& bOutHasDetail)
	{
		double Sum = 0.0;
		int32 MinLuma = 255;
		int32 MaxLuma = 0;
		for (const FColor& Pixel : Pixels)
		{
			const int32 Luma = (Pixel.R * 3 + Pixel.G * 6 + Pixel.B) / 10;
			Sum += Luma;
			MinLuma = FMath::Min(MinLuma, Luma);
			MaxLuma = FMath::Max(MaxLuma, Luma);
		}
		OutMean = Pixels.Num() > 0 ? static_cast<float>(Sum / Pixels.Num()) : 0.0f;
		bOutHasDetail = (MaxLuma - MinLuma) > 16;
	}

	bool Run(FRenderStudioScene& Scene, FRenderStudioViewportClient& Viewport, const FString& OutputDirectory, TArray<FString>& OutLog)
	{
		int32 NumFailed = 0;
		auto Expect = [&OutLog, &NumFailed](bool bCondition, const FString& What)
		{
			OutLog.Add(FString::Printf(TEXT("%s  %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *What));
			UE_LOG(LogRenderStudioSelfTest, Display, TEXT("%s"), *OutLog.Last());
			NumFailed += bCondition ? 0 : 1;
			return bCondition;
		};

		IFileManager::Get().MakeDirectory(*OutputDirectory, true);

		// Objects and undo
		Scene.NewScene();
		Expect(Scene.GetObjects().Num() == 1, TEXT("new scene contains the shader ball"));

		AActor* Cube = Scene.AddPrimitive(ERenderStudioPrimitive::Cube);
		AActor* Light = Scene.AddLight(ERenderStudioLight::Point);
		Expect(Cube && Light && Scene.GetObjects().Num() == 3, TEXT("add a cube and a point light"));
		Expect(Scene.GetSelectedObject() == Light, TEXT("new objects are selected"));
		Expect(Scene.IsDirty(), TEXT("changes mark the scene modified"));

		Scene.Undo();
		Scene.Undo();
		Expect(Scene.GetObjects().Num() == 1, TEXT("undo twice removes both objects"));
		Scene.Redo();
		Scene.Redo();
		Expect(Scene.GetObjects().Num() == 3 && !Scene.CanRedo(), TEXT("redo twice brings them back"));

		Cube = FindByLabel(Scene, TEXT("Cube"));
		if (!Expect(Cube != nullptr, TEXT("objects keep their labels through undo / redo")))
		{
			return false;
		}

		Scene.SetObjectLabel(Cube, TEXT("Hero Cube"));
		Scene.SetObjectVisible(Cube, false);
		Expect(!Scene.IsObjectVisible(Cube), TEXT("hide an object"));
		Scene.Undo();
		Cube = FindByLabel(Scene, TEXT("Hero Cube"));
		Expect(Cube && Scene.IsObjectVisible(Cube), TEXT("undo hide restores visibility and keeps the new label"));
		if (!Cube)
		{
			return false;
		}

		const FVector StartLocation = Cube->GetActorLocation();
		{
			FRenderStudioScopedChange Change(Scene, FText::FromString(TEXT("Move")));
			Cube->SetActorLocation(StartLocation + FVector(0.0f, 0.0f, 80.0f));
		}
		Scene.Undo();
		Cube = FindByLabel(Scene, TEXT("Hero Cube"));
		Expect(Cube && Cube->GetActorLocation().Equals(StartLocation, 0.1f), TEXT("undo a move restores the transform"));
		if (!Cube)
		{
			return false;
		}

		// Materials
		const FRenderStudioMaterialPreset* Gold = Scene.GetMaterialPresets().FindByPredicate([](const FRenderStudioMaterialPreset& Preset) { return Preset.Name == TEXT("Gold"); });
		UMaterialInstanceConstant* GoldInstance = Gold ? Scene.CreateMaterialFromPreset(*Gold) : nullptr;
		float Metalness = 0.0f;
		Expect(GoldInstance && GoldInstance->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Metalness")), Metalness) && Metalness > 0.99f,
			TEXT("create a material from the Gold preset"));
		if (GoldInstance)
		{
			Scene.AssignMaterial(Cube, GoldInstance);
			UMeshComponent* Mesh = Cube->FindComponentByClass<UMeshComponent>();
			Expect(Mesh && Mesh->GetMaterial(0) == GoldInstance, TEXT("assign the material"));
		}

		// Environment and cameras
		const TArray<UTextureCube*> Skies = Scene.GetEnvironmentMaps();
		Expect(Skies.Num() > 0, FString::Printf(TEXT("HDRI list (%d found)"), Skies.Num()));
		if (Skies.Num() > 0)
		{
			Scene.SetEnvironmentMap(Skies[0]);
			Expect(Scene.GetEnvironmentMap() == Skies[0], TEXT("switch the sky HDRI"));
		}

		Viewport.SetStandardView(ERenderStudioView::Front);
		Expect(FMath::IsNearlyZero(Viewport.GetCamera().Rotation.Yaw, 0.1f), TEXT("front view"));
		Scene.SetBookmark(0, Viewport.GetCamera());
		Viewport.SetStandardView(ERenderStudioView::Perspective);
		Scene.SetSceneCamera(Viewport.GetCamera());

		// Save, clear, load
		FEditorFileUtils::SaveDirtyPackages(false, false, true);
		const FString SceneFile = OutputDirectory / TEXT("SelfTest.rss");
		FText Error;
		Expect(Scene.SaveScene(SceneFile, Error) && !Scene.IsDirty(), FString::Printf(TEXT("save %s %s"), *SceneFile, *Error.ToString()));

		Scene.NewScene();
		Expect(Scene.GetObjects().Num() == 1, TEXT("new scene after saving"));

		Expect(Scene.LoadScene(SceneFile, Error), FString::Printf(TEXT("load the scene back %s"), *Error.ToString()));
		Cube = FindByLabel(Scene, TEXT("Hero Cube"));
		Expect(Scene.GetObjects().Num() == 3 && Cube != nullptr, TEXT("loaded scene has its 3 objects and labels"));
		if (Cube)
		{
			UMeshComponent* Mesh = Cube->FindComponentByClass<UMeshComponent>();
			Expect(Mesh && GoldInstance && Mesh->GetMaterial(0) == GoldInstance, TEXT("loaded scene keeps the material assignment"));
			Expect(Cube->GetActorLocation().Equals(StartLocation, 0.1f), TEXT("loaded scene keeps transforms"));
		}
		Expect(Scene.GetBookmark(0).bIsSet && FMath::IsNearlyZero(Scene.GetBookmark(0).Rotation.Yaw, 0.1f), TEXT("loaded scene keeps camera bookmarks"));
		Expect(!Scene.CanUndo(), TEXT("loading starts a fresh undo history"));

		// Rendering
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}

		FRenderStudioRenderSettings RenderSettings;
		RenderSettings.Width = 640;
		RenderSettings.Height = 360;
		RenderSettings.Supersampling = 2;
		RenderSettings.Samples = 8;

		TArray<FColor> Pixels;
		const bool bRendered = Viewport.RenderImage(RenderSettings, Pixels);
		float Mean = 0.0f;
		bool bHasDetail = false;
		Analyse(Pixels, Mean, bHasDetail);
		Expect(bRendered && Pixels.Num() == 640 * 360, TEXT("render a 640x360 image with 2x supersampling and 8 samples"));
		Expect(Mean > 8.0f && bHasDetail, FString::Printf(TEXT("render shows the scene (mean brightness %.1f)"), Mean));
		const FString RenderFile = OutputDirectory / TEXT("SelfTest_Render.png");
		Expect(bRendered && FRenderStudioViewportClient::SaveImage(RenderFile, Pixels, 640, 360), TEXT("save the render as PNG"));

		const FString JpegFile = OutputDirectory / TEXT("SelfTest_Render.jpg");
		Expect(bRendered && FRenderStudioViewportClient::SaveImage(JpegFile, Pixels, 640, 360), TEXT("save the render as JPEG"));

		// A short turntable: same orbit maths as the render dialog
		const FBox Bounds = Scene.GetBounds(false);
		const FVector Pivot = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
		const FRenderStudioCamera StartCamera = Viewport.GetCamera();
		RenderSettings.Width = 320;
		RenderSettings.Height = 180;
		RenderSettings.Supersampling = 1;
		RenderSettings.Samples = 4;
		int32 NumFrames = 0;
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			const FRotator Turn(0.0f, 90.0f * Frame, 0.0f);
			FRenderStudioCamera Camera = StartCamera;
			Camera.Location = Pivot + Turn.RotateVector(StartCamera.Location - Pivot);
			Camera.Rotation = (Turn.Quaternion() * StartCamera.Rotation.Quaternion()).Rotator();

			TArray<FColor> FramePixels;
			if (Viewport.RenderImage(RenderSettings, Camera, FramePixels)
				&& FRenderStudioViewportClient::SaveImage(OutputDirectory / FString::Printf(TEXT("SelfTest_Turntable_%04d.png"), Frame), FramePixels, 320, 180))
			{
				++NumFrames;
			}
		}
		Expect(NumFrames == 4, TEXT("render a 4 frame turntable"));
		Expect(FVector::Dist(Viewport.GetCamera().Location, StartCamera.Location) < 0.1f, TEXT("turntable leaves the viewport camera alone"));

		const FString ScreenshotFile = Viewport.SaveScreenshot(OutputDirectory / TEXT("SelfTest_Viewport.png"));
		Expect(!ScreenshotFile.IsEmpty(), TEXT("viewport screenshot"));

		OutLog.Add(NumFailed == 0 ? TEXT("RESULT: all checks passed") : FString::Printf(TEXT("RESULT: %d check(s) failed"), NumFailed));
		UE_LOG(LogRenderStudioSelfTest, Display, TEXT("%s"), *OutLog.Last());
		FFileHelper::SaveStringArrayToFile(OutLog, *(OutputDirectory / TEXT("SelfTest.log")));
		return NumFailed == 0;
	}
}
