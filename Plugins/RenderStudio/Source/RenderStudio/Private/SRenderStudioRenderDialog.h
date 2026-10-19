#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FRenderStudioScene;
class FRenderStudioViewportClient;
class SWindow;
enum class ERenderStudioImageFormat : uint8;

/**
 * Final render settings (resolution, supersampling, anti-aliasing samples, format, folder)
 * plus the two outputs: a still image of the current camera, or a turntable image sequence
 * that orbits the camera once around the scene. Values persist in URenderStudioSettings.
 */
class SRenderStudioRenderDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioRenderDialog) {}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene, FRenderStudioViewportClient* InViewportClient);

	/** File (still) or folder (turntable) written by the last render, empty if nothing was rendered. */
	const FString& GetLastOutput() const { return LastOutput; }

private:
	TSharedRef<SWidget> MakeRow(const FText& Label, const TSharedRef<SWidget>& Content);
	TSharedRef<SWidget> MakeResolutionMenu();
	TSharedRef<SWidget> MakeFormatButton(ERenderStudioImageFormat Format, const FText& Label);

	FText GetResolutionSummary() const;
	FText GetOutputDirectoryText() const;
	FReply OnBrowseOutputDirectory();
	FReply OnRenderImage();
	FReply OnRenderTurntable();
	FReply OnClose();

	TSharedPtr<FRenderStudioScene> Scene;
	FRenderStudioViewportClient* ViewportClient = nullptr;
	TWeakPtr<SWindow> ParentWindow;
	FString LastOutput;
};
