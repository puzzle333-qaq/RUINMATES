#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"

// sRGB design tokens from Documentation/UI_GUIDE.md (Ruinmates_UI_Guide.md).
// Alpha is applied after the sRGB to linear conversion so it is not doubled.
namespace RuinmatesUI
{
	inline FLinearColor Token(const TCHAR* Hex, float Alpha)
	{
		FLinearColor Linear = FLinearColor::FromSRGBColor(FColor::FromHex(Hex));
		Linear.A = Alpha;
		return Linear;
	}

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

	// Existing ERuinmatesItemQuality, in order, uses the guide's five rarity colors.
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

	inline FSlateFontInfo Font(int32 Size)
	{
		return FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}
}
