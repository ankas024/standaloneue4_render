#include "SRenderStudioOutliner.h"

#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "RenderStudioCommands.h"
#include "RenderStudioScene.h"
#include "RenderStudioStyle.h"
#include "SRenderStudioPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioOutliner"

SRenderStudioOutliner::~SRenderStudioOutliner()
{
	if (Scene.IsValid())
	{
		Scene->OnObjectsChanged().RemoveAll(this);
		Scene->OnSelectionChanged().RemoveAll(this);
	}
}

void SRenderStudioOutliner::Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene)
{
	Scene = InScene;
	CommandList = InArgs._CommandList;

	ChildSlot
	[
		SNew(SRenderStudioPanel)
		.Title(LOCTEXT("Title", "Scene"))
		.HeaderContent()
		[
			SNew(SComboButton)
			.ButtonStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FButtonStyle>("RenderStudio.Button"))
			.HasDownArrow(true)
			.ContentPadding(FMargin(4.0f, 0.0f))
			.ToolTipText(LOCTEXT("AddTooltip", "Add a primitive or a light to the scene"))
			.OnGetMenuContent(this, &SRenderStudioOutliner::MakeAddMenu)
			.ButtonContent()
			[
				SNew(STextBlock)
				.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Bold"))
				.Text(LOCTEXT("Add", "+ Add"))
			]
		]
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ListView, SListView<FItem>)
				.ListItemsSource(&Items)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SRenderStudioOutliner::OnGenerateRow)
				.OnSelectionChanged(this, &SRenderStudioOutliner::OnListSelectionChanged)
				.OnMouseButtonDoubleClick(this, &SRenderStudioOutliner::OnListDoubleClick)
				.OnContextMenuOpening(this, &SRenderStudioOutliner::OnContextMenuOpening)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.StatusBar"))
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(STextBlock)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
					.Text(this, &SRenderStudioOutliner::GetSummaryText)
				]
			]
		]
	];

	Scene->OnObjectsChanged().AddSP(this, &SRenderStudioOutliner::RefreshItems);
	Scene->OnSelectionChanged().AddSP(this, &SRenderStudioOutliner::SyncSelectionFromScene);
	RefreshItems();
}

void SRenderStudioOutliner::RefreshItems()
{
	Items.Reset();
	for (AActor* Actor : Scene->GetObjects())
	{
		Items.Add(Actor);
	}
	ListView->RequestListRefresh();
	SyncSelectionFromScene();
}

void SRenderStudioOutliner::SyncSelectionFromScene()
{
	TGuardValue<bool> Guard(bSyncingSelection, true);

	AActor* Selected = Scene->GetSelectedObject();
	if (Selected)
	{
		ListView->SetSelection(Selected, ESelectInfo::Direct);
		ListView->RequestScrollIntoView(Selected);
	}
	else
	{
		ListView->ClearSelection();
	}
}

TSharedRef<ITableRow> SRenderStudioOutliner::OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TWeakPtr<FRenderStudioScene> WeakScene = Scene;
	TWeakPtr<SListView<FItem>> WeakList = ListView;

	return SNew(STableRow<FItem>, OwnerTable)
		.Style(&FRenderStudioStyle::Get().GetWidgetStyle<FTableRowStyle>("RenderStudio.TableRow"))
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SImage)
				.Image(Scene->GetObjectIcon(Item.Get()))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SInlineEditableTextBlock)
				.Style(FEditorStyle::Get(), "InlineEditableTextBlockStyle")
				.Text_Lambda([WeakScene, Item]()
				{
					TSharedPtr<FRenderStudioScene> PinnedScene = WeakScene.Pin();
					return PinnedScene.IsValid() ? FText::FromString(PinnedScene->GetObjectLabel(Item.Get())) : FText::GetEmpty();
				})
				.IsSelected(FIsSelected::CreateLambda([WeakList, Item]()
				{
					TSharedPtr<SListView<FItem>> PinnedList = WeakList.Pin();
					return PinnedList.IsValid() && PinnedList->IsItemSelected(Item);
				}))
				.OnTextCommitted_Lambda([WeakScene, Item](const FText& NewText, ETextCommit::Type)
				{
					if (TSharedPtr<FRenderStudioScene> PinnedScene = WeakScene.Pin())
					{
						PinnedScene->SetObjectLabel(Item.Get(), NewText.ToString());
					}
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.ContentPadding(FMargin(2.0f, 0.0f))
				.ToolTipText(LOCTEXT("ToggleVisibility", "Show / hide this object"))
				.OnClicked_Lambda([WeakScene, Item]()
				{
					if (TSharedPtr<FRenderStudioScene> PinnedScene = WeakScene.Pin())
					{
						PinnedScene->SetObjectVisible(Item.Get(), !PinnedScene->IsObjectVisible(Item.Get()));
					}
					return FReply::Handled();
				})
				[
					SNew(SImage)
					.Image_Lambda([WeakScene, Item]()
					{
						TSharedPtr<FRenderStudioScene> PinnedScene = WeakScene.Pin();
						const bool bVisible = PinnedScene.IsValid() && PinnedScene->IsObjectVisible(Item.Get());
						return FRenderStudioStyle::GetBrush(bVisible ? "RenderStudio.Icon.Visible" : "RenderStudio.Icon.Hidden");
					})
				]
			]
		];
}

void SRenderStudioOutliner::OnListSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo)
{
	if (!bSyncingSelection)
	{
		Scene->SetSelectedObject(Item.Get());
	}
}

void SRenderStudioOutliner::OnListDoubleClick(FItem Item)
{
	Scene->SetSelectedObject(Item.Get());
	if (CommandList.IsValid())
	{
		CommandList->ExecuteAction(FRenderStudioCommands::Get().FocusSelected.ToSharedRef());
	}
}

TSharedPtr<SWidget> SRenderStudioOutliner::OnContextMenuOpening()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	FMenuBuilder MenuBuilder(true, CommandList);

	if (Scene->GetSelectedObject())
	{
		MenuBuilder.BeginSection("Object", LOCTEXT("ObjectSection", "Object"));
		MenuBuilder.AddMenuEntry(Commands.FocusSelected);
		MenuBuilder.AddMenuEntry(Commands.DuplicateSelected);
		MenuBuilder.AddMenuEntry(Commands.DeleteSelected);
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Material", LOCTEXT("MaterialSection", "Material"));
		MenuBuilder.AddMenuEntry(Commands.AssignMaterial);
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("Add", LOCTEXT("AddSection", "Add"));
	MenuBuilder.AddSubMenu(LOCTEXT("AddSubMenu", "Add"), LOCTEXT("AddSubMenuTooltip", "Add a primitive or light"),
		FNewMenuDelegate::CreateLambda([](FMenuBuilder& SubMenu)
		{
			const FRenderStudioCommands& SubCommands = FRenderStudioCommands::Get();
			SubMenu.AddMenuEntry(SubCommands.AddCube);
			SubMenu.AddMenuEntry(SubCommands.AddSphere);
			SubMenu.AddMenuEntry(SubCommands.AddCylinder);
			SubMenu.AddMenuEntry(SubCommands.AddCone);
			SubMenu.AddMenuEntry(SubCommands.AddPlane);
			SubMenu.AddMenuSeparator();
			SubMenu.AddMenuEntry(SubCommands.AddPointLight);
			SubMenu.AddMenuEntry(SubCommands.AddSpotLight);
			SubMenu.AddMenuEntry(SubCommands.AddRectLight);
		}));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SRenderStudioOutliner::MakeAddMenu()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	FMenuBuilder MenuBuilder(true, CommandList);

	MenuBuilder.BeginSection("Primitives", LOCTEXT("Primitives", "Primitives"));
	MenuBuilder.AddMenuEntry(Commands.AddCube);
	MenuBuilder.AddMenuEntry(Commands.AddSphere);
	MenuBuilder.AddMenuEntry(Commands.AddCylinder);
	MenuBuilder.AddMenuEntry(Commands.AddCone);
	MenuBuilder.AddMenuEntry(Commands.AddPlane);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Lights", LOCTEXT("Lights", "Lights"));
	MenuBuilder.AddMenuEntry(Commands.AddPointLight);
	MenuBuilder.AddMenuEntry(Commands.AddSpotLight);
	MenuBuilder.AddMenuEntry(Commands.AddRectLight);
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Import", LOCTEXT("ImportSection", "Import"));
	MenuBuilder.AddMenuEntry(Commands.ImportMesh);
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

FText SRenderStudioOutliner::GetSummaryText() const
{
	return FText::Format(LOCTEXT("Summary", "{0} object(s)"), FText::AsNumber(Items.Num()));
}

#undef LOCTEXT_NAMESPACE
