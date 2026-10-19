#pragma once

#include "CoreMinimal.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"

class FRenderStudioScene;
class FRenderStudioViewportClient;

/**
 * The main 3D view. Hosts the standard UE4 viewport toolbar (view modes, show flags, exposure, camera)
 * and accepts drag & drop: materials onto objects, mesh files to import, .hdr files to use as the sky.
 */
class SRenderStudioViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioViewport) {}
	SLATE_END_ARGS()

	SRenderStudioViewport();
	virtual ~SRenderStudioViewport();

	void Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene);

	TSharedPtr<FRenderStudioViewportClient> GetRenderStudioClient() const { return ViewportClient; }

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override {}

	// SWidget
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

protected:
	// SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual void OnFocusViewportToSelection() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;

private:
	FText GetOverlayText() const;
	/** Scene object under the cursor of a drag & drop event. */
	AActor* GetObjectUnderCursor(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) const;

	TSharedPtr<FRenderStudioScene> Scene;
	TSharedPtr<FRenderStudioViewportClient> ViewportClient;
};
