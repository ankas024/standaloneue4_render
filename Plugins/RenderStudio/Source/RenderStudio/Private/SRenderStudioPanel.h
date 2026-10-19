#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCompoundWidget.h"

/** A titled panel with the glossy gradient header used by every Render Studio pane. */
class SRenderStudioPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRenderStudioPanel) {}
		SLATE_ARGUMENT(FText, Title)
		/** Optional widgets placed at the right side of the header (e.g. an "Add" button) */
		SLATE_NAMED_SLOT(FArguments, HeaderContent)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

namespace RenderStudioWidgets
{
	/** Glossy push button with a label and optional icon. */
	TSharedRef<SWidget> MakeButton(const FText& Label, const FText& ToolTip, FOnClicked OnClicked, const FSlateBrush* Icon = nullptr, TAttribute<bool> IsEnabled = true);
}
