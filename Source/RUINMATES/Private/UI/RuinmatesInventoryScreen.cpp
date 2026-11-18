#include "UI/RuinmatesInventoryScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Game/RuinmatesPlayerState.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "UI/RuinmatesItemSlotWidget.h"

namespace
{
	const FLinearColor PanelColor(0.05f, 0.06f, 0.05f, 0.82f);
	const FLinearColor ScreenColor(0.02f, 0.025f, 0.02f, 0.58f);
	const FLinearColor Ink(0.82f, 0.86f, 0.78f);
	const FLinearColor Muted(0.55f, 0.62f, 0.52f);

	UTextBlock* MakeText(UWidgetTree* Tree, const FName& Name, const FText& Label, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		if (!Text)
		{
			return nullptr;
		}
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetText(Label);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetAutoWrapText(true);
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FName& Name)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		if (Border)
		{
			Border->SetBrushColor(PanelColor);
			Border->SetPadding(FMargin(14.f));
		}
		return Border;
	}
}

void URuinmatesInventoryScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildLayout();
}

void URuinmatesInventoryScreen::NativeDestruct()
{
	UnbindSources();
	Super::NativeDestruct();
}

void URuinmatesInventoryScreen::BuildLayout()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UBorder* Root = MakePanel(WidgetTree, TEXT("Screen"));
	Root->SetBrushColor(ScreenColor);
	Root->SetPadding(FMargin(28.f));
	WidgetTree->RootWidget = Root;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Root->SetContent(Column);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
	NameText = MakeText(WidgetTree, TEXT("PlayerName"), NSLOCTEXT("RuinMates", "PlayerFallback", "合伙人"), 22, Ink);
	MoneyText = MakeText(WidgetTree, TEXT("TeamMoney"), NSLOCTEXT("RuinMates", "TeamMoney", "团队资金 ¥0"), 16, RuinmatesItems::GetQualityColor(ERuinmatesItemQuality::Precious));
	TimeText = MakeText(WidgetTree, TEXT("WorldTime"), FText::GetEmpty(), 16, Muted);
	Header->AddChildToHorizontalBox(NameText);
	if (UHorizontalBoxSlot* MoneySlot = Header->AddChildToHorizontalBox(MoneyText))
	{
		MoneySlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
	}
	if (UHorizontalBoxSlot* TimeSlot = Header->AddChildToHorizontalBox(TimeText))
	{
		TimeSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
	}
	Column->AddChildToVerticalBox(Header);

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Body"));
	if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		BodySlot->SetPadding(FMargin(0.f, 16.f));
	}

	UBorder* Left = MakePanel(WidgetTree, TEXT("Equipment"));
	UBorder* Center = MakePanel(WidgetTree, TEXT("Backpack"));
	UBorder* Right = MakePanel(WidgetTree, TEXT("Right"));

	auto Weight = [](UHorizontalBoxSlot* BoxSlot, float Value)
	{
		if (!BoxSlot)
		{
			return;
		}
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = Value;
		BoxSlot->SetSize(Size);
		BoxSlot->SetPadding(FMargin(6.f));
	};
	Weight(Body->AddChildToHorizontalBox(Left), 0.28f);
	Weight(Body->AddChildToHorizontalBox(Center), 0.40f);
	Weight(Body->AddChildToHorizontalBox(Right), 0.32f);

	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LeftColumn"));
	Left->SetContent(LeftColumn);
	UBorder* Silhouette = MakePanel(WidgetTree, TEXT("Silhouette"));
	Silhouette->SetBrushColor(FLinearColor(0.08f, 0.10f, 0.08f, 0.9f));
	UTextBlock* SilhouetteLabel = MakeText(WidgetTree, TEXT("SilhouetteLabel"), NSLOCTEXT("RuinMates", "Silhouette", "角色剪影"), 18, Muted);
	Silhouette->SetContent(SilhouetteLabel);
	if (UVerticalBoxSlot* SilhouetteSlot = LeftColumn->AddChildToVerticalBox(Silhouette))
	{
		SilhouetteSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		SilhouetteSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	LeftColumn->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Head"), NSLOCTEXT("RuinMates", "HeadSlot", "头部  空"), 14, Ink));
	LeftColumn->AddChildToVerticalBox(MakeText(WidgetTree, TEXT("Armor"), NSLOCTEXT("RuinMates", "ArmorSlot", "护甲  空"), 14, Ink));
	PackNameText = MakeText(WidgetTree, TEXT("Pack"), NSLOCTEXT("RuinMates", "PackSlot", "背包"), 14, Ink);
	LeftColumn->AddChildToVerticalBox(PackNameText);

	UVerticalBox* CenterColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CenterColumn"));
	Center->SetContent(CenterColumn);
	CapacityText = MakeText(WidgetTree, TEXT("Capacity"), FText::GetEmpty(), 16, Ink);
	CenterColumn->AddChildToVerticalBox(CapacityText);
	CapacityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CapacityBar"));
	CapacityBar->SetFillColorAndOpacity(Muted);
	if (UVerticalBoxSlot* BarSlot = CenterColumn->AddChildToVerticalBox(CapacityBar))
	{
		BarSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 12.f));
	}
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetMinDesiredSlotWidth(108.f);
	BackpackGrid->SetMinDesiredSlotHeight(72.f);
	BackpackGrid->SetSlotPadding(FMargin(4.f));
	if (UVerticalBoxSlot* GridSlot = CenterColumn->AddChildToVerticalBox(BackpackGrid))
	{
		GridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ValueText = MakeText(WidgetTree, TEXT("Value"), FText::GetEmpty(), 16, RuinmatesItems::GetQualityColor(ERuinmatesItemQuality::Precious));
	if (UVerticalBoxSlot* ValueSlot = CenterColumn->AddChildToVerticalBox(ValueText))
	{
		ValueSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}

	RightSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("RightSwitcher"));
	Right->SetContent(RightSwitcher);

	UVerticalBox* Detail = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Detail"));
	DetailName = MakeText(WidgetTree, TEXT("DetailName"), NSLOCTEXT("RuinMates", "PickItem", "选择一件物品"), 20, Ink);
	DetailBody = MakeText(WidgetTree, TEXT("DetailBody"), NSLOCTEXT("RuinMates", "PickItemBody", "稀有和珍贵的东西会用更亮的颜色标出来。"), 14, Muted);
	Detail->AddChildToVerticalBox(DetailName);
	if (UVerticalBoxSlot* BodyTextSlot = Detail->AddChildToVerticalBox(DetailBody))
	{
		BodyTextSlot->SetPadding(FMargin(0.f, 10.f));
	}
	UButton* DiscardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Discard"));
	UTextBlock* DiscardLabel = MakeText(WidgetTree, TEXT("DiscardLabel"), NSLOCTEXT("RuinMates", "Discard", "丢弃"), 14, Ink);
	DiscardButton->AddChild(DiscardLabel);
	DiscardButton->OnClicked.AddDynamic(this, &URuinmatesInventoryScreen::HandleDiscard);
	Detail->AddChildToVerticalBox(DiscardButton);
	RightSwitcher->AddChild(Detail);

	UVerticalBox* Loot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Loot"));
	SearchTitle = MakeText(WidgetTree, TEXT("SearchTitle"), FText::GetEmpty(), 20, Ink);
	SearchProgress = MakeText(WidgetTree, TEXT("SearchProgress"), FText::GetEmpty(), 14, Muted);
	Loot->AddChildToVerticalBox(SearchTitle);
	Loot->AddChildToVerticalBox(SearchProgress);
	LootGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("LootGrid"));
	LootGrid->SetMinDesiredSlotWidth(108.f);
	LootGrid->SetMinDesiredSlotHeight(72.f);
	LootGrid->SetSlotPadding(FMargin(4.f));
	if (UVerticalBoxSlot* LootGridSlot = Loot->AddChildToVerticalBox(LootGrid))
	{
		LootGridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LootGridSlot->SetPadding(FMargin(0.f, 10.f));
	}
	UButton* TakeAll = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TakeAll"));
	UTextBlock* TakeAllLabel = MakeText(WidgetTree, TEXT("TakeAllLabel"), NSLOCTEXT("RuinMates", "TakeAll", "全部拾取"), 14, Ink);
	TakeAll->AddChild(TakeAllLabel);
	TakeAll->OnClicked.AddDynamic(this, &URuinmatesInventoryScreen::HandleTakeAll);
	Loot->AddChildToVerticalBox(TakeAll);
	RightSwitcher->AddChild(Loot);

	FooterText = MakeText(WidgetTree, TEXT("Footer"), NSLOCTEXT("RuinMates", "InventoryFooter", "Tab 关闭    背包满了就先丢掉不值钱的"), 13, Muted);
	Column->AddChildToVerticalBox(FooterText);
}

void URuinmatesInventoryScreen::OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer)
{
	UnbindSources();
	Character = InCharacter;
	Container = InContainer;
	SelectedBackpackSlot = INDEX_NONE;
	BindSources();
	Refresh();
}

void URuinmatesInventoryScreen::BindSources()
{
	if (Character && Character->GetBackpack())
	{
		Character->GetBackpack()->OnChanged.AddUObject(this, &URuinmatesInventoryScreen::Refresh);
	}
	if (Container)
	{
		Container->OnChanged.AddUObject(this, &URuinmatesInventoryScreen::Refresh);
	}
}

void URuinmatesInventoryScreen::UnbindSources()
{
	if (Character && Character->GetBackpack())
	{
		Character->GetBackpack()->OnChanged.RemoveAll(this);
	}
	if (Container)
	{
		Container->OnChanged.RemoveAll(this);
	}
}

void URuinmatesInventoryScreen::Refresh()
{
	if (!Character || !BackpackGrid || !RightSwitcher)
	{
		return;
	}

	if (APlayerState* PlayerState = Character->GetPlayerState())
	{
		if (NameText)
		{
			NameText->SetText(FText::FromString(PlayerState->GetPlayerName()));
		}
		if (const ARuinmatesPlayerState* RuinState = Cast<ARuinmatesPlayerState>(PlayerState))
		{
			if (MoneyText)
			{
				MoneyText->SetText(FText::Format(NSLOCTEXT("RuinMates", "TeamMoneyValue", "团队资金 ¥{0}"), FText::AsNumber(RuinState->GetTeamFunds())));
			}
		}
	}
	if (TimeText && GetWorld())
	{
		const int32 Seconds = FMath::FloorToInt(GetWorld()->GetTimeSeconds());
		TimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
	}

	const URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
	const int32 Capacity = Backpack ? Backpack->GetCapacity() : 0;
	const int32 Used = Backpack ? Backpack->GetUsedSlotCount() : 0;
	if (PackNameText && Backpack)
	{
		PackNameText->SetText(FText::Format(NSLOCTEXT("RuinMates", "PackLine", "背包  {0}"), RuinmatesItems::GetBackpackTierName(Backpack->GetTier())));
	}
	if (CapacityText)
	{
		CapacityText->SetText(FText::Format(NSLOCTEXT("RuinMates", "CapacityLine", "{0} / {1} 格"), FText::AsNumber(Used), FText::AsNumber(Capacity)));
	}
	if (CapacityBar)
	{
		CapacityBar->SetPercent(Capacity > 0 ? static_cast<float>(Used) / static_cast<float>(Capacity) : 0.f);
	}
	if (ValueText && Backpack)
	{
		ValueText->SetText(FText::Format(NSLOCTEXT("RuinMates", "LootValue", "战利品总价值 ¥{0}"), FText::AsNumber(Backpack->GetLootValue())));
	}

	BackpackGrid->ClearChildren();
	const TArray<FRuinmatesItemStack>& Slots = Backpack ? Backpack->GetSlots() : TArray<FRuinmatesItemStack>();
	for (int32 Index = 0; Index < Capacity; ++Index)
	{
		URuinmatesItemSlotWidget* SlotWidget = CreateWidget<URuinmatesItemSlotWidget>(this, URuinmatesItemSlotWidget::StaticClass());
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->SetIndex(Index);
		SlotWidget->OnSlotClicked.AddDynamic(this, &URuinmatesInventoryScreen::HandleBackpackSlotClicked);
		const FRuinmatesItemStack Stack = Slots.IsValidIndex(Index) ? Slots[Index] : FRuinmatesItemStack();
		SlotWidget->ShowStack(Stack, false);
		if (UUniformGridSlot* GridSlot = BackpackGrid->AddChildToUniformGrid(SlotWidget, Index / 5, Index % 5))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	const bool bSearching = IsValid(Container);
	RightSwitcher->SetActiveWidgetIndex(bSearching ? 1 : 0);
	if (!bSearching)
	{
		if (SelectedBackpackSlot != INDEX_NONE && Slots.IsValidIndex(SelectedBackpackSlot) && !Slots[SelectedBackpackSlot].IsEmpty())
		{
			const FRuinmatesItemStack& Stack = Slots[SelectedBackpackSlot];
			if (DetailName)
			{
				DetailName->SetText(Stack.DisplayName);
				DetailName->SetColorAndOpacity(FSlateColor(RuinmatesItems::GetQualityColor(Stack.Quality)));
			}
			if (DetailBody)
			{
				DetailBody->SetText(FText::Format(
					NSLOCTEXT("RuinMates", "DetailBodyFmt", "{0}\n¥{1}    {2}{3}"),
					Stack.Description,
					FText::AsNumber(Stack.GetTotalValue()),
					RuinmatesItems::GetQualityName(Stack.Quality),
					Stack.bUpgradeMaterial ? NSLOCTEXT("RuinMates", "UpgradeTag", "    升级材料") : FText::GetEmpty()));
			}
		}
		return;
	}

	if (SearchTitle)
	{
		SearchTitle->SetText(Container->GetContainerName());
	}
	const int32 Total = Container->GetContents().Num();
	const int32 Revealed = Container->GetRevealedCount();
	if (SearchProgress)
	{
		SearchProgress->SetText(FText::Format(NSLOCTEXT("RuinMates", "SearchProgressFmt", "{0} / {1} 已搜索"), FText::AsNumber(Revealed), FText::AsNumber(Total)));
	}
	LootGrid->ClearChildren();
	const TArray<FRuinmatesItemStack>& Loot = Container->GetContents();
	for (int32 Index = 0; Index < Loot.Num(); ++Index)
	{
		URuinmatesItemSlotWidget* SlotWidget = CreateWidget<URuinmatesItemSlotWidget>(this, URuinmatesItemSlotWidget::StaticClass());
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidget->SetIndex(Index);
		SlotWidget->OnSlotClicked.AddDynamic(this, &URuinmatesInventoryScreen::HandleLootSlotClicked);
		SlotWidget->ShowStack(Loot[Index], !Loot[Index].bRevealed);
		LootGrid->AddChildToUniformGrid(SlotWidget, Index / 3, Index % 3);
	}
}

void URuinmatesInventoryScreen::HandleTakeAll()
{
	if (Character)
	{
		Character->RequestTakeAllLoot();
	}
}

void URuinmatesInventoryScreen::HandleDiscard()
{
	if (Character && SelectedBackpackSlot != INDEX_NONE)
	{
		Character->RequestDiscard(SelectedBackpackSlot);
		SelectedBackpackSlot = INDEX_NONE;
	}
}

void URuinmatesInventoryScreen::HandleBackpackSlotClicked(int32 SlotIndex)
{
	SelectedBackpackSlot = SlotIndex;
	Refresh();
}

void URuinmatesInventoryScreen::HandleLootSlotClicked(int32 SlotIndex)
{
	if (Character)
	{
		Character->RequestPickup(SlotIndex);
	}
}
