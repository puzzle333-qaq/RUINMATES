#include "UI/RuinmatesTerminalItemSlot.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/RuinmatesUIStyle.h"

void URuinmatesTerminalItemSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSize"));
	Size->SetWidthOverride(184.f);
	Size->SetHeightOverride(112.f);
	WidgetTree->RootWidget = Size;

	Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
	Frame->SetPadding(FMargin(8.f, 8.f, 8.f, 10.f));
	Size->SetContent(Frame);

	UOverlay* Layer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Layer"));
	Frame->SetContent(Layer);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Hit"));
	FButtonStyle Style = Button->GetStyle();
	Style.Normal.TintColor = FSlateColor(FLinearColor::Transparent);
	Style.Hovered.TintColor = FSlateColor(RuinmatesUI::SlotHover());
	Style.Pressed.TintColor = FSlateColor(RuinmatesUI::AccentFill());
	Button->SetStyle(Style);
	Button->OnClicked.AddDynamic(this, &URuinmatesTerminalItemSlot::HandleClicked);
	Layer->AddChildToOverlay(Button);

	IconText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Icon"));
	IconText->SetFont(RuinmatesUI::Font(28));
	IconText->SetJustification(ETextJustify::Center);
	IconText->SetText(NSLOCTEXT("RuinMatesUI", "PlaceholderIcon", "□"));
	if (UOverlaySlot* IconSlot = Layer->AddChildToOverlay(IconText))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Center);
		IconSlot->SetVerticalAlignment(VAlign_Center);
		IconSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}

	CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Count"));
	CountText->SetFont(RuinmatesUI::Font(16));
	CountText->SetJustification(ETextJustify::Right);
	if (UOverlaySlot* CountSlot = Layer->AddChildToOverlay(CountText))
	{
		CountSlot->SetHorizontalAlignment(HAlign_Right);
		CountSlot->SetVerticalAlignment(VAlign_Top);
	}

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	NameText->SetFont(RuinmatesUI::Font(15));
	NameText->SetAutoWrapText(false);
	NameText->SetClipping(EWidgetClipping::ClipToBounds);
	if (UOverlaySlot* NameSlot = Layer->AddChildToOverlay(NameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetVerticalAlignment(VAlign_Bottom);
		NameSlot->SetPadding(FMargin(2.f, 0.f, 2.f, 4.f));
	}

	QualityLine = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QualityLine"));
	if (UOverlaySlot* LineSlot = Layer->AddChildToOverlay(QualityLine))
	{
		LineSlot->SetHorizontalAlignment(HAlign_Fill);
		LineSlot->SetVerticalAlignment(VAlign_Bottom);
	}
	USizeBox* LineSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LineSize"));
	LineSize->SetHeightOverride(2.f);
	QualityLine->SetContent(LineSize);

	ScanBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("Scan"));
	ScanBar->SetFillColorAndOpacity(RuinmatesUI::Accent());
	ScanBar->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* ScanSlot = Layer->AddChildToOverlay(ScanBar))
	{
		ScanSlot->SetHorizontalAlignment(HAlign_Fill);
		ScanSlot->SetVerticalAlignment(VAlign_Bottom);
		ScanSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}
}

void URuinmatesTerminalItemSlot::ApplyView(const FRuinmatesTerminalSlotView& InView)
{
	View = InView;
	RefreshVisual();
}

void URuinmatesTerminalItemSlot::RefreshVisual()
{
	if (!Frame || !NameText || !CountText || !IconText || !QualityLine || !ScanBar)
	{
		return;
	}

	const bool bKnown = View.State == ERuinmatesTerminalSlotState::Revealed || View.State == ERuinmatesTerminalSlotState::Pending;
	const FLinearColor Quality = RuinmatesUI::QualityColor(static_cast<uint8>(View.Quality));
	FLinearColor Fill = RuinmatesUI::SlotEmpty();
	FLinearColor Border = RuinmatesUI::Line();
	int32 BorderPx = 1;

	if (bKnown)
	{
		Fill = Quality;
		Fill.A = 0.14f;
		Border = Quality;
	}
	else if (View.State == ERuinmatesTerminalSlotState::Searching)
	{
		Border = RuinmatesUI::Accent();
		BorderPx = 2;
	}
	else if (View.State == ERuinmatesTerminalSlotState::Unknown)
	{
		Fill = RuinmatesUI::SlotEmpty();
		Fill.A = 0.9f;
	}
	else if (View.State == ERuinmatesTerminalSlotState::Taken || View.State == ERuinmatesTerminalSlotState::Empty)
	{
		Fill.A = 0.35f;
		Border = RuinmatesUI::Line();
		Border.A = 0.28f;
	}

	if (View.bSelected && bKnown)
	{
		Border = RuinmatesUI::Accent();
		BorderPx = 2;
		FLinearColor Selected = RuinmatesUI::AccentFill();
		Fill = FLinearColor(
			FMath::Lerp(Fill.R, Selected.R, 0.45f),
			FMath::Lerp(Fill.G, Selected.G, 0.45f),
			FMath::Lerp(Fill.B, Selected.B, 0.45f),
			FMath::Max(Fill.A, Selected.A));
	}

	Frame->SetBrushColor(Fill);
	Frame->SetPadding(FMargin(static_cast<float>(BorderPx) + 7.f));
	QualityLine->SetBrushColor(bKnown ? Quality : FLinearColor::Transparent);
	QualityLine->SetVisibility(bKnown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (View.State == ERuinmatesTerminalSlotState::Unknown)
	{
		IconText->SetText(NSLOCTEXT("RuinMatesUI", "UnknownMark", "?"));
		IconText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextMuted()));
		NameText->SetText(FText::GetEmpty());
		CountText->SetText(FText::GetEmpty());
	}
	else if (View.State == ERuinmatesTerminalSlotState::Searching)
	{
		IconText->SetText(NSLOCTEXT("RuinMatesUI", "ScanningMark", "扫描"));
		IconText->SetFont(RuinmatesUI::Font(16));
		IconText->SetColorAndOpacity(FSlateColor(RuinmatesUI::Accent()));
		NameText->SetText(FText::GetEmpty());
		CountText->SetText(FText::GetEmpty());
	}
	else if (!bKnown)
	{
		IconText->SetText(FText::GetEmpty());
		NameText->SetText(FText::GetEmpty());
		CountText->SetText(FText::GetEmpty());
	}
	else
	{
		IconText->SetText(NSLOCTEXT("RuinMatesUI", "PlaceholderIcon", "□"));
		IconText->SetFont(RuinmatesUI::Font(28));
		IconText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextPrimary()));
		NameText->SetText(View.Name);
		NameText->SetColorAndOpacity(FSlateColor(Quality));
		CountText->SetText(View.Count > 1 ? FText::Format(NSLOCTEXT("RuinMatesUI", "CountFmt", "×{0}"), FText::AsNumber(View.Count)) : FText::GetEmpty());
		CountText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextPrimary()));
	}

	const bool bScanning = View.State == ERuinmatesTerminalSlotState::Searching;
	ScanBar->SetVisibility(bScanning ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ScanBar->SetPercent(bScanning ? ScanTime : 0.f);
}

void URuinmatesTerminalItemSlot::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (View.State != ERuinmatesTerminalSlotState::Searching || !ScanBar)
	{
		return;
	}
	ScanTime = FMath::Fmod(ScanTime + InDeltaTime / 1.2f, 1.f);
	ScanBar->SetPercent(ScanTime);
}

void URuinmatesTerminalItemSlot::HandleClicked()
{
	if (View.State == ERuinmatesTerminalSlotState::Revealed)
	{
		OnSlotClicked.Broadcast(View.StableIndex);
	}
}
