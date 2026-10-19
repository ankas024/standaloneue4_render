#include "SRenderStudioViewport.h"

#include "DragAndDrop/AssetDragDropOp.h"
#include "GameFramework/Actor.h"
#include "Input/DragAndDrop.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"
#include "RenderStudioScene.h"
#include "RenderStudioStyle.h"
#include "RenderStudioViewportClient.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioViewport"

namespace RenderStudioViewport
{
	static bool IsMeshFile(const FString& File)
	{
		const FString Extension = FPaths::GetExtension(File).ToLower();
		return Extension == TEXT("fbx") || Extension == TEXT("obj");
	}

	static bool IsEnvironmentFile(const FString& File)
	{
		return FPaths::GetExtension(File).ToLower() == TEXT("hdr");
	}

	static UMaterialInterface* GetDraggedMaterial(const FDragDropEvent& DragDropEvent)
	{
		TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
		if (AssetOp.IsValid())
		{
			for (const FAssetData& Asset : AssetOp->GetAssets())
			{
				if (UMaterialInterface* Material = Cast<UMaterialInterface>(Asset.GetAsset()))
				{
					return Material;
				}
			}
		}
		return nullptr;
	}
}

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

AActor* SRenderStudioViewport::GetObjectUnderCursor(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) const
{
	if (!ViewportClient.IsValid() || !ViewportClient->Viewport)
	{
		return nullptr;
	}

	// Hit proxies are in viewport pixels, which differ from Slate units on high-DPI screens
	const FVector2D LocalPosition = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FIntPoint ViewportSize = ViewportClient->Viewport->GetSizeXY();
	if (LocalSize.X <= 0.0f || LocalSize.Y <= 0.0f)
	{
		return nullptr;
	}

	const int32 X = FMath::Clamp(FMath::FloorToInt(LocalPosition.X * ViewportSize.X / LocalSize.X), 0, ViewportSize.X - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt(LocalPosition.Y * ViewportSize.Y / LocalSize.Y), 0, ViewportSize.Y - 1);
	return ViewportClient->GetObjectAt(X, Y);
}

FReply SRenderStudioViewport::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (RenderStudioViewport::GetDraggedMaterial(DragDropEvent))
	{
		return FReply::Handled();
	}

	TSharedPtr<FExternalDragOperation> FileOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (FileOp.IsValid() && FileOp->HasFiles())
	{
		return FReply::Handled();
	}

	return SEditorViewport::OnDragOver(MyGeometry, DragDropEvent);
}

FReply SRenderStudioViewport::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// Material from the library onto an object
	if (UMaterialInterface* Material = RenderStudioViewport::GetDraggedMaterial(DragDropEvent))
	{
		if (AActor* Target = GetObjectUnderCursor(MyGeometry, DragDropEvent))
		{
			Scene->AssignMaterial(Target, Material);
			Scene->SetSelectedObject(Target);
			Scene->SetSelectedMaterial(Material);
		}
		return FReply::Handled();
	}

	// Files from the desktop
	TSharedPtr<FExternalDragOperation> FileOp = DragDropEvent.GetOperationAs<FExternalDragOperation>();
	if (FileOp.IsValid() && FileOp->HasFiles())
	{
		TArray<FString> MeshFiles;
		TArray<FString> EnvironmentFiles;
		for (const FString& File : FileOp->GetFiles())
		{
			if (RenderStudioViewport::IsMeshFile(File))
			{
				MeshFiles.Add(File);
			}
			else if (RenderStudioViewport::IsEnvironmentFile(File))
			{
				EnvironmentFiles.Add(File);
			}
		}

		if (EnvironmentFiles.Num() > 0)
		{
			Scene->ImportEnvironmentMaps(EnvironmentFiles);
		}
		if (MeshFiles.Num() > 0 && Scene->ImportMeshes(MeshFiles) > 0)
		{
			ViewportClient->FrameScene();
		}
		return FReply::Handled();
	}

	return SEditorViewport::OnDrop(MyGeometry, DragDropEvent);
}

#undef LOCTEXT_NAMESPACE
