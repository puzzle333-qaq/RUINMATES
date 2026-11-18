#include "UI/RuinmatesItemSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void URuinmatesItemSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	QualityBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Quality"));
	QualityBorder->SetPadding(FMargin(6.f));
	QualityBorder->SetBrushColor(FLinearColor(0.12f, 0.13f, 0.12f, 0.9f));
	WidgetTree->RootWidget = QualityBorder;

	ClickButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Click"));
	QualityBorder->SetContent(ClickButton);
	ClickButton->OnClicked.AddDynamic(this, &URuinmatesItemSlotWidget::HandleClicked);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	ClickButton->AddChild(Column);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Name"));
	CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Count"));
	NameText->SetAutoWrapText(true);
	NameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.84f, 0.78f)));
	CountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.74f, 0.66f)));
	if (UVerticalBoxSlot* NameSlot = Column->AddChildToVerticalBox(NameText))
	{
		NameSlot->SetPadding(FMargin(4.f));
	}
	if (UVerticalBoxSlot* CountSlot = Column->AddChildToVerticalBox(CountText))
	{
		CountSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 4.f));
	}
}

void URuinmatesItemSlotWidget::ShowStack(const FRuinmatesItemStack& Stack, bool bUnknown)
{
	if (!NameText || !CountText || !QualityBorder)
	{
		return;
	}
	if (bUnknown || Stack.IsEmpty())
	{
		NameText->SetText(bUnknown ? NSLOCTEXT("RuinMates", "UnknownItem", "未搜索") : FText::GetEmpty());
		CountText->SetText(FText::GetEmpty());
		QualityBorder->SetBrushColor(FLinearColor(0.10f, 0.11f, 0.10f, bUnknown ? 0.85f : 0.35f));
		return;
	}

	NameText->SetText(Stack.DisplayName);
	CountText->SetText(FText::AsNumber(Stack.Count));
	const FLinearColor Color = RuinmatesItems::GetQualityColor(Stack.Quality);
	const float Glow = Stack.Quality == ERuinmatesItemQuality::Precious ? 1.f : 0.55f;
	QualityBorder->SetBrushColor(FLinearColor(Color.R * Glow, Color.G * Glow, Color.B * Glow, 0.92f));
	NameText->SetColorAndOpacity(FSlateColor(Color));
}

void URuinmatesItemSlotWidget::HandleClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}
