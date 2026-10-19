#include "SRenderStudioPanel.h"

#include "RenderStudioStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SRenderStudioPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.Panel"))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.PanelHeader"))
				.Padding(FMargin(8.0f, 3.0f, 4.0f, 3.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Header"))
						.Text(InArgs._Title)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						InArgs._HeaderContent.Widget
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				InArgs._Content.Widget
			]
		]
	];
}

TSharedRef<SWidget> RenderStudioWidgets::MakeButton(const FText& Label, const FText& ToolTip, FOnClicked OnClicked, const FSlateBrush* Icon, TAttribute<bool> IsEnabled)
{
	TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);

	if (Icon)
	{
		Content->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, Label.IsEmpty() ? 0.0f : 4.0f, 0.0f)
		[
			SNew(SImage).Image(Icon)
		];
	}

	if (!Label.IsEmpty())
	{
		Content->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text"))
			.Text(Label)
		];
	}

	return SNew(SButton)
		.ButtonStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FButtonStyle>("RenderStudio.Button"))
		.ToolTipText(ToolTip)
		.OnClicked(OnClicked)
		.IsEnabled(IsEnabled)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			Content
		];
}
