#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * The Render Studio skin: an early-2010s desktop DCC look (grey gradients,
 * glossy buttons, orange selection) built from the bitmaps in Resources/Style.
 */
class FRenderStudioStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

	/** Shorthand for Get().GetBrush(Name) */
	static const FSlateBrush* GetBrush(FName PropertyName);

	/** Common colours, so widgets that draw solid fills stay consistent with the bitmaps */
	static const FLinearColor PanelColor;
	static const FLinearColor PanelDarkColor;
	static const FLinearColor AccentColor;
	static const FLinearColor TextColor;
	static const FLinearColor SubduedTextColor;

private:
	static TSharedRef<FSlateStyleSet> Create();

	static TSharedPtr<FSlateStyleSet> StyleInstance;
};
