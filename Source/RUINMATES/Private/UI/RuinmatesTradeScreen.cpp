#include "UI/RuinmatesTradeScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/RuinmatesGameState.h"
#include "InputCoreTypes.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Inventory/RuinmatesMerchant.h"
#include "UI/RuinmatesUIStyle.h"

namespace
{

	UBorder* MakeSolid(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		return Border;
	}

	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(RuinmatesUI::Font(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FName& Name, const FLinearColor& Normal, UTextBlock** OutLabel)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style;
		Style.Normal = FSlateColorBrush(Normal);
		Style.Hovered = FSlateColorBrush(RuinmatesUI::SlotHover());
		Style.Pressed = FSlateColorBrush(RuinmatesUI::AccentFill());
		Style.Disabled = FSlateColorBrush(RuinmatesUI::WithAlpha(Normal, 0.4f));
		Style.NormalPadding = FMargin(12.f, 8.f);
		Style.PressedPadding = FMargin(12.f, 8.f);
		Button->SetStyle(Style);

		UTextBlock* Label = MakeLabel(Tree, FName(*(Name.ToString() + TEXT("Label"))), 17, RuinmatesUI::TextPrimary());
		Label->SetJustification(ETextJustify::Center);
		Button->AddChild(Label);
		if (OutLabel)
		{
			*OutLabel = Label;
		}
		return Button;
	}
}

void URuinmatesTradeScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void URuinmatesTradeScreen::NativeDestruct()
{
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
		{
			Backpack->OnChanged.RemoveAll(this);
		}
	}
	if (ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr)
	{
		GameState->OnSharedFundsChanged.RemoveAll(this);
	}
	Super::NativeDestruct();
}

FReply URuinmatesTradeScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseTrade();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URuinmatesTradeScreen::BuildLayout()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TradeRoot"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Dimmer = MakeSolid(WidgetTree, TEXT("TradeDimmer"), RuinmatesUI::Token(TEXT("040B14"), 0.55f));
	Dimmer->SetPadding(FMargin(0.f));
	if (UCanvasPanelSlot* DimmerSlot = Canvas->AddChildToCanvas(Dimmer))
	{
		DimmerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimmerSlot->SetOffsets(FMargin(0.f));
	}

	UBorder* Shell = MakeSolid(WidgetTree, TEXT("TradeShell"), RuinmatesUI::Token(TEXT("0A1626"), 0.95f));
	Shell->SetPadding(FMargin(24.f, 20.f));
	if (UCanvasPanelSlot* ShellSlot = Canvas->AddChildToCanvas(Shell))
	{
		ShellSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		ShellSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ShellSlot->SetSize(FVector2D(1100.f, 700.f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TradeColumn"));
	Shell->SetContent(Column);

	// Header: merchant, funds, close.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TradeHeader"));
	MerchantNameText = MakeLabel(WidgetTree, TEXT("MerchantName"), 26, RuinmatesUI::TextPrimary());
	MerchantNameText->SetText(NSLOCTEXT("RuinMatesUI", "TradeMerchant", "商人"));
	if (UHorizontalBoxSlot* NameSlot = Header->AddChildToHorizontalBox(MerchantNameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}
	FundsText = MakeLabel(WidgetTree, TEXT("Funds"), 20, RuinmatesUI::Accent());
	FundsText->SetText(FText::FromString(TEXT("团队资金 0")));
	if (UHorizontalBoxSlot* FundsSlot = Header->AddChildToHorizontalBox(FundsText))
	{
		FundsSlot->SetVerticalAlignment(VAlign_Center);
	}
	UTextBlock* CloseLabel = nullptr;
	UButton* CloseButton = MakeButton(WidgetTree, TEXT("TradeClose"), RuinmatesUI::WithAlpha(RuinmatesUI::Danger(), 0.35f), &CloseLabel);
	CloseLabel->SetText(NSLOCTEXT("RuinMatesUI", "Close", "×"));
	CloseButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleClose);
	if (UHorizontalBoxSlot* CloseSlot = Header->AddChildToHorizontalBox(CloseButton))
	{
		CloseSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
	}

	// Mode switch.
	UHorizontalBox* Modes = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TradeModes"));
	UTextBlock* BuyLabel = nullptr;
	BuyModeButton = MakeButton(WidgetTree, TEXT("ModeBuy"), RuinmatesUI::AccentFill(), &BuyLabel);
	BuyLabel->SetText(NSLOCTEXT("RuinMatesUI", "ModeBuy", "购买物资"));
	BuyModeButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleBuyMode);
	if (UHorizontalBoxSlot* BuySlot = Modes->AddChildToHorizontalBox(BuyModeButton))
	{
		BuySlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}
	UTextBlock* SellLabel = nullptr;
	SellModeButton = MakeButton(WidgetTree, TEXT("ModeSell"), RuinmatesUI::AccentFill(), &SellLabel);
	SellLabel->SetText(NSLOCTEXT("RuinMatesUI", "ModeSell", "出售物资"));
	SellModeButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleSellMode);
	Modes->AddChildToHorizontalBox(SellModeButton);
	ModeText = MakeLabel(WidgetTree, TEXT("ModeHint"), 15, RuinmatesUI::TextMuted());
	ModeText->SetText(NSLOCTEXT("RuinMatesUI", "ModeHint", "选择左侧条目，然后在右侧确认数量"));
	if (UHorizontalBoxSlot* HintSlot = Modes->AddChildToHorizontalBox(ModeText))
	{
		HintSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		HintSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (UVerticalBoxSlot* ModesSlot = Column->AddChildToVerticalBox(Modes))
	{
		ModesSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	}

	// Body: list on the left, detail on the right.
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TradeBody"));
	if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UBorder* ListPanel = MakeSolid(WidgetTree, TEXT("TradeListPanel"), RuinmatesUI::Token(TEXT("102A43"), 0.62f));
	ListPanel->SetPadding(FMargin(14.f, 12.f));
	if (UHorizontalBoxSlot* ListSlot = Body->AddChildToHorizontalBox(ListPanel))
	{
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = 1.f;
		ListSlot->SetSize(Size);
		ListSlot->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TradeScroll"));
	EntryList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TradeEntries"));
	Scroll->AddChild(EntryList);
	ListPanel->SetContent(Scroll);

	UBorder* DetailPanel = MakeSolid(WidgetTree, TEXT("TradeDetailPanel"), RuinmatesUI::Token(TEXT("153651"), 0.90f));
	DetailPanel->SetPadding(FMargin(18.f, 16.f));
	if (UHorizontalBoxSlot* DetailSlot = Body->AddChildToHorizontalBox(DetailPanel))
	{
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = 1.f;
		DetailSlot->SetSize(Size);
	}
	DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TradeDetail"));
	DetailPanel->SetContent(DetailBox);

	DetailNameText = MakeLabel(WidgetTree, TEXT("DetailName"), 24, RuinmatesUI::TextPrimary());
	DetailNameText->SetText(NSLOCTEXT("RuinMatesUI", "DetailEmpty", "未选择"));
	DetailBox->AddChildToVerticalBox(DetailNameText);

	DetailInfoText = MakeLabel(WidgetTree, TEXT("DetailInfo"), 17, RuinmatesUI::TextSecondary());
	DetailBox->AddChildToVerticalBox(DetailInfoText);

	OwnedText = MakeLabel(WidgetTree, TEXT("DetailOwned"), 17, RuinmatesUI::TextSecondary());
	if (UVerticalBoxSlot* OwnedSlot = DetailBox->AddChildToVerticalBox(OwnedText))
	{
		OwnedSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}

	UHorizontalBox* QuantityRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TradeQuantity"));
	UTextBlock* MinusLabel = nullptr;
	UButton* MinusButton = MakeButton(WidgetTree, TEXT("QtyMinus"), RuinmatesUI::Token(TEXT("102A43"), 0.8f), &MinusLabel);
	MinusLabel->SetText(FText::FromString(TEXT("-")));
	MinusButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleQuantityDown);
	QuantityRow->AddChildToHorizontalBox(MinusButton);

	QuantityText = MakeLabel(WidgetTree, TEXT("QtyValue"), 22, RuinmatesUI::TextPrimary());
	QuantityText->SetText(FText::AsNumber(1));
	QuantityText->SetJustification(ETextJustify::Center);
	USizeBox* QuantitySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("QtySize"));
	QuantitySize->SetWidthOverride(64.f);
	QuantitySize->SetContent(QuantityText);
	QuantityRow->AddChildToHorizontalBox(QuantitySize);

	UTextBlock* PlusLabel = nullptr;
	UButton* PlusButton = MakeButton(WidgetTree, TEXT("QtyPlus"), RuinmatesUI::Token(TEXT("102A43"), 0.8f), &PlusLabel);
	PlusLabel->SetText(FText::FromString(TEXT("+")));
	PlusButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleQuantityUp);
	QuantityRow->AddChildToHorizontalBox(PlusButton);

	UTextBlock* AllLabel = nullptr;
	UButton* AllButton = MakeButton(WidgetTree, TEXT("QtyAll"), RuinmatesUI::Token(TEXT("102A43"), 0.6f), &AllLabel);
	AllLabel->SetText(NSLOCTEXT("RuinMatesUI", "All", "全部"));
	AllButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleSelectAll);
	if (UHorizontalBoxSlot* AllSlot = QuantityRow->AddChildToHorizontalBox(AllButton))
	{
		AllSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* QuantitySlot = DetailBox->AddChildToVerticalBox(QuantityRow))
	{
		QuantitySlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}

	UTextBlock* ConfirmCaption = nullptr;
	ConfirmButton = MakeButton(WidgetTree, TEXT("TradeConfirm"), RuinmatesUI::AccentFill(), &ConfirmCaption);
	ConfirmLabel = ConfirmCaption;
	ConfirmLabel->SetText(NSLOCTEXT("RuinMatesUI", "ConfirmBuy", "购买"));
	ConfirmButton->OnClicked.AddDynamic(this, &URuinmatesTradeScreen::HandleConfirm);
	if (UVerticalBoxSlot* ConfirmSlot = DetailBox->AddChildToVerticalBox(ConfirmButton))
	{
		ConfirmSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	}

	StatusText = MakeLabel(WidgetTree, TEXT("TradeStatus"), 16, RuinmatesUI::TextMuted());
	StatusText->SetText(FText::GetEmpty());
	if (UVerticalBoxSlot* StatusSlot = Column->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}
}

void URuinmatesTradeScreen::SetContext(ARuinmatesCharacter* InCharacter, ARuinmatesMerchant* InMerchant)
{
	LiveCharacter = InCharacter;
	LiveMerchant = InMerchant;
	bSellMode = false;
	SelectedItemId = NAME_None;
	SelectedSlotIndex = INDEX_NONE;
	Quantity = 1;

	if (MerchantNameText)
	{
		MerchantNameText->SetText(IsValid(InMerchant)
			? InMerchant->GetMerchantName()
			: NSLOCTEXT("RuinMatesUI", "TradeMerchant", "商人"));
	}

	if (IsValid(InCharacter))
	{
		if (URuinmatesBackpackComponent* Backpack = InCharacter->GetBackpack())
		{
			Backpack->OnChanged.AddUObject(this, &URuinmatesTradeScreen::RefreshDetail);
		}
	}
	if (ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr)
	{
		GameState->OnSharedFundsChanged.AddUObject(this, &URuinmatesTradeScreen::RefreshFunds);
	}

	RefreshFunds();
	RebuildList();
	RefreshDetail();
}

void URuinmatesTradeScreen::ShowResult(bool bSuccess, const FText& Message)
{
	if (StatusText)
	{
		StatusText->SetText(Message);
		StatusText->SetColorAndOpacity(FSlateColor(bSuccess ? RuinmatesUI::Success() : RuinmatesUI::Danger()));
	}
	RebuildList();
	RefreshDetail();
	RefreshFunds();
}

void URuinmatesTradeScreen::RefreshFunds()
{
	ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr;
	const int32 Funds = GameState ? GameState->GetSharedFunds() : 0;
	if (FundsText)
	{
		FundsText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "TradeFunds", "团队资金 {0}"), FText::AsNumber(Funds)));
	}
}

void URuinmatesTradeScreen::RebuildList()
{
	if (!EntryList)
	{
		return;
	}
	EntryList->ClearChildren();
	RowProxies.Reset();

	ARuinmatesCharacter* Character = LiveCharacter.Get();
	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);

	auto AddRow = [this](const FName& Tag, const FText& Label, const FLinearColor& Color)
	{
		UTextBlock* RowLabel = nullptr;
		UButton* RowButton = MakeButton(WidgetTree, FName(*FString::Printf(TEXT("Row_%s"), *Tag.ToString())),
			RuinmatesUI::Token(TEXT("102A43"), 0.45f), &RowLabel);
		RowLabel->SetText(Label);
		RowLabel->SetColorAndOpacity(FSlateColor(Color));

		URuinmatesTradeRowProxy* Proxy = NewObject<URuinmatesTradeRowProxy>(this);
		Proxy->Tag = Tag;
		Proxy->OnRowSelected.AddUObject(this, &URuinmatesTradeScreen::HandleRowSelected);
		RowButton->OnClicked.AddDynamic(Proxy, &URuinmatesTradeRowProxy::HandleClicked);
		RowProxies.Add(Proxy);

		if (UVerticalBoxSlot* RowSlot = EntryList->AddChildToVerticalBox(RowButton))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	};

	if (!bSellMode)
	{
		ARuinmatesMerchant* Merchant = LiveMerchant.Get();
		if (!IsValid(Merchant) || !Database)
		{
			return;
		}
		for (const FName& ItemId : Merchant->GetMerchantItems())
		{
			const FRuinmatesItemRow* Row = Database->FindItemDefinition(ItemId);
			if (!Row)
			{
				continue;
			}
			AddRow(ItemId,
				FText::Format(NSLOCTEXT("RuinMatesUI", "BuyRow", "{0}    {1}"),
					Row->DisplayName, FText::AsNumber(Row->BuyPrice)),
				RuinmatesUI::QualityColor(static_cast<uint8>(Row->Quality)));
		}
		return;
	}

	if (!IsValid(Character) || !Character->GetBackpack())
	{
		return;
	}
	const TArray<FRuinmatesItemStack>& Slots = Character->GetBackpack()->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FRuinmatesItemStack& Stack = Slots[Index];
		if (Stack.IsEmpty())
		{
			continue;
		}
		const int32 SellPrice = Database ? Database->GetItemSellPrice(Stack.ItemId) : 0;
		AddRow(FName(*FString::Printf(TEXT("slot:%d"), Index)),
			FText::Format(NSLOCTEXT("RuinMatesUI", "SellRow", "{0}  ×{1}    {2}"),
				Stack.DisplayName, FText::AsNumber(Stack.Count), FText::AsNumber(SellPrice)),
			RuinmatesUI::QualityColor(static_cast<uint8>(Stack.Quality)));
	}
}

void URuinmatesTradeScreen::RefreshDetail()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	URuinmatesBackpackComponent* Backpack = Character ? Character->GetBackpack() : nullptr;

	FText Name = NSLOCTEXT("RuinMatesUI", "DetailEmpty", "未选择");
	FText Info;
	FText Owned;
	int32 MaxQuantity = 1;

	if (!bSellMode)
	{
		if (const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(SelectedItemId) : nullptr)
		{
			Name = Row->DisplayName;
			Info = FText::Format(
				NSLOCTEXT("RuinMatesUI", "DetailBuyInfo", "{0} · 单价 {1}"),
				RuinmatesItems::GetQualityName(Row->Quality), FText::AsNumber(Row->BuyPrice));

			int32 Held = 0;
			if (Backpack)
			{
				for (const FRuinmatesItemStack& Stack : Backpack->GetSlots())
				{
					if (Stack.ItemId == SelectedItemId)
					{
						Held += Stack.Count;
					}
				}
			}
			Owned = FText::Format(NSLOCTEXT("RuinMatesUI", "Owned", "当前持有 {0}"), FText::AsNumber(Held));
			MaxQuantity = 99;
		}
	}
	else if (Backpack && Backpack->GetSlots().IsValidIndex(SelectedSlotIndex))
	{
		const FRuinmatesItemStack& Stack = Backpack->GetSlots()[SelectedSlotIndex];
		if (!Stack.IsEmpty())
		{
			const int32 SellPrice = Database ? Database->GetItemSellPrice(Stack.ItemId) : 0;
			Name = Stack.DisplayName;
			Info = FText::Format(
				NSLOCTEXT("RuinMatesUI", "DetailSellInfo", "{0} · 单价 {1}"),
				RuinmatesItems::GetQualityName(Stack.Quality), FText::AsNumber(SellPrice));
			Owned = FText::Format(NSLOCTEXT("RuinMatesUI", "OwnedCount", "持有 ×{0}"), FText::AsNumber(Stack.Count));
			MaxQuantity = Stack.Count;
		}
	}

	Quantity = FMath::Clamp(Quantity, 1, FMath::Max(1, MaxQuantity));
	if (DetailNameText)
	{
		DetailNameText->SetText(Name);
	}
	if (DetailInfoText)
	{
		DetailInfoText->SetText(Info);
	}
	if (OwnedText)
	{
		OwnedText->SetText(Owned);
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(Quantity));
	}
	if (ConfirmLabel)
	{
		ConfirmLabel->SetText(bSellMode
			? NSLOCTEXT("RuinMatesUI", "ConfirmSell", "卖出")
			: NSLOCTEXT("RuinMatesUI", "ConfirmBuy", "购买"));
	}
}

void URuinmatesTradeScreen::HandleRowSelected(FName Tag)
{
	if (!bSellMode)
	{
		SelectedItemId = Tag;
		SelectedSlotIndex = INDEX_NONE;
	}
	else
	{
		const FString TagString = Tag.ToString();
		SelectedSlotIndex = TagString.StartsWith(TEXT("slot:")) ? FCString::Atoi(*TagString.RightChop(5)) : INDEX_NONE;
		SelectedItemId = NAME_None;
	}
	Quantity = 1;
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleBuyMode()
{
	bSellMode = false;
	SelectedItemId = NAME_None;
	SelectedSlotIndex = INDEX_NONE;
	Quantity = 1;
	RebuildList();
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleSellMode()
{
	bSellMode = true;
	SelectedItemId = NAME_None;
	SelectedSlotIndex = INDEX_NONE;
	Quantity = 1;
	RebuildList();
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleQuantityDown()
{
	Quantity = FMath::Max(1, Quantity - 1);
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleQuantityUp()
{
	Quantity = FMath::Min(99, Quantity + 1);
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleSelectAll()
{
	if (bSellMode)
	{
		if (ARuinmatesCharacter* Character = LiveCharacter.Get())
		{
			if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
			{
				const TArray<FRuinmatesItemStack>& Slots = Backpack->GetSlots();
				if (Slots.IsValidIndex(SelectedSlotIndex))
				{
					Quantity = FMath::Max(1, Slots[SelectedSlotIndex].Count);
				}
			}
		}
	}
	else
	{
		Quantity = 99;
	}
	RefreshDetail();
}

void URuinmatesTradeScreen::HandleConfirm()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	if (!IsValid(Character))
	{
		return;
	}

	if (!bSellMode)
	{
		if (SelectedItemId.IsNone())
		{
			ShowResult(false, NSLOCTEXT("RuinMatesUI", "PickItem", "请先选择一件商品"));
			return;
		}
		Character->RequestBuyItem(SelectedItemId, Quantity);
	}
	else
	{
		if (SelectedSlotIndex == INDEX_NONE)
		{
			ShowResult(false, NSLOCTEXT("RuinMatesUI", "PickStack", "请先选择要出售的背包格子"));
			return;
		}
		Character->RequestSellItem(SelectedSlotIndex, Quantity);
	}
}

void URuinmatesTradeScreen::HandleClose()
{
	CloseTrade();
}

void URuinmatesTradeScreen::CloseTrade()
{
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->EndTrade();
	}
	else
	{
		RemoveFromParent();
	}
}
