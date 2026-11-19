#include "UI/RuinmatesUIStyle.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPath.h"

namespace
{
	// Root of the generated texture set. Matches Scripts/UI/import_terminal_ui.py.
	const TCHAR* TextureRoot = TEXT("/Game/RUINMATES/UI/Textures");

	TMap<FString, TWeakObjectPtr<UTexture2D>>& TextureCache()
	{
		static TMap<FString, TWeakObjectPtr<UTexture2D>> Cache;
		return Cache;
	}

	// Remember misses too, so a missing asset is only looked up once per session.
	TSet<FString>& MissingTextures()
	{
		static TSet<FString> Missing;
		return Missing;
	}

	UTexture2D* LoadCached(const FString& ObjectPath)
	{
		if (TWeakObjectPtr<UTexture2D>* Found = TextureCache().Find(ObjectPath))
		{
			if (UTexture2D* Texture = Found->Get())
			{
				return Texture;
			}
		}
		if (MissingTextures().Contains(ObjectPath))
		{
			return nullptr;
		}
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!IsValid(Texture))
		{
			MissingTextures().Add(ObjectPath);
			UE_LOG(LogTemp, Verbose, TEXT("RuinmatesUI: texture %s is missing, using a flat brush."), *ObjectPath);
			return nullptr;
		}
		TextureCache().Add(ObjectPath, Texture);
		return Texture;
	}

	// Plain images are authored at 2 px per design unit and drawn at half size. 9-slice (Box)
	// textures are authored at 1 px per unit: Slate's box batcher sizes the margins from the
	// texture's real pixel count and ignores ImageSize, so a 2x texture would draw 2x borders.
	FVector2D DesignSize(const UTexture2D* Texture, ESlateBrushDrawType::Type DrawAs)
	{
		if (!Texture)
		{
			return FVector2D(32.f, 32.f);
		}
		const float Scale = DrawAs == ESlateBrushDrawType::Image ? 0.5f : 1.f;
		return FVector2D(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale);
	}

	FSlateBrush MakeBrush(UTexture2D* Texture, ESlateBrushDrawType::Type DrawAs, const FMargin& Margin, const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = DrawAs;
		Brush.Margin = Margin;
		Brush.TintColor = FSlateColor(Tint);
		if (Texture)
		{
			Brush.SetResourceObject(Texture);
			Brush.SetImageSize(DesignSize(Texture, DrawAs));
		}
		else
		{
			// Missing art: draw a flat box with a dimmed tint rather than the engine checkerboard.
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.Margin = FMargin(0.f);
			Brush.TintColor = FSlateColor(RuinmatesUI::WithAlpha(Tint, Tint.A * 0.35f));
		}
		return Brush;
	}

	const FMargin ButtonMargin(0.25f, 0.3f, 0.25f, 0.3f);

	struct FV4BrushSpec
	{
		float Width;
		float Height;
		ESlateBrushDrawType::Type DrawAs;
		FMargin Margin;
	};

	const TMap<FString, FV4BrushSpec> V4Manifest = {
#include "RuinmatesV4Brushes.inl"
	};
}

FSlateBrush RuinmatesUI::V4(const TCHAR* RelativePath)
{
	const FV4BrushSpec* Spec = V4Manifest.Find(RelativePath);
	if (!Spec)
	{
		return FSlateBrush();
	}
	const FString Original(RelativePath);
	const FString Relative = Spec->DrawAs == ESlateBrushDrawType::Box && Original.StartsWith(TEXT("Art/"))
		? TEXT("Derived/") + Original : Original;
	const FString Name = FPaths::GetCleanFilename(Relative);
	const FString Path = FString::Printf(TEXT("/Game/RUINMATES/UI/AssetPackV4/%s.%s"), *Relative, *Name);
	FSlateBrush Brush = MakeBrush(LoadCached(Path), Spec->DrawAs, Spec->Margin, FLinearColor::White);
	Brush.SetImageSize(FVector2D(Spec->Width * 0.5f, Spec->Height * 0.5f));
	return Brush;
}

const TCHAR* RuinmatesUI::QualityFinish(uint8 QualityIndex)
{
	switch (QualityIndex)
	{
	case 1:
	case 2: return TEXT("Green");
	case 3: return TEXT("Purple");
	case 4: return TEXT("Gold");
	default: return TEXT("White");
	}
}

UTexture2D* RuinmatesUI::Texture(const TCHAR* Group, const TCHAR* Name)
{
	const FString Path = FString::Printf(TEXT("%s/%s/%s.%s"), TextureRoot, Group, Name, Name);
	return LoadCached(Path);
}

UTexture2D* RuinmatesUI::TextureAt(const FSoftObjectPath& Path)
{
	return Path.IsNull() ? nullptr : LoadCached(Path.ToString());
}

FSlateBrush RuinmatesUI::Box(const TCHAR* Group, const TCHAR* Name, const FMargin& Margin, const FLinearColor& Tint)
{
	return MakeBrush(Texture(Group, Name), ESlateBrushDrawType::Box, Margin, Tint);
}

FSlateBrush RuinmatesUI::Image(const TCHAR* Group, const TCHAR* Name, const FLinearColor& Tint)
{
	return MakeBrush(Texture(Group, Name), ESlateBrushDrawType::Image, FMargin(0.f), Tint);
}

FSlateBrush RuinmatesUI::ImageFor(UTexture2D* InTexture, const FLinearColor& Tint)
{
	if (!InTexture)
	{
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Empty;
	}
	return MakeBrush(InTexture, ESlateBrushDrawType::Image, FMargin(0.f), Tint);
}

FSlateBrush RuinmatesUI::Tiled(const TCHAR* Group, const TCHAR* Name, const FLinearColor& Tint)
{
	UTexture2D* Tex = Texture(Group, Name);
	if (!Tex)
	{
		FSlateBrush Empty;
		Empty.DrawAs = ESlateBrushDrawType::NoDrawType;
		return Empty;
	}
	FSlateBrush Brush = MakeBrush(Tex, ESlateBrushDrawType::Image, FMargin(0.f), Tint);
	Brush.Tiling = ESlateBrushTileType::Both;
	return Brush;
}

FSlateBrush RuinmatesUI::PanelBrush()
{
	return V4(TEXT("Art/Panels/T_UI_Panel_Large"));
}

FSlateBrush RuinmatesUI::InsetBrush()
{
	return V4(TEXT("Art/Panels/T_UI_Card_Base"));
}

FSlateBrush RuinmatesUI::ChipBrush()
{
	return V4(TEXT("Art/Panels/T_UI_Panel_Small"));
}

FSlateBrush RuinmatesUI::KeycapBrush()
{
	return V4(TEXT("Art/Controls/T_UI_KeyHint"));
}

FSlateBrush RuinmatesUI::SlotBrush(const TCHAR* Name, const FLinearColor& Tint)
{
	const float Margin = FCString::Strcmp(Name, TEXT("T_UI_Slot_Selected")) == 0 ? 24.f / 96.f : 22.f / 96.f;
	return Box(TEXT("Slots"), Name, FMargin(Margin), Tint);
}

FButtonStyle RuinmatesUI::ButtonStyle()
{
	FButtonStyle Style;
	Style.Normal = Box(TEXT("Buttons"), TEXT("T_UI_Button_Normal"), ButtonMargin);
	Style.Hovered = Box(TEXT("Buttons"), TEXT("T_UI_Button_Hover"), ButtonMargin);
	Style.Pressed = Box(TEXT("Buttons"), TEXT("T_UI_Button_Pressed"), ButtonMargin);
	Style.Disabled = Box(TEXT("Buttons"), TEXT("T_UI_Button_Disabled"), ButtonMargin);
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f, 1.f, 0.f, -1.f);
	return Style;
}

FButtonStyle RuinmatesUI::TabStyle(bool bSelected)
{
	FButtonStyle Style;
	Style.Normal = Box(TEXT("Buttons"), bSelected ? TEXT("T_UI_Tab_Selected") : TEXT("T_UI_Tab_Normal"), ButtonMargin);
	Style.Hovered = Box(TEXT("Buttons"), bSelected ? TEXT("T_UI_Tab_Selected") : TEXT("T_UI_Tab_Hover"), ButtonMargin);
	Style.Pressed = Box(TEXT("Buttons"), bSelected ? TEXT("T_UI_Tab_Selected") : TEXT("T_UI_Tab_Pressed"), ButtonMargin);
	Style.Disabled = Box(TEXT("Buttons"), TEXT("T_UI_Tab_Normal"), ButtonMargin, WithAlpha(FLinearColor::White, 0.5f));
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f, 1.f, 0.f, -1.f);
	return Style;
}

FButtonStyle RuinmatesUI::CloseButtonStyle()
{
	FButtonStyle Style;
	Style.Normal = Box(TEXT("Buttons"), TEXT("T_UI_Button_Close_Normal"), FMargin(0.3f));
	Style.Hovered = Box(TEXT("Buttons"), TEXT("T_UI_Button_Close_Hover"), FMargin(0.3f));
	Style.Pressed = Box(TEXT("Buttons"), TEXT("T_UI_Button_Close_Pressed"), FMargin(0.3f));
	Style.Disabled = Style.Normal;
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f);
	return Style;
}

FProgressBarStyle RuinmatesUI::ProgressStyle()
{
	FProgressBarStyle Style;
	Style.BackgroundImage = V4(TEXT("Art/Progress/T_UI_ProgressTrack"));
	Style.FillImage = V4(TEXT("Art/Progress/T_UI_ProgressFill_White"));
	Style.MarqueeImage = Style.FillImage;
	return Style;
}

FScrollBarStyle RuinmatesUI::ScrollBarStyle()
{
	FScrollBarStyle Style = FCoreStyle::Get().GetWidgetStyle<FScrollBarStyle>("ScrollBar");
	FSlateBrush Track;
	Track.DrawAs = ESlateBrushDrawType::Box;
	Track.TintColor = FSlateColor(Token(TEXT("040B14"), 0.55f));
	FSlateBrush Thumb;
	Thumb.DrawAs = ESlateBrushDrawType::Box;
	Thumb.TintColor = FSlateColor(WithAlpha(Accent(), 0.45f));
	FSlateBrush ThumbHot = Thumb;
	ThumbHot.TintColor = FSlateColor(WithAlpha(Accent(), 0.85f));
	Style.SetVerticalBackgroundImage(Track);
	Style.SetHorizontalBackgroundImage(Track);
	Style.SetVerticalTopSlotImage(Track);
	Style.SetVerticalBottomSlotImage(Track);
	Style.SetHorizontalTopSlotImage(Track);
	Style.SetHorizontalBottomSlotImage(Track);
	Style.SetNormalThumbImage(Thumb);
	Style.SetHoveredThumbImage(ThumbHot);
	Style.SetDraggedThumbImage(ThumbHot);
	return Style;
}

void RuinmatesUI::SetImageSize(UImage* Image, const FVector2D& Size)
{
	if (!Image || Size.X <= 0.f || Size.Y <= 0.f)
	{
		return;
	}
	FSlateBrush Brush = Image->GetBrush();
	Brush.SetImageSize(Size);
	Image->SetBrush(Brush);
}
