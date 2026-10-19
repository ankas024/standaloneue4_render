#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class AActor;
class FRenderStudioScene;
class FUICommandList;
class ITableRow;
class STableViewBase;

/** Flat list of the objects in the scene with visibility toggles, rename and a context menu. */
class SRenderStudioOutliner : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioOutliner) {}
		SLATE_ARGUMENT(TSharedPtr<FUICommandList>, CommandList)
	SLATE_END_ARGS()

	virtual ~SRenderStudioOutliner();

	void Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene);

private:
	typedef TWeakObjectPtr<AActor> FItem;

	void RefreshItems();
	void SyncSelectionFromScene();

	TSharedRef<ITableRow> OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnListSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo);
	void OnListDoubleClick(FItem Item);
	TSharedPtr<SWidget> OnContextMenuOpening();
	TSharedRef<SWidget> MakeAddMenu();
	FText GetSummaryText() const;

	TSharedPtr<FRenderStudioScene> Scene;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<SListView<FItem>> ListView;
	TArray<FItem> Items;
	bool bSyncingSelection = false;
};
