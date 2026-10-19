#include "RenderStudioStyle.h"

#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "EditorStyleSet.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr<FSlateStyleSet> FRenderStudioStyle::StyleInstance = nullptr;

const FLinearColor FRenderStudioStyle::PanelColor = FColor(58, 58, 62);
const FLinearColor FRenderStudioStyle::PanelDarkColor = FColor(38, 38, 41);
const FLinearColor FRenderStudioStyle::AccentColor = FColor(232, 146, 56);
const FLinearColor FRenderStudioStyle::TextColor = FColor(222, 222, 222);
const FLinearColor FRenderStudioStyle::SubduedTextColor = FColor(150, 150, 155);

#define STYLE_IMAGE(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define STYLE_BOX(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

void FRenderStudioStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FRenderStudioStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

const ISlateStyle& FRenderStudioStyle::Get()
{
	return *StyleInstance;
}

FName FRenderStudioStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("RenderStudioStyle"));
	return StyleSetName;
}

const FSlateBrush* FRenderStudioStyle::GetBrush(FName PropertyName)
{
	return Get().GetBrush(PropertyName);
}

TSharedRef<FSlateStyleSet> FRenderStudioStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin(TEXT("RenderStudio"))->GetBaseDir() / TEXT("Resources"));

	const FVector2D Icon16(16.0f, 16.0f);
	const FVector2D Icon40(40.0f, 40.0f);
	const FMargin ButtonMargin(4.0f / 16.0f);

	// Flat fills
	Style->Set("RenderStudio.Panel", new FSlateColorBrush(PanelColor));
	Style->Set("RenderStudio.PanelDark", new FSlateColorBrush(PanelDarkColor));
	Style->Set("RenderStudio.Separator", new FSlateColorBrush(FLinearColor(FColor(28, 28, 30))));

	// Gradient chrome
	Style->Set("RenderStudio.PanelHeader", new STYLE_BOX("Style/PanelHeader", FMargin(2.0f / 8.0f, 3.0f / 22.0f)));
	Style->Set("RenderStudio.Toolbar", new STYLE_BOX("Style/ToolbarBackground", FMargin(2.0f / 8.0f, 3.0f / 34.0f)));
	Style->Set("RenderStudio.MenuBar", new STYLE_BOX("Style/MenuBarBackground", FMargin(2.0f / 8.0f, 2.0f / 22.0f)));
	Style->Set("RenderStudio.StatusBar", new STYLE_BOX("Style/StatusBar", FMargin(2.0f / 8.0f, 2.0f / 20.0f)));
	Style->Set("RenderStudio.Selection", new STYLE_BOX("Style/Selection", FMargin(2.0f / 8.0f, 2.0f / 18.0f)));
	Style->Set("RenderStudio.RowHover", new STYLE_BOX("Style/RowHover", FMargin(2.0f / 8.0f, 2.0f / 18.0f)));

	// Glossy buttons
	const FButtonStyle GlossyButton = FButtonStyle()
		.SetNormal(STYLE_BOX("Style/Button", ButtonMargin))
		.SetHovered(STYLE_BOX("Style/Button_Hovered", ButtonMargin))
		.SetPressed(STYLE_BOX("Style/Button_Pressed", ButtonMargin))
		.SetDisabled(STYLE_BOX("Style/Button_Pressed", ButtonMargin, FLinearColor(1, 1, 1, 0.4f)))
		.SetNormalPadding(FMargin(6, 3, 6, 3))
		.SetPressedPadding(FMargin(6, 4, 6, 2));
	Style->Set("RenderStudio.Button", GlossyButton);

	const FCheckBoxStyle ToggleButton = FCheckBoxStyle()
		.SetCheckBoxType(ESlateCheckBoxType::ToggleButton)
		.SetUncheckedImage(STYLE_BOX("Style/Button", ButtonMargin))
		.SetUncheckedHoveredImage(STYLE_BOX("Style/Button_Hovered", ButtonMargin))
		.SetUncheckedPressedImage(STYLE_BOX("Style/Button_Pressed", ButtonMargin))
		.SetCheckedImage(STYLE_BOX("Style/Button_Checked", ButtonMargin))
		.SetCheckedHoveredImage(STYLE_BOX("Style/Button_Checked", ButtonMargin, FLinearColor(1.1f, 1.1f, 1.1f)))
		.SetCheckedPressedImage(STYLE_BOX("Style/Button_Pressed", ButtonMargin))
		.SetPadding(FMargin(4, 2, 4, 2));
	Style->Set("RenderStudio.ToggleButton", ToggleButton);

	// Text
	const FTextBlockStyle NormalText = FTextBlockStyle()
		.SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 9))
		.SetColorAndOpacity(FSlateColor(TextColor))
		.SetShadowOffset(FVector2D(1.0f, 1.0f))
		.SetShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.8f));
	Style->Set("RenderStudio.Text", NormalText);
	Style->Set("RenderStudio.Text.Bold", FTextBlockStyle(NormalText).SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 9)));
	Style->Set("RenderStudio.Text.Header", FTextBlockStyle(NormalText).SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 9)).SetColorAndOpacity(FSlateColor(FLinearColor(FColor(235, 235, 235)))));
	Style->Set("RenderStudio.Text.Subdued", FTextBlockStyle(NormalText).SetColorAndOpacity(FSlateColor(SubduedTextColor)));
	Style->Set("RenderStudio.Text.Title", FTextBlockStyle(NormalText).SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12)).SetColorAndOpacity(FSlateColor(AccentColor)));

	// List rows: orange selection like the classic look-dev tools
	const FTableRowStyle RowStyle = FTableRowStyle(FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
		.SetEvenRowBackgroundBrush(FSlateColorBrush(PanelColor))
		.SetOddRowBackgroundBrush(FSlateColorBrush(FLinearColor(FColor(63, 63, 67))))
		.SetEvenRowBackgroundHoveredBrush(STYLE_BOX("Style/RowHover", FMargin(2.0f / 8.0f, 2.0f / 18.0f)))
		.SetOddRowBackgroundHoveredBrush(STYLE_BOX("Style/RowHover", FMargin(2.0f / 8.0f, 2.0f / 18.0f)))
		.SetActiveBrush(STYLE_BOX("Style/Selection", FMargin(2.0f / 8.0f, 2.0f / 18.0f)))
		.SetActiveHoveredBrush(STYLE_BOX("Style/Selection", FMargin(2.0f / 8.0f, 2.0f / 18.0f)))
		.SetInactiveBrush(STYLE_BOX("Style/Selection", FMargin(2.0f / 8.0f, 2.0f / 18.0f), FLinearColor(0.7f, 0.7f, 0.7f)))
		.SetInactiveHoveredBrush(STYLE_BOX("Style/Selection", FMargin(2.0f / 8.0f, 2.0f / 18.0f), FLinearColor(0.7f, 0.7f, 0.7f)))
		.SetTextColor(FSlateColor(TextColor))
		.SetSelectedTextColor(FSlateColor(FLinearColor::White));
	Style->Set("RenderStudio.TableRow", RowStyle);

	// Icons
	Style->Set("RenderStudio.Icon.Mesh", new STYLE_IMAGE("Style/Icon_Mesh", Icon16));
	Style->Set("RenderStudio.Icon.Sphere", new STYLE_IMAGE("Style/Icon_Sphere", Icon16));
	Style->Set("RenderStudio.Icon.Light", new STYLE_IMAGE("Style/Icon_Light", Icon16));
	Style->Set("RenderStudio.Icon.Sky", new STYLE_IMAGE("Style/Icon_Sky", Icon16));
	Style->Set("RenderStudio.Icon.Camera", new STYLE_IMAGE("Style/Icon_Camera", Icon16));
	Style->Set("RenderStudio.Icon.Material", new STYLE_IMAGE("Style/Icon_Material", Icon16));
	Style->Set("RenderStudio.Icon.Visible", new STYLE_IMAGE("Style/Icon_Visible", Icon16));
	Style->Set("RenderStudio.Icon.Hidden", new STYLE_IMAGE("Style/Icon_Hidden", Icon16));
	Style->Set("RenderStudio.AppIcon", new STYLE_IMAGE("Icon128", FVector2D(128.0f, 128.0f)));
	Style->Set("RenderStudio.AppIcon.Small", new STYLE_IMAGE("Icon128", Icon16));

	// Toolbar command icons (borrowed from the editor so commands look right in menus too)
	Style->Set("RenderStudio.AddCube", new STYLE_IMAGE("Style/Icon_Mesh", Icon16));
	Style->Set("RenderStudio.AddSphere", new STYLE_IMAGE("Style/Icon_Sphere", Icon16));
	Style->Set("RenderStudio.AddPointLight", new STYLE_IMAGE("Style/Icon_Light", Icon16));
	Style->Set("RenderStudio.NewMaterial", new STYLE_IMAGE("Style/Icon_Material", Icon16));

	return Style;
}

#undef STYLE_IMAGE
#undef STYLE_BOX
