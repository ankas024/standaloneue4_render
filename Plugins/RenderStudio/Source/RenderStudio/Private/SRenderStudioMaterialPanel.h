#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FAssetThumbnail;
class FAssetThumbnailPool;
class FRenderStudioScene;
class FUICommandList;
class ITableRow;
class SScrollBox;
class STableViewBase;
class UMaterialInterface;
class UTexture;
struct FAssetData;

/**
 * Material library + quick parameter editor.
 * The list shows every material in the project folder / scene with a live thumbnail;
 * the lower half exposes the selected material's parameters as sliders, swatches and
 * texture slots. "Open Material Editor" jumps into the full UE4 node graph.
 */
class SRenderStudioMaterialPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioMaterialPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
	SLATE_END_ARGS()

	virtual ~SRenderStudioMaterialPanel();

	void Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene);

private:
	typedef TWeakObjectPtr<UMaterialInterface> FItem;

	enum class EParameterType : uint8 { Scalar, Vector, Texture };

	struct FParameter
	{
		EParameterType Type;
		FMaterialParameterInfo Info;
		FName Group;
	};

	void RefreshMaterials();
	void OnObjectSelectionChanged();
	void RebuildParameters();
	TSharedRef<SWidget> MakeParameterWidget(const FParameter& Parameter);
	TSharedRef<SWidget> MakePresetButton();

	TSharedRef<ITableRow> OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnListSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo);
	void OnListDoubleClick(FItem Item);
	TSharedPtr<SWidget> OnContextMenuOpening();

	// Parameter accessors (work on both UMaterial defaults and UMaterialInstanceConstant overrides)
	float GetScalar(FMaterialParameterInfo Info) const;
	void SetScalar(float Value, FMaterialParameterInfo Info, bool bCommit);
	FLinearColor GetVector(FMaterialParameterInfo Info) const;
	void SetVector(FLinearColor Value, FMaterialParameterInfo Info, bool bCommit);
	FString GetTexturePath(FMaterialParameterInfo Info) const;
	void SetTexture(const FAssetData& AssetData, FMaterialParameterInfo Info);
	FReply OnColorSwatchClicked(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FMaterialParameterInfo Info);
	void OnPickerColorCommitted(FLinearColor NewColor, TWeakObjectPtr<UMaterialInterface> Target, FMaterialParameterInfo Info);
	void CloseColorPicker();
	void CommitMaterialChange(UMaterialInterface* Material);

	FText GetSelectedMaterialTitle() const;
	FText GetSelectedMaterialHint() const;

	TSharedPtr<FRenderStudioScene> Scene;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<FAssetThumbnailPool> ThumbnailPool;
	TMap<FItem, TSharedPtr<FAssetThumbnail>> Thumbnails;

	TSharedPtr<SListView<FItem>> ListView;
	TArray<FItem> Items;
	TSharedPtr<SScrollBox> ParameterBox;
	TWeakObjectPtr<UMaterialInterface> ParametersFor;
	bool bSyncingSelection = false;
	bool bColorPickerOpen = false;
};
