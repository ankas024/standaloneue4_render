#include "SRenderStudioRenderDialog.h"

#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "IDesktopPlatform.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "RenderStudioScene.h"
#include "RenderStudioSettings.h"
#include "RenderStudioStyle.h"
#include "RenderStudioViewportClient.h"
#include "SRenderStudioPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioRenderDialog"

namespace RenderStudioRenderDialog
{
	struct FResolutionPreset
	{
		const TCHAR* Name;
		int32 Width;
		int32 Height;
	};

	static const FResolutionPreset ResolutionPresets[] =
	{
		{ TEXT("HD 720p"),					1280, 720 },
		{ TEXT("Full HD 1080p"),			1920, 1080 },
		{ TEXT("QHD 1440p"),				2560, 1440 },
		{ TEXT("4K UHD"),					3840, 2160 },
		{ TEXT("Square 1080"),				1080, 1080 },
		{ TEXT("Square 2048"),				2048, 2048 },
		{ TEXT("Portrait 4:5 (1080x1350)"),	1080, 1350 },
		{ TEXT("Portrait 9:16 (1080x1920)"),	1080, 1920 },
		{ TEXT("A4 @ 300 dpi"),				2480, 3508 },
	};

	static const FTextBlockStyle& TextStyle(const ANSICHAR* Name)
	{
		return FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>(Name);
	}
}

void SRenderStudioRenderDialog::Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene, FRenderStudioViewportClient* InViewportClient)
{
	Scene = InScene;
	ViewportClient = InViewportClient;
	ParentWindow = InArgs._ParentWindow;

	URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();

	auto MakeIntSpin = [Settings](int32 URenderStudioSettings::* Member, int32 Min, int32 Max)
	{
		return SNew(SSpinBox<int32>)
			.MinValue(Min)
			.MaxValue(Max)
			.MinSliderValue(Min)
			.MaxSliderValue(Max)
			.Value_Lambda([Settings, Member]() { return Settings->*Member; })
			.OnValueChanged_Lambda([Settings, Member](int32 Value) { Settings->*Member = Value; })
			.OnValueCommitted_Lambda([Settings, Member](int32 Value, ETextCommit::Type) { Settings->*Member = Value; Settings->SaveConfig(); });
	};

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(460.0f)
		[
			SNew(SRenderStudioPanel)
			.Title(LOCTEXT("Title", "Render"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(10.0f, 8.0f, 10.0f, 4.0f))
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text.Subdued"))
					.Text(LOCTEXT("Intro", "Renders the current camera off screen. Grid, gizmo and selection outline are left out. The camera keeps its horizontal field of view, so wider images show more of the scene."))
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Preset", "Preset"), MakeResolutionMenu())
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Width", "Width"), MakeIntSpin(&URenderStudioSettings::RenderWidth, 16, 8192))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Height", "Height"), MakeIntSpin(&URenderStudioSettings::RenderHeight, 16, 8192))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Supersampling", "Supersampling"), MakeIntSpin(&URenderStudioSettings::RenderSupersampling, 1, 4))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Samples", "AA Samples"), MakeIntSpin(&URenderStudioSettings::RenderSamples, 1, 64))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Format", "Format"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							MakeFormatButton(ERenderStudioImageFormat::PNG, LOCTEXT("PNG", "PNG"))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							MakeFormatButton(ERenderStudioImageFormat::JPEG, LOCTEXT("JPEG", "JPEG"))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							MakeFormatButton(ERenderStudioImageFormat::BMP, LOCTEXT("BMP", "BMP"))
						])
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("Folder", "Folder"),
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text"))
							.Text(this, &SRenderStudioRenderDialog::GetOutputDirectoryText)
							.ToolTipText(this, &SRenderStudioRenderDialog::GetOutputDirectoryText)
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
						[
							RenderStudioWidgets::MakeButton(LOCTEXT("Browse", "Browse..."), LOCTEXT("BrowseTooltip", "Choose where renders are written."),
								FOnClicked::CreateSP(this, &SRenderStudioRenderDialog::OnBrowseOutputDirectory))
						])
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeRow(LOCTEXT("TurntableFrames", "Turntable Frames"), MakeIntSpin(&URenderStudioSettings::TurntableFrames, 2, 720))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(10.0f, 6.0f))
				[
					SNew(STextBlock)
					.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text.Subdued"))
					.Text(this, &SRenderStudioRenderDialog::GetResolutionSummary)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.Toolbar"))
					.Padding(FMargin(8.0f, 6.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							RenderStudioWidgets::MakeButton(LOCTEXT("RenderImage", "Render Image"), LOCTEXT("RenderImageTooltip", "Render the current camera to a single image."),
								FOnClicked::CreateSP(this, &SRenderStudioRenderDialog::OnRenderImage))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							RenderStudioWidgets::MakeButton(LOCTEXT("RenderTurntable", "Render Turntable"), LOCTEXT("RenderTurntableTooltip", "Orbit the camera once around the scene and write a numbered image sequence (combine it into a video with any editor)."),
								FOnClicked::CreateSP(this, &SRenderStudioRenderDialog::OnRenderTurntable))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							RenderStudioWidgets::MakeButton(LOCTEXT("Close", "Close"), FText::GetEmpty(), FOnClicked::CreateSP(this, &SRenderStudioRenderDialog::OnClose))
						]
					]
				]
			]
		]
	];
}

TSharedRef<SWidget> SRenderStudioRenderDialog::MakeRow(const FText& Label, const TSharedRef<SWidget>& Content)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(10.0f, 3.0f))
		[
			SNew(SBox)
			.WidthOverride(120.0f)
			[
				SNew(STextBlock)
				.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text"))
				.Text(Label)
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.0f, 3.0f, 10.0f, 3.0f))
		[
			Content
		];
}

TSharedRef<SWidget> SRenderStudioRenderDialog::MakeResolutionMenu()
{
	return SNew(SComboButton)
		.ButtonContent()
		[
			SNew(STextBlock)
			.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text"))
			.Text(LOCTEXT("PickPreset", "Choose a size..."))
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FMenuBuilder MenuBuilder(true, nullptr);

			MenuBuilder.AddMenuEntry(LOCTEXT("ViewportSize", "Viewport Size"), LOCTEXT("ViewportSizeTooltip", "Match the size of the viewport."), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					if (ViewportClient && ViewportClient->Viewport)
					{
						URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();
						Settings->RenderWidth = ViewportClient->Viewport->GetSizeXY().X;
						Settings->RenderHeight = ViewportClient->Viewport->GetSizeXY().Y;
						Settings->SaveConfig();
					}
				})));
			MenuBuilder.AddMenuSeparator();

			for (const RenderStudioRenderDialog::FResolutionPreset& Preset : RenderStudioRenderDialog::ResolutionPresets)
			{
				const int32 Width = Preset.Width;
				const int32 Height = Preset.Height;
				MenuBuilder.AddMenuEntry(
					FText::Format(LOCTEXT("PresetEntry", "{0}  ({1} x {2})"), FText::FromString(Preset.Name), FText::AsNumber(Width, &FNumberFormattingOptions::DefaultNoGrouping()), FText::AsNumber(Height, &FNumberFormattingOptions::DefaultNoGrouping())),
					FText::GetEmpty(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateLambda([Width, Height]()
					{
						URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();
						Settings->RenderWidth = Width;
						Settings->RenderHeight = Height;
						Settings->SaveConfig();
					})));
			}
			return MenuBuilder.MakeWidget();
		});
}

TSharedRef<SWidget> SRenderStudioRenderDialog::MakeFormatButton(ERenderStudioImageFormat Format, const FText& Label)
{
	return SNew(SCheckBox)
		.Style(&FRenderStudioStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RenderStudio.ToggleButton"))
		.IsChecked_Lambda([Format]() { return GetDefault<URenderStudioSettings>()->RenderFormat == Format ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged_Lambda([Format](ECheckBoxState)
		{
			URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();
			Settings->RenderFormat = Format;
			Settings->SaveConfig();
		})
		[
			SNew(SBox)
			.Padding(FMargin(10.0f, 2.0f))
			[
				SNew(STextBlock)
				.TextStyle(&RenderStudioRenderDialog::TextStyle("RenderStudio.Text"))
				.Text(Label)
			]
		];
}

FText SRenderStudioRenderDialog::GetResolutionSummary() const
{
	const URenderStudioSettings* Settings = GetDefault<URenderStudioSettings>();
	const int32 Factor = FMath::Clamp(Settings->RenderSupersampling, 1, 4);
	return FText::Format(LOCTEXT("Summary", "Output {0} x {1}, rendered internally at {2} x {3} with {4} anti-aliasing samples."),
		FText::AsNumber(Settings->RenderWidth, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(Settings->RenderHeight, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(Settings->RenderWidth * Factor, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(Settings->RenderHeight * Factor, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(Settings->RenderSamples));
}

FText SRenderStudioRenderDialog::GetOutputDirectoryText() const
{
	return FText::FromString(FPaths::ConvertRelativePathToFull(GetDefault<URenderStudioSettings>()->GetScreenshotDirectory()));
}

FReply SRenderStudioRenderDialog::OnBrowseOutputDirectory()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	URenderStudioSettings* Settings = GetMutableDefault<URenderStudioSettings>();

	FString Folder;
	if (DesktopPlatform && DesktopPlatform->OpenDirectoryDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
		LOCTEXT("BrowseTitle", "Render Output Folder").ToString(), FPaths::ConvertRelativePathToFull(Settings->GetScreenshotDirectory()), Folder))
	{
		Settings->ScreenshotDirectory.Path = Folder;
		Settings->SaveConfig();
	}
	return FReply::Handled();
}

FReply SRenderStudioRenderDialog::OnRenderImage()
{
	if (!ViewportClient)
	{
		return FReply::Handled();
	}

	const URenderStudioSettings* Settings = GetDefault<URenderStudioSettings>();
	FRenderStudioRenderSettings RenderSettings;
	RenderSettings.Width = Settings->RenderWidth;
	RenderSettings.Height = Settings->RenderHeight;
	RenderSettings.Supersampling = Settings->RenderSupersampling;
	RenderSettings.Samples = Settings->RenderSamples;

	const FString Filename = FPaths::ConvertRelativePathToFull(Settings->GetScreenshotDirectory()
		/ FString::Printf(TEXT("%s_%s.%s"), *Scene->GetDisplayName().ToString(), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *Settings->GetImageExtension()));

	TArray<FColor> Pixels;
	bool bSaved = false;
	{
		FScopedSlowTask SlowTask(1.0f, LOCTEXT("RenderingImage", "Rendering..."));
		SlowTask.MakeDialog();
		SlowTask.EnterProgressFrame();
		bSaved = ViewportClient->RenderImage(RenderSettings, Pixels)
			&& FRenderStudioViewportClient::SaveImage(Filename, Pixels, RenderSettings.Width, RenderSettings.Height);
	}

	if (!bSaved)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("RenderFailed", "The render failed. Very large sizes with supersampling can exceed the GPU limit of 16384 pixels: lower the size or the supersampling."));
		return FReply::Handled();
	}

	LastOutput = Filename;
	FPlatformProcess::ExploreFolder(*Filename);
	return OnClose();
}

FReply SRenderStudioRenderDialog::OnRenderTurntable()
{
	if (!ViewportClient)
	{
		return FReply::Handled();
	}

	const URenderStudioSettings* Settings = GetDefault<URenderStudioSettings>();
	FRenderStudioRenderSettings RenderSettings;
	RenderSettings.Width = Settings->RenderWidth;
	RenderSettings.Height = Settings->RenderHeight;
	RenderSettings.Supersampling = Settings->RenderSupersampling;
	RenderSettings.Samples = Settings->RenderSamples;

	const int32 NumFrames = FMath::Clamp(Settings->TurntableFrames, 2, 720);
	const FString Folder = FPaths::ConvertRelativePathToFull(Settings->GetScreenshotDirectory()
		/ FString::Printf(TEXT("%s_Turntable_%s"), *Scene->GetDisplayName().ToString(), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))));
	IFileManager::Get().MakeDirectory(*Folder, true);

	// Orbit the current camera around the centre of the scene, keeping its height and distance.
	FBox Bounds = Scene->GetBounds(false);
	const FVector Pivot = Bounds.IsValid ? Bounds.GetCenter() : FVector::ZeroVector;
	const FRenderStudioCamera StartCamera = ViewportClient->GetCamera();
	const FVector Offset = StartCamera.Location - Pivot;

	int32 NumWritten = 0;
	{
		FScopedSlowTask SlowTask(NumFrames, LOCTEXT("RenderingTurntable", "Rendering turntable..."));
		SlowTask.MakeDialog(true);

		for (int32 Frame = 0; Frame < NumFrames; ++Frame)
		{
			if (SlowTask.ShouldCancel())
			{
				break;
			}
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TurntableFrame", "Frame {0} of {1}"), FText::AsNumber(Frame + 1), FText::AsNumber(NumFrames)));

			const FRotator Turn(0.0f, 360.0f * Frame / NumFrames, 0.0f);
			FRenderStudioCamera Camera = StartCamera;
			Camera.Location = Pivot + Turn.RotateVector(Offset);
			Camera.Rotation = (Turn.Quaternion() * StartCamera.Rotation.Quaternion()).Rotator();

			TArray<FColor> Pixels;
			const FString Filename = Folder / FString::Printf(TEXT("frame_%04d.%s"), Frame, *Settings->GetImageExtension());
			if (!ViewportClient->RenderImage(RenderSettings, Camera, Pixels) || !FRenderStudioViewportClient::SaveImage(Filename, Pixels, RenderSettings.Width, RenderSettings.Height))
			{
				break;
			}
			++NumWritten;
		}
	}

	if (NumWritten == 0)
	{
		FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("TurntableFailed", "The turntable render failed. Lower the size or the supersampling and try again."));
		return FReply::Handled();
	}

	LastOutput = Folder;
	FPlatformProcess::ExploreFolder(*Folder);
	return OnClose();
}

FReply SRenderStudioRenderDialog::OnClose()
{
	GetMutableDefault<URenderStudioSettings>()->SaveConfig();
	if (TSharedPtr<SWindow> Window = ParentWindow.Pin())
	{
		Window->RequestDestroyWindow();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
