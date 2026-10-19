#include "SRenderStudioMaterialPanel.h"

#include "AssetData.h"
#include "AssetThumbnail.h"
#include "Components/MeshComponent.h"
#include "EditorStyleSet.h"
#include "Engine/Texture.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "PropertyCustomizationHelpers.h"
#include "RenderStudioCommands.h"
#include "RenderStudioScene.h"
#include "RenderStudioStyle.h"
#include "SRenderStudioPanel.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SRenderStudioMaterialPanel"

namespace RenderStudioMaterialPanel
{
	static const float ThumbnailSize = 44.0f;
	static const float LabelWidth = 110.0f;

	static FString MakeDisplayName(const FMaterialParameterInfo& Info)
	{
		return FName::NameToDisplayString(Info.Name.ToString(), false);
	}
}

SRenderStudioMaterialPanel::~SRenderStudioMaterialPanel()
{
	if (Scene.IsValid())
	{
		Scene->OnMaterialsChanged().RemoveAll(this);
		Scene->OnObjectsChanged().RemoveAll(this);
		Scene->OnSelectionChanged().RemoveAll(this);
	}
}

void SRenderStudioMaterialPanel::Construct(const FArguments& InArgs, const TSharedRef<FRenderStudioScene>& InScene)
{
	Scene = InScene;
	CommandList = InArgs._CommandList;
	ThumbnailPool = MakeShareable(new FAssetThumbnailPool(64));

	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	auto MakeCommandButton = [this](const TSharedPtr<FUICommandInfo>& Command, const FText& Label)
	{
		TWeakPtr<FUICommandList> WeakCommands = CommandList;
		return RenderStudioWidgets::MakeButton(
			Label,
			Command->GetDescription(),
			FOnClicked::CreateLambda([WeakCommands, Command]()
			{
				if (TSharedPtr<FUICommandList> PinnedCommands = WeakCommands.Pin())
				{
					PinnedCommands->ExecuteAction(Command.ToSharedRef());
				}
				return FReply::Handled();
			}),
			nullptr,
			TAttribute<bool>::Create([WeakCommands, Command]()
			{
				TSharedPtr<FUICommandList> PinnedCommands = WeakCommands.Pin();
				return PinnedCommands.IsValid() && PinnedCommands->CanExecuteAction(Command.ToSharedRef());
			}));
	};

	ChildSlot
	[
		SNew(SRenderStudioPanel)
		.Title(LOCTEXT("Title", "Materials"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.4f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(ListView, SListView<FItem>)
					.ListItemsSource(&Items)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow(this, &SRenderStudioMaterialPanel::OnGenerateRow)
					.OnSelectionChanged(this, &SRenderStudioMaterialPanel::OnListSelectionChanged)
					.OnMouseButtonDoubleClick(this, &SRenderStudioMaterialPanel::OnListDoubleClick)
					.OnContextMenuOpening(this, &SRenderStudioMaterialPanel::OnContextMenuOpening)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.Toolbar"))
					.Padding(FMargin(4.0f, 4.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
						[
							MakeCommandButton(Commands.NewMaterial, LOCTEXT("New", "New"))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
						[
							MakeCommandButton(Commands.NewMaterialInstance, LOCTEXT("Instance", "Instance"))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
						[
							MakeCommandButton(Commands.AssignMaterial, LOCTEXT("Assign", "Assign"))
						]
						+ SHorizontalBox::Slot().FillWidth(1.0f)
						[
							SNullWidget::NullWidget
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f)
						[
							MakeCommandButton(Commands.EditMaterial, LOCTEXT("EditGraph", "Edit Graph..."))
						]
					]
				]
			]

			+ SSplitter::Slot()
			.Value(0.6f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.PanelHeader"))
					.Padding(FMargin(8.0f, 3.0f))
					[
						SNew(STextBlock)
						.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Header"))
						.Text(this, &SRenderStudioMaterialPanel::GetSelectedMaterialTitle)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(8.0f, 4.0f))
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
					.Text(this, &SRenderStudioMaterialPanel::GetSelectedMaterialHint)
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SAssignNew(ParameterBox, SScrollBox)
				]
			]
		]
	];

	Scene->OnMaterialsChanged().AddSP(this, &SRenderStudioMaterialPanel::RefreshMaterials);
	Scene->OnObjectsChanged().AddSP(this, &SRenderStudioMaterialPanel::RefreshMaterials);
	Scene->OnSelectionChanged().AddSP(this, &SRenderStudioMaterialPanel::OnObjectSelectionChanged);

	OnObjectSelectionChanged();
	RefreshMaterials();
}

void SRenderStudioMaterialPanel::RefreshMaterials()
{
	Items.Reset();
	for (UMaterialInterface* Material : Scene->GetMaterials())
	{
		Items.Add(Material);
	}

	// Drop thumbnails of materials that disappeared
	for (auto It = Thumbnails.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !Items.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	ListView->RequestListRefresh();

	{
		TGuardValue<bool> Guard(bSyncingSelection, true);
		UMaterialInterface* Selected = Scene->GetSelectedMaterial();
		if (Selected && Items.Contains(Selected))
		{
			ListView->SetSelection(Selected, ESelectInfo::Direct);
			ListView->RequestScrollIntoView(Selected);
		}
		else
		{
			ListView->ClearSelection();
		}
	}

	if (ParametersFor.Get() != Scene->GetSelectedMaterial())
	{
		RebuildParameters();
	}
}

void SRenderStudioMaterialPanel::OnObjectSelectionChanged()
{
	// Picking an object picks its (first) material, like the classic look-dev tools.
	if (AActor* Actor = Scene->GetSelectedObject())
	{
		if (UMeshComponent* MeshComponent = Actor->FindComponentByClass<UMeshComponent>())
		{
			if (UMaterialInterface* Material = MeshComponent->GetMaterial(0))
			{
				Scene->SetSelectedMaterial(Material);
			}
		}
	}
}

TSharedRef<ITableRow> SRenderStudioMaterialPanel::OnGenerateRow(FItem Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	UMaterialInterface* Material = Item.Get();

	TSharedPtr<FAssetThumbnail>& Thumbnail = Thumbnails.FindOrAdd(Item);
	if (!Thumbnail.IsValid())
	{
		Thumbnail = MakeShareable(new FAssetThumbnail(Material, RenderStudioMaterialPanel::ThumbnailSize, RenderStudioMaterialPanel::ThumbnailSize, ThumbnailPool));
	}

	FText Subtitle = LOCTEXT("MaterialType", "Material");
	if (const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
	{
		Subtitle = FText::Format(LOCTEXT("InstanceType", "Instance of {0}"), FText::FromString(Instance->Parent ? Instance->Parent->GetName() : TEXT("None")));
	}
	if (Material && !Material->GetPathName().StartsWith(TEXT("/Game/")))
	{
		Subtitle = FText::Format(LOCTEXT("ReadOnlyType", "{0} (engine, read only)"), Subtitle);
	}

	return SNew(STableRow<FItem>, OwnerTable)
		.Style(&FRenderStudioStyle::Get().GetWidgetStyle<FTableRowStyle>("RenderStudio.TableRow"))
		.Padding(FMargin(4.0f, 3.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(RenderStudioMaterialPanel::ThumbnailSize)
				.HeightOverride(RenderStudioMaterialPanel::ThumbnailSize)
				[
					Thumbnail->MakeThumbnailWidget()
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Bold"))
					.Text(FText::FromString(Material ? Material->GetName() : TEXT("<missing>")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
					.Text(Subtitle)
				]
			]
		];
}

void SRenderStudioMaterialPanel::OnListSelectionChanged(FItem Item, ESelectInfo::Type SelectInfo)
{
	if (!bSyncingSelection && Item.IsValid())
	{
		Scene->SetSelectedMaterial(Item.Get());
	}
}

void SRenderStudioMaterialPanel::OnListDoubleClick(FItem Item)
{
	Scene->OpenMaterialEditor(Item.Get());
}

TSharedPtr<SWidget> SRenderStudioMaterialPanel::OnContextMenuOpening()
{
	const FRenderStudioCommands& Commands = FRenderStudioCommands::Get();
	FMenuBuilder MenuBuilder(true, CommandList);
	MenuBuilder.AddMenuEntry(Commands.AssignMaterial);
	MenuBuilder.AddMenuEntry(Commands.NewMaterialInstance);
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddMenuEntry(Commands.EditMaterial);
	return MenuBuilder.MakeWidget();
}

//////////////////////////////////////////////////////////////////////////
// Parameters

void SRenderStudioMaterialPanel::RebuildParameters()
{
	ParameterBox->ClearChildren();

	UMaterialInterface* Material = Scene->GetSelectedMaterial();
	ParametersFor = Material;
	if (!Material)
	{
		return;
	}

	TArray<FParameter> Parameters;
	auto Gather = [&Parameters, Material](EParameterType Type, const TArray<FMaterialParameterInfo>& Infos)
	{
		for (const FMaterialParameterInfo& Info : Infos)
		{
			FParameter& Parameter = Parameters.AddDefaulted_GetRef();
			Parameter.Type = Type;
			Parameter.Info = Info;
			Material->GetGroupName(Info, Parameter.Group);
		}
	};

	TArray<FMaterialParameterInfo> Infos;
	TArray<FGuid> Ids;
	Material->GetAllTextureParameterInfo(Infos, Ids);
	Gather(EParameterType::Texture, Infos);
	Infos.Reset(); Ids.Reset();
	Material->GetAllVectorParameterInfo(Infos, Ids);
	Gather(EParameterType::Vector, Infos);
	Infos.Reset(); Ids.Reset();
	Material->GetAllScalarParameterInfo(Infos, Ids);
	Gather(EParameterType::Scalar, Infos);

	Parameters.StableSort([](const FParameter& A, const FParameter& B)
	{
		const FString GroupA = A.Group.IsNone() ? TEXT("~") : A.Group.ToString();
		const FString GroupB = B.Group.IsNone() ? TEXT("~") : B.Group.ToString();
		return GroupA < GroupB;
	});

	if (Parameters.Num() == 0)
	{
		ParameterBox->AddSlot()
		.Padding(FMargin(8.0f))
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Subdued"))
			.Text(LOCTEXT("NoParameters", "This material exposes no parameters. Open the graph and convert constants to parameters (right click > Convert to Parameter) to tweak them here."))
		];
		return;
	}

	FName CurrentGroup = NAME_Name; // sentinel that never matches a real group
	for (const FParameter& Parameter : Parameters)
	{
		if (Parameter.Group != CurrentGroup)
		{
			CurrentGroup = Parameter.Group;

			FString GroupLabel = CurrentGroup.IsNone() ? TEXT("General") : CurrentGroup.ToString();
			// "01 Albedo" -> "Albedo": numeric prefixes are only used for ordering
			int32 SpaceIndex = INDEX_NONE;
			if (GroupLabel.FindChar(TEXT(' '), SpaceIndex) && GroupLabel.Left(SpaceIndex).IsNumeric())
			{
				GroupLabel = GroupLabel.Mid(SpaceIndex + 1);
			}

			ParameterBox->AddSlot()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 2.0f))
			[
				SNew(SBorder)
				.BorderImage(FRenderStudioStyle::GetBrush("RenderStudio.PanelDark"))
				.Padding(FMargin(8.0f, 2.0f))
				[
					SNew(STextBlock)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text.Title"))
					.Text(FText::FromString(GroupLabel.ToUpper()))
				]
			];
		}

		ParameterBox->AddSlot()
		.Padding(FMargin(8.0f, 2.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(RenderStudioMaterialPanel::LabelWidth)
				[
					SNew(STextBlock)
					.TextStyle(&FRenderStudioStyle::Get().GetWidgetStyle<FTextBlockStyle>("RenderStudio.Text"))
					.Text(FText::FromString(RenderStudioMaterialPanel::MakeDisplayName(Parameter.Info)))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				MakeParameterWidget(Parameter)
			]
		];
	}
}

TSharedRef<SWidget> SRenderStudioMaterialPanel::MakeParameterWidget(const FParameter& Parameter)
{
	UMaterialInterface* Material = Scene->GetSelectedMaterial();
	const FMaterialParameterInfo Info = Parameter.Info;

	switch (Parameter.Type)
	{
	case EParameterType::Scalar:
	{
		float SliderMin = 0.0f;
		float SliderMax = 1.0f;
		if (!Material->GetScalarParameterSliderMinMax(Info, SliderMin, SliderMax) || SliderMax <= SliderMin)
		{
			SliderMin = 0.0f;
			SliderMax = FMath::Max(1.0f, GetScalar(Info) * 2.0f);
		}

		return SNew(SSpinBox<float>)
			.MinSliderValue(SliderMin)
			.MaxSliderValue(SliderMax)
			.Delta(0.0f)
			.Value(this, &SRenderStudioMaterialPanel::GetScalar, Info)
			.OnValueChanged(this, &SRenderStudioMaterialPanel::SetScalar, Info, false)
			.OnValueCommitted_Lambda([this, Info](float Value, ETextCommit::Type) { SetScalar(Value, Info, true); })
			.OnEndSliderMovement(this, &SRenderStudioMaterialPanel::SetScalar, Info, true);
	}

	case EParameterType::Vector:
		return SNew(SBox)
			.HeightOverride(18.0f)
			[
				SNew(SColorBlock)
				.Color(this, &SRenderStudioMaterialPanel::GetVector, Info)
				.ShowBackgroundForAlpha(false)
				.UseSRGB(true)
				.ToolTipText(LOCTEXT("ColorTooltip", "Click to pick a colour"))
				.OnMouseButtonDown(this, &SRenderStudioMaterialPanel::OnColorSwatchClicked, Info)
			];

	case EParameterType::Texture:
	default:
		return SNew(SObjectPropertyEntryBox)
			.AllowedClass(UTexture::StaticClass())
			.ObjectPath(this, &SRenderStudioMaterialPanel::GetTexturePath, Info)
			.OnObjectChanged(this, &SRenderStudioMaterialPanel::SetTexture, Info)
			.ThumbnailPool(ThumbnailPool)
			.DisplayThumbnail(true)
			.AllowClear(true);
	}
}

float SRenderStudioMaterialPanel::GetScalar(FMaterialParameterInfo Info) const
{
	float Value = 0.0f;
	if (UMaterialInterface* Material = ParametersFor.Get())
	{
		Material->GetScalarParameterValue(Info, Value);
	}
	return Value;
}

void SRenderStudioMaterialPanel::SetScalar(float Value, FMaterialParameterInfo Info, bool bCommit)
{
	UMaterialInterface* Material = ParametersFor.Get();
	if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material))
	{
		// Instances update live while dragging
		Instance->SetScalarParameterValueEditorOnly(Info, Value);
		if (bCommit)
		{
			CommitMaterialChange(Instance);
		}
	}
	else if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		// Base material defaults need a shader recompile: only apply on commit
		if (bCommit)
		{
			BaseMaterial->SetScalarParameterValueEditorOnly(Info.Name, Value);
			CommitMaterialChange(BaseMaterial);
		}
	}
}

FLinearColor SRenderStudioMaterialPanel::GetVector(FMaterialParameterInfo Info) const
{
	FLinearColor Value = FLinearColor::Black;
	if (UMaterialInterface* Material = ParametersFor.Get())
	{
		Material->GetVectorParameterValue(Info, Value);
	}
	return Value;
}

void SRenderStudioMaterialPanel::SetVector(FLinearColor Value, FMaterialParameterInfo Info, bool bCommit)
{
	UMaterialInterface* Material = ParametersFor.Get();
	if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material))
	{
		Instance->SetVectorParameterValueEditorOnly(Info, Value);
		if (bCommit)
		{
			CommitMaterialChange(Instance);
		}
	}
	else if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		if (bCommit)
		{
			BaseMaterial->SetVectorParameterValueEditorOnly(Info.Name, Value);
			CommitMaterialChange(BaseMaterial);
		}
	}
}

FString SRenderStudioMaterialPanel::GetTexturePath(FMaterialParameterInfo Info) const
{
	UTexture* Texture = nullptr;
	if (UMaterialInterface* Material = ParametersFor.Get())
	{
		Material->GetTextureParameterValue(Info, Texture);
	}
	return Texture ? Texture->GetPathName() : FString();
}

void SRenderStudioMaterialPanel::SetTexture(const FAssetData& AssetData, FMaterialParameterInfo Info)
{
	UTexture* Texture = Cast<UTexture>(AssetData.GetAsset());
	UMaterialInterface* Material = ParametersFor.Get();
	if (UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Material))
	{
		Instance->SetTextureParameterValueEditorOnly(Info, Texture);
		CommitMaterialChange(Instance);
	}
	else if (UMaterial* BaseMaterial = Cast<UMaterial>(Material))
	{
		BaseMaterial->SetTextureParameterValueEditorOnly(Info.Name, Texture);
		CommitMaterialChange(BaseMaterial);
	}
}

FReply SRenderStudioMaterialPanel::OnColorSwatchClicked(const FGeometry& Geometry, const FPointerEvent& MouseEvent, FMaterialParameterInfo Info)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const bool bIsInstance = ParametersFor.IsValid() && ParametersFor->IsA<UMaterialInstanceConstant>();

	FColorPickerArgs PickerArgs;
	PickerArgs.bUseAlpha = false;
	PickerArgs.bIsModal = false;
	PickerArgs.bOnlyRefreshOnMouseUp = !bIsInstance;
	PickerArgs.ParentWidget = AsShared();
	PickerArgs.InitialColorOverride = GetVector(Info);
	PickerArgs.OnColorCommitted = FOnLinearColorValueChanged::CreateLambda([this, Info](FLinearColor NewColor)
	{
		SetVector(NewColor, Info, true);
	});
	OpenColorPicker(PickerArgs);

	return FReply::Handled();
}

void SRenderStudioMaterialPanel::CommitMaterialChange(UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}

	Material->PostEditChange();
	Material->MarkPackageDirty();

	if (TSharedPtr<FAssetThumbnail>* Thumbnail = Thumbnails.Find(Material))
	{
		ThumbnailPool->RefreshThumbnail(*Thumbnail);
	}
}

FText SRenderStudioMaterialPanel::GetSelectedMaterialTitle() const
{
	if (UMaterialInterface* Material = Scene->GetSelectedMaterial())
	{
		return FText::Format(LOCTEXT("ParametersTitle", "Parameters - {0}"), FText::FromString(Material->GetName()));
	}
	return LOCTEXT("NoMaterialTitle", "Parameters");
}

FText SRenderStudioMaterialPanel::GetSelectedMaterialHint() const
{
	UMaterialInterface* Material = Scene->GetSelectedMaterial();
	if (!Material)
	{
		return LOCTEXT("NoMaterialHint", "Select a material, or click New to create one.");
	}
	if (!Material->GetPathName().StartsWith(TEXT("/Game/")))
	{
		return LOCTEXT("EngineHint", "Engine material: create an Instance to tweak it.");
	}
	if (Material->IsA<UMaterialInstanceConstant>())
	{
		return LOCTEXT("InstanceHint", "Changes are live. Use Edit Graph to change the node network of the parent material.");
	}
	return LOCTEXT("BaseHint", "Base material: edits recompile shaders on release. Create an Instance for live tweaking.");
}

#undef LOCTEXT_NAMESPACE
