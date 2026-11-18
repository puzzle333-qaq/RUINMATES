#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Fonts/SlateFontInfo.h"

class UTexture2D;
class UImage;

// Ruinmates terminal design system (see Documentation/UI_Terminal.md).
//
// Colours are sRGB design tokens; Token() converts to linear and only then applies alpha so the
// alpha is not doubled. Brushes come from the generated textures in /Game/RUINMATES/UI/Textures
// (Scripts/UI/generate_terminal_ui.py). Every brush helper is null-safe: if a texture is missing
// (fresh checkout without the assets, -NullRHI tests) it degrades to a flat tinted brush.
namespace RuinmatesUI
{
	inline FLinearColor Token(const TCHAR* Hex, float Alpha)
	{
		FLinearColor Linear = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
		Linear.A = Alpha;
		return Linear;
	}

	inline FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A = Alpha;
		return Color;
	}

	// ---- V2 palette -------------------------------------------------------------------
	inline FLinearColor ShellDark() { return Token(TEXT("08121D"), 1.f); }
	inline FLinearColor Metal() { return Token(TEXT("263B50"), 1.f); }
	inline FLinearColor MetalHighlight() { return Token(TEXT("718CA3"), 1.f); }
	inline FLinearColor Screen() { return Token(TEXT("0A1626"), 0.84f); }
	inline FLinearColor PanelFill() { return Token(TEXT("102A43"), 0.72f); }
	inline FLinearColor SlotFill() { return Token(TEXT("142C44"), 0.75f); }
	inline FLinearColor Stroke() { return Token(TEXT("365C76"), 0.60f); }
	inline FLinearColor StrokeHighlight() { return Token(TEXT("66C9F3"), 1.f); }
	inline FLinearColor Cyan() { return Token(TEXT("42D4F4"), 1.f); }
	inline FLinearColor TextWeak() { return Token(TEXT("648197"), 1.f); }
	inline FLinearColor Money() { return Token(TEXT("E7BF6B"), 1.f); }
	inline FLinearColor Error() { return Token(TEXT("E17B83"), 1.f); }

	// ---- V1 tokens kept for existing callers (trade screen) ----------------------------
	inline FLinearColor Overlay() { return Token(TEXT("040B14"), 0.44f); }
	inline FLinearColor Shell() { return Token(TEXT("0A1626"), 0.84f); }
	inline FLinearColor Panel() { return Token(TEXT("102A43"), 0.74f); }
	inline FLinearColor PanelElevated() { return Token(TEXT("153651"), 0.86f); }
	inline FLinearColor SlotEmpty() { return Token(TEXT("142C44"), 0.72f); }
	inline FLinearColor SlotHover() { return Token(TEXT("23516D"), 0.76f); }
	inline FLinearColor Line() { return Token(TEXT("547D9D"), 0.52f); }
	inline FLinearColor LineStrong() { return Token(TEXT("83B8D6"), 0.85f); }
	inline FLinearColor Accent() { return Token(TEXT("66C9F3"), 1.f); }
	inline FLinearColor AccentFill() { return Token(TEXT("2C7DA6"), 0.28f); }
	inline FLinearColor TextPrimary() { return Token(TEXT("E7F2FA"), 1.f); }
	inline FLinearColor TextSecondary() { return Token(TEXT("9DB7CA"), 1.f); }
	inline FLinearColor TextMuted() { return Token(TEXT("6C879E"), 1.f); }
	inline FLinearColor Success() { return Token(TEXT("65C5A3"), 1.f); }
	inline FLinearColor Warning() { return Token(TEXT("E8B56C"), 1.f); }
	inline FLinearColor Danger() { return Token(TEXT("E77D80"), 1.f); }

	// Existing ERuinmatesItemQuality, in order: Common, Fine, Advanced, Rare, Precious.
	inline FLinearColor QualityColor(uint8 QualityIndex)
	{
		switch (QualityIndex)
		{
		case 1: return Token(TEXT("72C49D"), 1.f);
		case 2: return Token(TEXT("69ACF0"), 1.f);
		case 3: return Token(TEXT("B394ED"), 1.f);
		case 4: return Token(TEXT("E7BF6B"), 1.f);
		default: return Token(TEXT("A6B5C7"), 1.f);
		}
	}

	// ---- Typography ---------------------------------------------------------------------
	// Engine default composite font: Roboto for Latin, the engine CJK fallback for Chinese.
	inline FSlateFontInfo Font(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	inline FSlateFontInfo FontBold(int32 Size, int32 LetterSpacing = 0)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), Size);
		Info.LetterSpacing = LetterSpacing;
		return Info;
	}

	// ---- Textures and brushes ----------------------------------------------------------------
	/** Loads /Game/RUINMATES/UI/Textures/<Group>/<Name>. Cached; returns null when missing. */
	RUINMATES_API UTexture2D* Texture(const TCHAR* Group, const TCHAR* Name);

	/** Loads an absolute texture path (item icons from DT_Items). Cached; null-safe. */
	RUINMATES_API UTexture2D* TextureAt(const FSoftObjectPath& Path);

	/** Nine-slice brush. Margin is a UV fraction; the brush draws at half the texture's pixels. */
	RUINMATES_API FSlateBrush Box(const TCHAR* Group, const TCHAR* Name, const FMargin& Margin, const FLinearColor& Tint = FLinearColor::White);

	/** Plain stretched image brush at half the texture's pixel size. */
	RUINMATES_API FSlateBrush Image(const TCHAR* Group, const TCHAR* Name, const FLinearColor& Tint = FLinearColor::White);

	/** Image brush for an already loaded texture (item icons). Transparent when null. */
	RUINMATES_API FSlateBrush ImageFor(UTexture2D* Texture, const FLinearColor& Tint = FLinearColor::White);

	/** Tiled image brush (noise, hatch). */
	RUINMATES_API FSlateBrush Tiled(const TCHAR* Group, const TCHAR* Name, const FLinearColor& Tint = FLinearColor::White);

	// Named brushes shared by every terminal page. Margins match manifest.json.
	RUINMATES_API FSlateBrush PanelBrush();
	RUINMATES_API FSlateBrush InsetBrush();
	RUINMATES_API FSlateBrush ChipBrush();
	RUINMATES_API FSlateBrush KeycapBrush();
	RUINMATES_API FSlateBrush SlotBrush(const TCHAR* Name, const FLinearColor& Tint = FLinearColor::White);

	/** Chamfered terminal button (primary action). */
	RUINMATES_API FButtonStyle ButtonStyle();

	/** Category tab; bSelected swaps the normal brush for the lit one. */
	RUINMATES_API FButtonStyle TabStyle(bool bSelected);

	/** Square close button with the red hover state. */
	RUINMATES_API FButtonStyle CloseButtonStyle();

	/** Grooved track with a white fill that is tinted per bar via SetFillColorAndOpacity. */
	RUINMATES_API FProgressBarStyle ProgressStyle();

	/**
	 * Sets the drawn size of an image. Use this instead of UImage::SetDesiredSizeOverride: that call
	 * is a no-op until the Slate widget exists, so trees built in NativeOnInitialized lose it and
	 * fall back to the texture size. The brush ImageSize is stored on the UImage and always applies.
	 */
	RUINMATES_API void SetImageSize(UImage* Image, const FVector2D& Size);

	/** Thin cyan scroll thumb, no engine grey track. */
	RUINMATES_API FScrollBarStyle ScrollBarStyle();
}
