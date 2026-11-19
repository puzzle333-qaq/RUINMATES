#include "UI/RuinmatesTerminalItemSlot.h"

#include "Blueprint/WidgetTree.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/TextBlock.h"
#include "InputCoreTypes.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "UI/RuinmatesUIStyle.h"

namespace
{
	// Plate textures indexed by the button style we push. Kept in one table so ApplyPlateStyle
	// only rebuilds the FButtonStyle when the plate actually changes.
	enum class EPlate : int32
	{
		Empty,
		Filled,
		Unknown,
		Searching,
		Disabled
	};

	UOverlaySlot* AddLayer(UOverlay* Layer, UWidget* Child, EHorizontalAlignment H, EVerticalAlignment V, const FMargin& Padding = FMargin(0.f))
	{
		UOverlaySlot* LayerSlot = Layer->AddChildToOverlay(Child);
		if (LayerSlot)
		{
			LayerSlot->SetHorizontalAlignment(H);
			LayerSlot->SetVerticalAlignment(V);
			LayerSlot->SetPadding(Padding);
		}
		// Every display layer is invisible to hit testing; the Button owns the whole cell.
		Child->SetVisibility(ESlateVisibility::HitTestInvisible);
		return LayerSlot;
	}

	void Show(UWidget* Widget, bool bShow)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}

	FText QualityLine(const FRuinmatesTerminalSlotView& View)
	{
		return RuinmatesItems::GetQualityName(View.Quality);
	}
}

void URuinmatesTerminalItemSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTree();
	RefreshLayoutMetrics();
	RefreshVisual();
}

void URuinmatesTerminalItemSlot::BuildTree()
{
	SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSize"));
	SlotSize->SetWidthOverride(CellSize.X);
	SlotSize->SetHeightOverride(CellSize.Y);
	WidgetTree->RootWidget = SlotSize;

	HitButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Hit"));
	HitButton->OnClicked.AddDynamic(this, &URuinmatesTerminalItemSlot::HandleClicked);
	HitButton->OnHovered.AddDynamic(this, &URuinmatesTerminalItemSlot::HandleHovered);
	HitButton->OnUnhovered.AddDynamic(this, &URuinmatesTerminalItemSlot::HandleUnhovered);
	SlotSize->SetContent(HitButton);

	UOverlay* Layer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layer"));
	HitButton->AddChild(Layer);
	// ROOT CAUSE of the old "tiny slot": UButtonSlot defaults to HAlign_Center / VAlign_Center, so
	// the button shrank its content to the desired size of a 28px text glyph while the SizeBox
	// still reserved 184x112. The content must fill the whole button.
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Layer->Slot))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(0.f));
	}

	QualityGlow = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("QualityGlow"));
	QualityGlow->SetBrush(RuinmatesUI::Box(TEXT("Slots"), TEXT("T_UI_Slot_QualityGlow"), FMargin(0.f, 0.f, 0.f, 0.25f)));
	AddLayer(Layer, QualityGlow, HAlign_Fill, VAlign_Fill, FMargin(2.f));

	Hatch = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Hatch"));
	Hatch->SetBrush(RuinmatesUI::Tiled(TEXT("Slots"), TEXT("T_UI_HatchTile")));
	AddLayer(Layer, Hatch, HAlign_Fill, VAlign_Fill, FMargin(3.f, 3.f, 3.f, 10.f));

	ScanBand = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ScanBand"));
	ScanBand->SetBrush(RuinmatesUI::Image(TEXT("Decorations"), TEXT("T_UI_ScanPattern")));
	RuinmatesUI::SetImageSize(ScanBand, FVector2D(64.f, 48.f));
	AddLayer(Layer, ScanBand, HAlign_Fill, VAlign_Top, FMargin(3.f, 0.f));

	EmptyMark = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("EmptyMark"));
	EmptyMark->SetBrush(RuinmatesUI::Image(TEXT("Slots"), TEXT("T_UI_Slot_EmptyMark")));
	RuinmatesUI::SetImageSize(EmptyMark, FVector2D(44.f, 44.f));
	AddLayer(Layer, EmptyMark, HAlign_Center, VAlign_Center);

	IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("IconBox"));
	ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
	UScaleBox* IconFit = WidgetTree->ConstructWidget<UScaleBox>();
	IconFit->SetStretch(EStretch::ScaleToFit);
	IconFit->SetContent(ItemIcon);
	IconBox->SetContent(IconFit);
	AddLayer(Layer, IconBox, HAlign_Center, VAlign_Top);

	UnknownBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("UnknownBox"));
	UnknownIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("UnknownIcon"));
	UnknownBox->SetContent(UnknownIcon);
	AddLayer(Layer, UnknownBox, HAlign_Center, VAlign_Center);

	NameStrip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("NameStrip"));
	NameStrip->SetBrush(RuinmatesUI::Image(TEXT("Slots"), TEXT("T_UI_Slot_NameStrip")));
	RuinmatesUI::SetImageSize(NameStrip, FVector2D(32.f, 44.f));
	AddLayer(Layer, NameStrip, HAlign_Fill, VAlign_Bottom, FMargin(2.f, 0.f, 2.f, 2.f));

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	NameText->SetAutoWrapText(false);
	NameText->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
	NameText->SetShadowOffset(FVector2D(0.f, 1.f));
	NameText->SetShadowColorAndOpacity(RuinmatesUI::Token(TEXT("02060B"), 0.8f));
	AddLayer(Layer, NameText, HAlign_Fill, VAlign_Bottom);

	CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Count"));
	CountText->SetJustification(ETextJustify::Right);
	CountText->SetShadowOffset(FVector2D(0.f, 1.f));
	CountText->SetShadowColorAndOpacity(RuinmatesUI::Token(TEXT("02060B"), 0.9f));
	AddLayer(Layer, CountText, HAlign_Right, VAlign_Top);

	RarityPips = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RarityPips"));
	for (int32 Index = 0; Index < 5; ++Index)
	{
		UImage* Pip = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(*FString::Printf(TEXT("Pip_%d"), Index)));
		Pip->SetBrush(RuinmatesUI::Image(TEXT("Slots"), TEXT("T_UI_RarityPip")));
		RuinmatesUI::SetImageSize(Pip, FVector2D(9.f, 9.f));
		if (UHorizontalBoxSlot* PipSlot = RarityPips->AddChildToHorizontalBox(Pip))
		{
			PipSlot->SetPadding(FMargin(0.f, 0.f, 3.f, 0.f));
			PipSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	AddLayer(Layer, RarityPips, HAlign_Left, VAlign_Top);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
	StatusText->SetJustification(ETextJustify::Center);
	AddLayer(Layer, StatusText, HAlign_Center, VAlign_Bottom);

	ScanBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Scan"));
	ScanBar->SetWidgetStyle(RuinmatesUI::ProgressStyle());
	ScanBar->SetFillColorAndOpacity(RuinmatesUI::Cyan());
	USizeBox* ScanSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ScanSize"));
	ScanSize->SetHeightOverride(8.f);
	ScanSize->SetContent(ScanBar);
	AddLayer(Layer, ScanSize, HAlign_Fill, VAlign_Bottom);
	ScanBar->SetVisibility(ESlateVisibility::HitTestInvisible);

	QualityFrame = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("QualityFrame"));
	QualityFrame->SetBrush(RuinmatesUI::SlotBrush(TEXT("T_UI_Slot_Frame")));
	AddLayer(Layer, QualityFrame, HAlign_Fill, VAlign_Fill);

	Selection = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Selection"));
	Selection->SetBrush(RuinmatesUI::V4(TEXT("Derived/Slots/T_UI_SlotOverlay_Selected")));
	AddLayer(Layer, Selection, HAlign_Fill, VAlign_Fill);
	FinishPips = WidgetTree->ConstructWidget<UImage>();
	RuinmatesUI::SetImageSize(FinishPips, FVector2D(40, 6));
	AddLayer(Layer, FinishPips, HAlign_Left, VAlign_Bottom, FMargin(10, 0, 0, 2));
}

void URuinmatesTerminalItemSlot::Configure(const FVector2D& InSize, ERuinmatesTerminalSlotLayout InLayout)
{
	const bool bLayoutChanged = Layout != InLayout;
	const bool bSizeChanged = !CellSize.Equals(InSize, 0.5f);
	CellSize = InSize;
	Layout = InLayout;
	if (SlotSize && bSizeChanged)
	{
		SlotSize->SetWidthOverride(CellSize.X);
		SlotSize->SetHeightOverride(CellSize.Y);
	}
	if (bSizeChanged || bLayoutChanged)
	{
		RefreshLayoutMetrics();
		RefreshVisual();
	}
}

void URuinmatesTerminalItemSlot::RefreshLayoutMetrics()
{
	if (!IconBox || !NameText)
	{
		return;
	}
	const bool bCard = Layout == ERuinmatesTerminalSlotLayout::Card;
	// Icon takes ~60% of the short side; on cards it sits above the name block.
	const float Short = FMath::Min(CellSize.X, CellSize.Y);
	IconBox->SetWidthOverride(CellSize.X - 60.f);
	IconBox->SetHeightOverride(CellSize.Y - 40.f);
	UnknownBox->SetWidthOverride(Short * 0.40f);
	UnknownBox->SetHeightOverride(Short * 0.40f);

	const float Pad = FMath::Clamp(Short * 0.07f, 8.f, 14.f);
	if (UOverlaySlot* IconSlot = Cast<UOverlaySlot>(IconBox->Slot))
	{
		IconSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
	if (UOverlaySlot* UnknownSlot = Cast<UOverlaySlot>(UnknownBox->Slot))
	{
		UnknownSlot->SetPadding(FMargin(0.f, 0.f, 0.f, bCard ? CellSize.Y * 0.12f : Short * 0.06f));
	}
	if (UOverlaySlot* NameSlot = Cast<UOverlaySlot>(NameText->Slot))
	{
		NameSlot->SetPadding(FMargin(10.f, 0.f, 10.f, 8.f));
	}
	if (UOverlaySlot* CountSlot = Cast<UOverlaySlot>(CountText->Slot))
	{
		CountSlot->SetPadding(FMargin(0.f, 6.f, 8.f, 0.f));
	}
	if (UOverlaySlot* PipSlot = Cast<UOverlaySlot>(RarityPips->Slot))
	{
		PipSlot->SetPadding(FMargin(Pad, Pad + 2.f, 0.f, 0.f));
	}
	if (UOverlaySlot* StatusSlot = Cast<UOverlaySlot>(StatusText->Slot))
	{
		StatusSlot->SetPadding(FMargin(Pad, 0.f, Pad, Pad + 18.f));
	}
	if (UWidget* ScanSize = ScanBar ? ScanBar->GetParent() : nullptr)
	{
		if (UOverlaySlot* ScanSlot = Cast<UOverlaySlot>(ScanSize->Slot))
		{
			ScanSlot->SetPadding(FMargin(Pad, 0.f, Pad, Pad));
		}
	}

	NameText->SetFont(RuinmatesUI::Font(16));
	CountText->SetFont(RuinmatesUI::Font(16));
	StatusText->SetFont(RuinmatesUI::Font(14));
	RuinmatesUI::SetImageSize(NameStrip, FVector2D(32.f, bCard ? 76.f : 52.f));
	NameText->SetJustification(ETextJustify::Left);
}

void URuinmatesTerminalItemSlot::ApplyView(const FRuinmatesTerminalSlotView& InView)
{
	const bool bWasHidden = View.State == ERuinmatesTerminalSlotState::Unknown || View.State == ERuinmatesTerminalSlotState::Searching;
	const bool bNowRevealed = InView.State == ERuinmatesTerminalSlotState::Revealed;
	if (bWasHidden && bNowRevealed && InView.Quality >= ERuinmatesItemQuality::Rare)
	{
		RevealFlash = 0.3f;
	}
	if (View.State != InView.State)
	{
		ScanTime = 0.f;
	}
	View = InView;
	RefreshVisual();
}

void URuinmatesTerminalItemSlot::ApplyPlateStyle()
{
	if (!HitButton)
	{
		return;
	}
	EPlate Plate = EPlate::Filled;
	switch (View.State)
	{
	case ERuinmatesTerminalSlotState::Empty: Plate = EPlate::Empty; break;
	case ERuinmatesTerminalSlotState::Unknown: Plate = EPlate::Unknown; break;
	case ERuinmatesTerminalSlotState::Searching: Plate = EPlate::Searching; break;
	case ERuinmatesTerminalSlotState::Taken: Plate = EPlate::Disabled; break;
	default: Plate = View.bDimmed ? EPlate::Disabled : EPlate::Filled; break;
	}
	const int32 FinishKey = static_cast<int32>(Plate) * 10 + static_cast<int32>(View.Quality);
	if (AppliedPlate == FinishKey)
	{
		return;
	}
	AppliedPlate = FinishKey;

	const bool bKnown = View.State == ERuinmatesTerminalSlotState::Revealed || View.State == ERuinmatesTerminalSlotState::Pending;
	const FString Name = bKnown ? FString::Printf(TEXT("Derived/Slots/T_UI_Slot_%s"),
		RuinmatesUI::QualityFinish(static_cast<uint8>(View.Quality))) : TEXT("Derived/Slots/T_UI_Slot_Empty");
	FButtonStyle V4Style;
	V4Style.Normal = RuinmatesUI::V4(*Name);
	V4Style.Hovered = V4Style.Normal;
	V4Style.Pressed = V4Style.Normal;
	V4Style.Disabled = V4Style.Normal;
	V4Style.NormalPadding = FMargin(0.f);
	V4Style.PressedPadding = FMargin(0.f);
	HitButton->SetStyle(V4Style);
}

void URuinmatesTerminalItemSlot::RefreshVisual()
{
	if (!HitButton || !NameText || !ItemIcon)
	{
		return;
	}

	const ERuinmatesTerminalSlotState State = View.State;
	const bool bKnown = State == ERuinmatesTerminalSlotState::Revealed || State == ERuinmatesTerminalSlotState::Pending;
	const bool bEmpty = State == ERuinmatesTerminalSlotState::Empty;
	const bool bUnknown = State == ERuinmatesTerminalSlotState::Unknown;
	const bool bSearching = State == ERuinmatesTerminalSlotState::Searching;
	const bool bTaken = State == ERuinmatesTerminalSlotState::Taken;
	const bool bCard = Layout == ERuinmatesTerminalSlotLayout::Card;
	const FLinearColor Quality = RuinmatesUI::QualityColor(static_cast<uint8>(View.Quality));
	const float Dim = View.bDimmed ? 0.28f : 1.f;

	ApplyPlateStyle();
	// Pending (request in flight), taken and filtered cells cannot be clicked again.
	HitButton->SetIsEnabled(!(State == ERuinmatesTerminalSlotState::Pending || bTaken || View.bDimmed));

	// Quality: low-alpha gradient that peaks in a thin bottom line, plus a tinted frame.
	Show(QualityGlow, false);
	QualityGlow->SetColorAndOpacity(RuinmatesUI::WithAlpha(Quality, (bHovered ? 0.55f : 0.42f) * Dim));
	Show(QualityFrame, false);
	const float FrameAlpha = (bHovered ? 0.95f : 0.62f) + RevealFlash * 1.2f;
	QualityFrame->SetColorAndOpacity(RuinmatesUI::WithAlpha(Quality, FMath::Min(1.f, FrameAlpha) * Dim));

	Show(Hatch, bUnknown || bSearching);
	Hatch->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bSearching ? 0.9f : 0.6f));
	Show(ScanBand, bSearching);
	Show(EmptyMark, false);
	EmptyMark->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, bTaken ? 0.35f : (bHovered ? 1.f : 0.75f)));

	// Item art.
	Show(IconBox, bKnown);
	if (bKnown)
	{
		static const FSoftObjectPath Generic(TEXT("/Game/RUINMATES/UI/AssetPackV4/Icons/System/T_Icon_SYS_Inventory.T_Icon_SYS_Inventory"));
		const FSoftObjectPath& Wanted = View.Icon.IsNull() ? Generic : View.Icon;
		if (AppliedIcon != Wanted)
		{
			UTexture2D* IconTexture = RuinmatesUI::TextureAt(Wanted);
			if (!IconTexture)
			{
				IconTexture = RuinmatesUI::TextureAt(Generic);
			}
			ItemIcon->SetBrush(RuinmatesUI::ImageFor(IconTexture));
			AppliedIcon = Wanted;
		}
		ItemIcon->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, State == ERuinmatesTerminalSlotState::Pending ? 0.5f : Dim));
	}

	Show(UnknownBox, bUnknown || bSearching);
	if (bUnknown || bSearching)
	{
		UnknownIcon->SetBrush(bSearching
			? RuinmatesUI::Image(TEXT("Icons"), TEXT("T_UI_Icon_Scan"))
			: RuinmatesUI::Image(TEXT("Icons"), TEXT("T_UI_Icon_Unknown")));
		UnknownIcon->SetColorAndOpacity(bSearching ? RuinmatesUI::Cyan() : RuinmatesUI::WithAlpha(RuinmatesUI::TextWeak(), 0.9f));
	}

	// Text.
	Show(NameStrip, false);
	Show(NameText, bKnown);
	Show(CountText, bKnown && View.Count > 1);
	if (bKnown)
	{
		NameText->SetText(View.Name);
		NameText->SetColorAndOpacity(FSlateColor(RuinmatesUI::WithAlpha(RuinmatesUI::TextPrimary(), Dim)));
		CountText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "CountFmt", "×{0}"), FText::AsNumber(View.Count)));
		CountText->SetColorAndOpacity(FSlateColor(RuinmatesUI::WithAlpha(RuinmatesUI::TextPrimary(), Dim)));
	}

	// Rarity pips: one per quality step, so rarity reads without relying on colour alone.
	Show(RarityPips, false);
	const int32 PipCount = static_cast<int32>(View.Quality) + 1;
	for (int32 Index = 0; Index < RarityPips->GetChildrenCount(); ++Index)
	{
		if (UImage* Pip = Cast<UImage>(RarityPips->GetChildAt(Index)))
		{
			Pip->SetVisibility(Index < PipCount ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			Pip->SetColorAndOpacity(Quality);
		}
	}

	Show(StatusText, bSearching || (bUnknown && bCard) || (bTaken && bCard));
	if (bSearching)
	{
		StatusText->SetText(NSLOCTEXT("RuinMatesUI", "SlotScanning", "扫描中…"));
		StatusText->SetColorAndOpacity(FSlateColor(RuinmatesUI::Cyan()));
	}
	else if (bTaken)
	{
		StatusText->SetText(NSLOCTEXT("RuinMatesUI", "SlotTaken", "已取走"));
		StatusText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextWeak()));
	}
	else
	{
		StatusText->SetText(NSLOCTEXT("RuinMatesUI", "SlotUnknown", "未识别"));
		StatusText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextWeak()));
	}

	// The per-item scan bar is a visual loop only: the server reveal timer owns the real timing.
	Show(ScanBar ? ScanBar->GetParent() : nullptr, bSearching);
	Show(ScanBar, bSearching);

	Show(Selection, bKnown && (View.bSelected || bHovered) && !View.bDimmed);
	Selection->SetBrush(RuinmatesUI::V4(View.bSelected
		? TEXT("Derived/Slots/T_UI_SlotOverlay_Selected")
		: TEXT("Derived/Slots/T_UI_SlotOverlay_Hover")));
	Selection->SetRenderOpacity(View.bSelected ? 1.f : 0.45f);
	Show(FinishPips, bKnown && !View.bDimmed);
	if (bKnown)
	{
		FinishPips->SetBrush(RuinmatesUI::V4(*FString::Printf(TEXT("Art/Slots/T_UI_QualityPips_%s"),
			RuinmatesUI::QualityFinish(static_cast<uint8>(View.Quality)))));
		RuinmatesUI::SetImageSize(FinishPips, FVector2D(40, 6));
	}

	// Tooltip with the details that do not fit on the cell. Hidden items never leak content.
	if (bKnown && !View.bDimmed)
	{
		FText Tip = FText::Format(NSLOCTEXT("RuinMatesUI", "SlotTooltip", "{0}\n品质：{1}    数量：{2}"),
			View.Name, QualityLine(View), FText::AsNumber(View.Count));
		if (View.UnitValue > 0)
		{
			// Unit price and stack value are both part of the item readout.
			Tip = FText::Format(NSLOCTEXT("RuinMatesUI", "SlotTooltipValue", "{0}\n单价：{1}    参考价值：{2}"),
				Tip, FText::AsNumber(View.UnitValue), FText::AsNumber(View.UnitValue * View.Count));
		}
		if (!View.Description.IsEmpty())
		{
			Tip = FText::Format(NSLOCTEXT("RuinMatesUI", "SlotTooltipDesc", "{0}\n{1}"), Tip, View.Description);
		}
		if (!DetailTooltip)
		{
			DetailTooltip = WidgetTree->ConstructWidget<USizeBox>();
			DetailTooltip->SetWidthOverride(320.f);
			UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
			Panel->SetBrush(RuinmatesUI::PanelBrush());
			Panel->SetPadding(FMargin(16.f));
			DetailText = WidgetTree->ConstructWidget<UTextBlock>();
			DetailText->SetFont(RuinmatesUI::Font(16));
			DetailText->SetColorAndOpacity(RuinmatesUI::TextPrimary());
			DetailText->SetAutoWrapText(true);
			Panel->SetContent(DetailText);
			DetailTooltip->SetContent(Panel);
			DetailTooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		DetailText->SetText(Tip);
		SetToolTip(DetailTooltip);
	}
	else
	{
		SetToolTip(nullptr);
	}
}

void URuinmatesTerminalItemSlot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (RevealFlash > 0.f)
	{
		RevealFlash = FMath::Max(0.f, RevealFlash - InDeltaTime);
		const FLinearColor Quality = RuinmatesUI::QualityColor(static_cast<uint8>(View.Quality));
		QualityFrame->SetColorAndOpacity(RuinmatesUI::WithAlpha(Quality, FMath::Min(1.f, 0.62f + RevealFlash * 1.2f)));
	}

	// Only searching cells animate; everything else stays idle.
	if (View.State != ERuinmatesTerminalSlotState::Searching || !ScanBar || !ScanBand)
	{
		return;
	}
	ScanTime = FMath::Fmod(ScanTime + InDeltaTime / 1.2f, 1.f);
	ScanBar->SetPercent(ScanTime);
	const float Travel = FMath::Max(0.f, CellSize.Y - 48.f);
	ScanBand->SetRenderTranslation(FVector2D(0.f, Travel * ScanTime));
	ScanBand->SetRenderOpacity(FMath::Sin(ScanTime * PI) * 0.9f + 0.1f);
}

FReply URuinmatesTerminalItemSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// The button only consumes the left button, so right clicks bubble up to here.
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton
		&& View.State == ERuinmatesTerminalSlotState::Revealed && !View.bDimmed)
	{
		OnSlotRightClicked.Broadcast(View.StableIndex);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URuinmatesTerminalItemSlot::HandleClicked()
{
	if (View.State == ERuinmatesTerminalSlotState::Revealed && !View.bDimmed)
	{
		OnSlotClicked.Broadcast(View.StableIndex);
	}
}

void URuinmatesTerminalItemSlot::HandleHovered()
{
	bHovered = true;
	RefreshVisual();
}

void URuinmatesTerminalItemSlot::HandleUnhovered()
{
	bHovered = false;
	RefreshVisual();
}