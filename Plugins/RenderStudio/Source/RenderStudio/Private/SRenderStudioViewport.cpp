#include "SRenderStudioViewport.h"

#include "GameFramework/Actor.h"
#include "RenderStudioScene.h"
#include "RenderStudioStyle.h"
#include "RenderStudioViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioViewport"

SRenderStudioViewport::SRenderStudioViewport()
{
}

SRenderStudioViewport::~SRenderStudioViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = nullptr;
	}
}

void SRenderStudioViewport::Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene)
{
	Scene = InScene;

	SEditorViewport::Construct(SEditorViewport::FArguments());

	ViewportClient->FrameScene(true);
}

TSharedRef<SEditorViewport> SRenderStudioViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SRenderStudioViewport::GetExtenders() const
{
	return MakeShareable(new FExtender);
}

TSharedRef<FEditorViewportClient> SRenderStudioViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FRenderStudioViewportClient(Scene.ToSharedRef(), SharedThis(this)));
	ViewportClient->VisibilityDelegate.BindSP(this, &SRenderStudioViewport::IsVisible);
	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SRenderStudioViewport::MakeViewportToolbar()
{
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}

void SRenderStudioViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	// Small HUD in the bottom-left corner: selection + navigation hint, like the old DCC apps.
	Overlay->AddSlot()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Left)
	.Padding(FMargin(8.0f))
	[
		SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.PanelDark"))
		.BorderBackgroundColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.6f))
		.Padding(FMargin(6.0f, 3.0f))
		[
			SNew(STextBlock)
			.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
			.Text(this, &SRenderStudioViewport::GetOverlayText)
		]
	];
}

FText SRenderStudioViewport::GetOverlayText() const
{
	FText Selection = LOCTEXT("NoSelection", "Nothing selected");
	if (Scene.IsValid() && Scene->GetSelectedObject())
	{
		Selection = FText::FromString(Scene->GetObjectLabel(Scene->GetSelectedObject()));
	}

	return FText::Format(LOCTEXT("OverlayFormat", "{0}    |    LMB orbit  MMB pan  RMB zoom  F frame  W/E/R gizmo  L+drag sun  K+drag sky"), Selection);
}

void SRenderStudioViewport::OnFocusViewportToSelection()
{
	ViewportClient->FrameSelection();
}

EVisibility SRenderStudioViewport::OnGetViewportContentVisibility() const
{
	return IsVisible() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
