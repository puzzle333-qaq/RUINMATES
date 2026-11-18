#include "UI/RuinmatesTerminalScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesLootContainer.h"
#include "UI/RuinmatesTerminalItemSlot.h"
#include "UI/RuinmatesUIStyle.h"

namespace
{
	UTextBlock* MakeLabel(UWidgetTree* Tree, const FName& Name, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetFont(RuinmatesUI::Font(Size));
		Text->SetColorAndOpacity(FSlateColor(Color));
		return Text;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FName& Name, const FLinearColor& Color)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(FMargin(20.f));
		return Border;
	}

	void FillSlot(UHorizontalBoxSlot* Slot, float Weight)
	{
		if (!Slot)
		{
			return;
		}
		FSlateChildSize Size;
		Size.SizeRule = ESlateSizeRule::Fill;
		Size.Value = Weight;
		Slot->SetSize(Size);
	}
}

void URuinmatesTerminalScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildChrome();
}

void URuinmatesTerminalScreen::NativeDestruct()
{
	UnbindSources();
	Super::NativeDestruct();
}

FReply URuinmatesTerminalScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Tab)
	{
		ClosePreview();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void URuinmatesTerminalScreen::BuildChrome()
{
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("WBP_TerminalRoot"));
	WidgetTree->RootWidget = Canvas;

	UBorder* Dimmer = MakePanel(WidgetTree, TEXT("WorldDimmer"), RuinmatesUI::Overlay());
	Dimmer->SetPadding(FMargin(0.f));
	if (UCanvasPanelSlot* DimmerSlot = Canvas->AddChildToCanvas(Dimmer))
	{
		DimmerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DimmerSlot->SetOffsets(FMargin(0.f));
	}

	UBorder* Shell = MakePanel(WidgetTree, TEXT("ShellBackground"), RuinmatesUI::Shell());
	Shell->SetPadding(FMargin(32.f));
	if (UCanvasPanelSlot* ShellSlot = Canvas->AddChildToCanvas(Shell))
	{
		ShellSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		ShellSlot->SetOffsets(FMargin(64.f, 48.f, 64.f, 48.f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SafeContent"));
	Shell->SetContent(Column);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_TerminalHeader"));
	UTextBlock* Brand = MakeLabel(WidgetTree, TEXT("Brand"), 16, RuinmatesUI::TextSecondary());
	Brand->SetText(NSLOCTEXT("RuinMatesUI", "Brand", "RUINMATES  /  个人终端"));
	PageTitle = MakeLabel(WidgetTree, TEXT("PageTitle"), 32, RuinmatesUI::TextPrimary());
	ClockText = MakeLabel(WidgetTree, TEXT("Clock"), 16, RuinmatesUI::TextSecondary());
	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Close"));
	UTextBlock* CloseLabel = MakeLabel(WidgetTree, TEXT("CloseLabel"), 20, RuinmatesUI::TextPrimary());
	CloseLabel->SetText(NSLOCTEXT("RuinMatesUI", "Close", "×"));
	CloseButton->AddChild(CloseLabel);
	CloseButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::ClosePreview);
	Header->AddChildToHorizontalBox(Brand);
	if (UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(PageTitle))
	{
		FillSlot(TitleSlot, 1.f);
		TitleSlot->SetPadding(FMargin(24.f, 0.f, 0.f, 0.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
	}
	Header->AddChildToHorizontalBox(ClockText);
	if (UHorizontalBoxSlot* CloseSlot = Header->AddChildToHorizontalBox(CloseButton))
	{
		CloseSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}
	if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
	}

	UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_TerminalTabs"));
	Column->AddChildToVerticalBox(Tabs);

	UButton* AllTab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabAll"));
	UButton* ConsumableTab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabConsumable"));
	UButton* MaterialTab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabMaterial"));
	UButton* QuestTab = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TabQuest"));
	UTextBlock* AllLabel = MakeLabel(WidgetTree, TEXT("AllLabel"), 20, RuinmatesUI::Accent());
	UTextBlock* ConsumableLabel = MakeLabel(WidgetTree, TEXT("ConsumableLabel"), 20, RuinmatesUI::TextSecondary());
	UTextBlock* MaterialLabel = MakeLabel(WidgetTree, TEXT("MaterialLabel"), 20, RuinmatesUI::TextSecondary());
	UTextBlock* QuestLabel = MakeLabel(WidgetTree, TEXT("QuestLabel"), 20, RuinmatesUI::TextSecondary());
	AllLabel->SetText(NSLOCTEXT("RuinMatesUI", "TabAll", "全部物资"));
	ConsumableLabel->SetText(NSLOCTEXT("RuinMatesUI", "TabConsumable", "消耗品"));
	MaterialLabel->SetText(NSLOCTEXT("RuinMatesUI", "TabMaterial", "材料"));
	QuestLabel->SetText(NSLOCTEXT("RuinMatesUI", "TabQuest", "任务物品"));
	AllTab->AddChild(AllLabel);
	ConsumableTab->AddChild(ConsumableLabel);
	MaterialTab->AddChild(MaterialLabel);
	QuestTab->AddChild(QuestLabel);
	AllTab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectAll);
	ConsumableTab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectConsumable);
	MaterialTab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectMaterial);
	QuestTab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectQuest);
	for (UButton* Tab : { AllTab, ConsumableTab, MaterialTab, QuestTab })
	{
		if (UHorizontalBoxSlot* TabSlot = Tabs->AddChildToHorizontalBox(Tab))
		{
			TabSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 12.f));
		}
	}

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_InventoryScreen"));
	if (UVerticalBoxSlot* BodySlot = Column->AddChildToVerticalBox(Body))
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UBorder* Left = MakePanel(WidgetTree, TEXT("WBP_BackpackPanel"), RuinmatesUI::Panel());
	UBorder* Right = MakePanel(WidgetTree, TEXT("RightPanel"), RuinmatesUI::Panel());
	FillSlot(Body->AddChildToHorizontalBox(Left), 1024.f);
	if (UHorizontalBoxSlot* Gap = Body->AddChildToHorizontalBox(WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Gap"))))
	{
		Gap->SetPadding(FMargin(12.f, 0.f));
	}
	FillSlot(Body->AddChildToHorizontalBox(Right), 680.f);

	UVerticalBox* LeftColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BackpackColumn"));
	Left->SetContent(LeftColumn);
	UHorizontalBox* PackHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_PanelHeader"));
	PackTitle = MakeLabel(WidgetTree, TEXT("PackTitle"), 24, RuinmatesUI::TextPrimary());
	CapacityText = MakeLabel(WidgetTree, TEXT("CapacityText"), 18, RuinmatesUI::TextSecondary());
	FillSlot(PackHeader->AddChildToHorizontalBox(PackTitle), 1.f);
	PackHeader->AddChildToHorizontalBox(CapacityText);
	LeftColumn->AddChildToVerticalBox(PackHeader);
	CapacityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("WBP_CapacityBar"));
	CapacityBar->SetFillColorAndOpacity(RuinmatesUI::Accent());
	if (UVerticalBoxSlot* BarSlot = LeftColumn->AddChildToVerticalBox(CapacityBar))
	{
		BarSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 16.f));
	}
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("BackpackScroll"));
	BackpackGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("BackpackGrid"));
	BackpackGrid->SetSlotPadding(FMargin(6.f));
	Scroll->AddChild(BackpackGrid);
	if (UVerticalBoxSlot* ScrollSlot = LeftColumn->AddChildToVerticalBox(Scroll))
	{
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	ValueText = MakeLabel(WidgetTree, TEXT("WBP_BackpackSummary"), 18, RuinmatesUI::TextPrimary());
	if (UVerticalBoxSlot* ValueSlot = LeftColumn->AddChildToVerticalBox(ValueText))
	{
		ValueSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	}

	RightSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("RightSwitcher"));
	Right->SetContent(RightSwitcher);

	UVerticalBox* Equipment = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WBP_EquipmentPanel"));
	RightTitle = MakeLabel(WidgetTree, TEXT("CharacterName"), 24, RuinmatesUI::TextPrimary());
	RightSubtitle = MakeLabel(WidgetTree, TEXT("CharacterLevel"), 16, RuinmatesUI::TextSecondary());
	Equipment->AddChildToVerticalBox(RightTitle);
	Equipment->AddChildToVerticalBox(RightSubtitle);
	UBorder* Preview = MakePanel(WidgetTree, TEXT("WBP_CharacterPreview"), RuinmatesUI::PanelElevated());
	UTextBlock* PreviewLabel = MakeLabel(WidgetTree, TEXT("PreviewLabel"), 18, RuinmatesUI::TextMuted());
	PreviewLabel->SetText(NSLOCTEXT("RuinMatesUI", "Preview", "角色预览"));
	PreviewLabel->SetJustification(ETextJustify::Center);
	Preview->SetContent(PreviewLabel);
	USizeBox* PreviewSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PreviewSize"));
	PreviewSize->SetWidthOverride(300.f);
	PreviewSize->SetHeightOverride(420.f);
	PreviewSize->SetContent(Preview);
	if (UVerticalBoxSlot* PreviewSlot = Equipment->AddChildToVerticalBox(PreviewSize))
	{
		PreviewSlot->SetPadding(FMargin(0.f, 16.f));
		PreviewSlot->SetHorizontalAlignment(HAlign_Center);
	}
	UTextBlock* EquipNote = MakeLabel(WidgetTree, TEXT("EquipNote"), 15, RuinmatesUI::TextMuted());
	EquipNote->SetText(NSLOCTEXT("RuinMatesUI", "EquipNote", "头部 · 护甲 · 背包 · 主武器 · 副武器 · 近战"));
	Equipment->AddChildToVerticalBox(EquipNote);
	RightSwitcher->AddChild(Equipment);

	UVerticalBox* Loot = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("WBP_LootContainerPanel"));
	UHorizontalBox* LootHeader = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("LootHeader"));
	UTextBlock* LootTitle = MakeLabel(WidgetTree, TEXT("LootTitle"), 24, RuinmatesUI::TextPrimary());
	UTextBlock* LootCount = MakeLabel(WidgetTree, TEXT("LootCount"), 18, RuinmatesUI::TextSecondary());
	LootTitleText = LootTitle;
	LootCountText = LootCount;
	FillSlot(LootHeader->AddChildToHorizontalBox(LootTitle), 1.f);
	LootHeader->AddChildToHorizontalBox(LootCount);
	Loot->AddChildToVerticalBox(LootHeader);
	SearchBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("WBP_SearchProgress"));
	SearchBar->SetFillColorAndOpacity(RuinmatesUI::Accent());
	if (UVerticalBoxSlot* SearchSlot = Loot->AddChildToVerticalBox(SearchBar))
	{
		SearchSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 16.f));
	}
	LootGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("LootGrid"));
	LootGrid->SetSlotPadding(FMargin(6.f));
	if (UVerticalBoxSlot* LootGridSlot = Loot->AddChildToVerticalBox(LootGrid))
	{
		LootGridSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UTextBlock* TakeLabel = MakeLabel(WidgetTree, TEXT("TakeLabel"), 18, RuinmatesUI::TextPrimary());
	TakeLabel->SetText(NSLOCTEXT("RuinMatesUI", "TakeFound", "拾取已发现物品"));
	TakeAllButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WBP_TerminalButton"));
	FButtonStyle TakeStyle = TakeAllButton->GetStyle();
	TakeStyle.Normal.TintColor = FSlateColor(RuinmatesUI::AccentFill());
	TakeStyle.Hovered.TintColor = FSlateColor(RuinmatesUI::SlotHover());
	TakeStyle.Pressed.TintColor = FSlateColor(RuinmatesUI::Accent());
	TakeAllButton->SetStyle(TakeStyle);
	TakeAllButton->AddChild(TakeLabel);
	TakeAllButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleTakeAll);
	if (UVerticalBoxSlot* TakeSlot = Loot->AddChildToVerticalBox(TakeAllButton))
	{
		TakeSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
		TakeSlot->SetHorizontalAlignment(HAlign_Left);
	}
	RightSwitcher->AddChild(Loot);

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("WBP_TerminalFooter"));
	FooterLeft = MakeLabel(WidgetTree, TEXT("FooterLeft"), 15, RuinmatesUI::TextMuted());
	FooterRight = MakeLabel(WidgetTree, TEXT("FooterRight"), 18, RuinmatesUI::Accent());
	FillSlot(Footer->AddChildToHorizontalBox(FooterLeft), 1.f);
	Footer->AddChildToHorizontalBox(FooterRight);
	if (UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer))
	{
		FooterSlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}
}

void URuinmatesTerminalScreen::OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer)
{
	bLive = true;
	bClosing = false;
	LiveCharacter = InCharacter;
	LiveContainer = InContainer;
	ActiveFilter = NAME_None;
	BindSources();
	RefreshFromLive();
}

void URuinmatesTerminalScreen::ShowPreview(bool bSearchPage)
{
	UnbindSources();
	bLive = false;
	bClosing = false;
	LiveCharacter = nullptr;
	LiveContainer = nullptr;
	Page = bSearchPage ? RuinmatesTerminalPreview::MakeSearchPage() : RuinmatesTerminalPreview::MakeInventoryPage();
	ActiveFilter = NAME_None;
	SelectedStableIndex = INDEX_NONE;
	ApplyPageText();
	RebuildSlots();
}

void URuinmatesTerminalScreen::ApplyPageText()
{
	if (PageTitle)
	{
		PageTitle->SetText(Page.PageTitle);
	}
	if (ClockText)
	{
		ClockText->SetText(Page.StatusText);
	}
	if (PackTitle)
	{
		PackTitle->SetText(Page.PackTitle);
	}
	if (CapacityText)
	{
		CapacityText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "Capacity", "{0} / {1}"), FText::AsNumber(Page.Used), FText::AsNumber(Page.Capacity)));
	}
	if (CapacityBar)
	{
		const float Ratio = Page.Capacity > 0 ? static_cast<float>(Page.Used) / static_cast<float>(Page.Capacity) : 0.f;
		CapacityBar->SetPercent(Ratio);
		CapacityBar->SetFillColorAndOpacity(Ratio >= 1.f ? RuinmatesUI::Warning() : RuinmatesUI::Accent());
	}
	if (ValueText)
	{
		ValueText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "Value", "物资总价值    {0}"), FText::AsNumber(Page.LootValue)));
	}
	if (RightSwitcher)
	{
		RightSwitcher->SetActiveWidgetIndex(Page.bRightIsContainer ? 1 : 0);
	}
	if (!Page.bRightIsContainer)
	{
		if (RightTitle)
		{
			RightTitle->SetText(Page.RightTitle);
		}
		if (RightSubtitle)
		{
			RightSubtitle->SetText(Page.RightSubtitle);
		}
	}
	else
	{
		if (LootTitleText)
		{
			LootTitleText->SetText(Page.RightTitle);
		}
		if (LootCountText)
		{
			LootCountText->SetText(Page.RightSubtitle);
		}
		if (SearchBar)
		{
			SearchBar->SetPercent(Page.RightProgress);
		}
	}
	if (FooterLeft)
	{
		FooterLeft->SetText(Page.FooterLeft);
	}
	if (FooterRight)
	{
		FooterRight->SetText(Page.FooterRight);
	}
}

void URuinmatesTerminalScreen::RebuildSlots()
{
	if (!BackpackGrid)
	{
		return;
	}
	BackpackGrid->ClearChildren();
	BackpackSlots.Reset();
	for (const FRuinmatesTerminalSlotView& Source : Page.Backpack)
	{
		FRuinmatesTerminalSlotView Shown = Source;
		if (!ActiveFilter.IsNone() && Shown.State == ERuinmatesTerminalSlotState::Revealed && !Shown.Category.IsNone() && Shown.Category != ActiveFilter)
		{
			Shown = FRuinmatesTerminalSlotView();
			Shown.StableIndex = Source.StableIndex;
			Shown.State = ERuinmatesTerminalSlotState::Empty;
		}
		URuinmatesTerminalItemSlot* Cell = CreateWidget<URuinmatesTerminalItemSlot>(this, URuinmatesTerminalItemSlot::StaticClass());
		if (!Cell)
		{
			continue;
		}
		BackpackGrid->AddChildToUniformGrid(Cell, BackpackSlots.Num() / 5, BackpackSlots.Num() % 5);
		Cell->ApplyView(Shown);
		Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSlotClicked);
		BackpackSlots.Add(Cell);
	}

	if (!Page.bRightIsContainer || !LootGrid)
	{
		return;
	}
	LootGrid->ClearChildren();
	LootSlots.Reset();
	for (const FRuinmatesTerminalSlotView& Source : Page.RightSlots)
	{
		URuinmatesTerminalItemSlot* Cell = CreateWidget<URuinmatesTerminalItemSlot>(this, URuinmatesTerminalItemSlot::StaticClass());
		if (!Cell)
		{
			continue;
		}
		LootGrid->AddChildToUniformGrid(Cell, LootSlots.Num() / 3, LootSlots.Num() % 3);
		Cell->ApplyView(Source);
		Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleLootClicked);
		LootSlots.Add(Cell);
	}
}

void URuinmatesTerminalScreen::SetFilter(FName InFilter)
{
	ActiveFilter = InFilter;
	RebuildSlots();
}

void URuinmatesTerminalScreen::SelectAll() { SetFilter(NAME_None); }
void URuinmatesTerminalScreen::SelectConsumable() { SetFilter(TEXT("Consumable")); }
void URuinmatesTerminalScreen::SelectMaterial() { SetFilter(TEXT("Material")); }
void URuinmatesTerminalScreen::SelectQuest() { SetFilter(TEXT("Quest")); }

void URuinmatesTerminalScreen::HandleSlotClicked(int32 StableIndex)
{
	SelectedStableIndex = StableIndex;
	for (FRuinmatesTerminalSlotView& View : Page.Backpack)
	{
		View.bSelected = View.StableIndex == StableIndex && View.State == ERuinmatesTerminalSlotState::Revealed;
	}
	RebuildSlots();
}

void URuinmatesTerminalScreen::HandleLootClicked(int32 StableIndex)
{
	if (!bLive)
	{
		return;
	}
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->RequestPickup(StableIndex);
	}
}

void URuinmatesTerminalScreen::HandleTakeAll()
{
	if (!bLive)
	{
		return;
	}
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		Character->RequestTakeAllLoot();
	}
}

void URuinmatesTerminalScreen::BindSources()
{
	UnbindSources();
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
		{
			Backpack->OnChanged.AddUObject(this, &URuinmatesTerminalScreen::Refresh);
		}
	}
	if (ARuinmatesLootContainer* Container = LiveContainer.Get())
	{
		Container->OnChanged.AddUObject(this, &URuinmatesTerminalScreen::Refresh);
	}
}

void URuinmatesTerminalScreen::UnbindSources()
{
	if (ARuinmatesCharacter* Character = LiveCharacter.Get())
	{
		if (URuinmatesBackpackComponent* Backpack = Character->GetBackpack())
		{
			Backpack->OnChanged.RemoveAll(this);
		}
	}
	if (ARuinmatesLootContainer* Container = LiveContainer.Get())
	{
		Container->OnChanged.RemoveAll(this);
	}
}

void URuinmatesTerminalScreen::Refresh()
{
	if (bLive)
	{
		RefreshFromLive();
	}
}

void URuinmatesTerminalScreen::RefreshFromLive()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	if (!Character)
	{
		return;
	}

	URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
	ARuinmatesLootContainer* Container = LiveContainer.Get();

	FRuinmatesTerminalPreviewPage Next;
	Next.PageTitle = Container
		? NSLOCTEXT("RuinMatesUI", "PageSearch", "物资回收")
		: NSLOCTEXT("RuinMatesUI", "PageInventory", "背包");
	Next.PackTitle = Backpack
		? RuinmatesItems::GetBackpackTierName(Backpack->GetTier())
		: NSLOCTEXT("RuinMatesUI", "PackMissing", "背包");
	Next.Capacity = Backpack ? Backpack->GetCapacity() : 0;
	Next.Used = Backpack ? Backpack->GetUsedSlotCount() : 0;
	Next.LootValue = Backpack ? Backpack->GetLootValue() : 0;

	const TArray<FRuinmatesItemStack> NoSlots;
	const TArray<FRuinmatesItemStack>& Slots = Backpack ? Backpack->GetSlots() : NoSlots;
	const int32 SlotCount = FMath::Max(Next.Capacity, Slots.Num());
	Next.Backpack.Reserve(SlotCount);
	bool bSelectionStillValid = false;
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FRuinmatesTerminalSlotView View;
		View.StableIndex = Index;
		if (Slots.IsValidIndex(Index) && !Slots[Index].IsEmpty())
		{
			const FRuinmatesItemStack& Stack = Slots[Index];
			View.State = ERuinmatesTerminalSlotState::Revealed;
			View.Name = Stack.DisplayName;
			View.Count = Stack.Count;
			View.Quality = Stack.Quality;
			View.Category = Stack.bUpgradeMaterial ? FName(TEXT("Material")) : NAME_None;
			View.bSelected = Index == SelectedStableIndex;
			bSelectionStillValid |= View.bSelected;
		}
		else
		{
			View.State = ERuinmatesTerminalSlotState::Empty;
		}
		Next.Backpack.Add(View);
	}
	if (!bSelectionStillValid)
	{
		SelectedStableIndex = INDEX_NONE;
	}

	if (Container)
	{
		Next.bRightIsContainer = true;
		Next.RightTitle = Container->GetContainerName();
		const TArray<FRuinmatesItemStack>& Contents = Container->GetContents();
		int32 Occupied = 0;
		int32 Revealed = 0;
		int32 SearchingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Contents.Num(); ++Index)
		{
			if (Contents[Index].IsEmpty())
			{
				continue;
			}
			++Occupied;
			if (Contents[Index].bRevealed)
			{
				++Revealed;
			}
			else if (SearchingIndex == INDEX_NONE)
			{
				SearchingIndex = Index;
			}
		}
		Next.RightSubtitle = FText::Format(
			NSLOCTEXT("RuinMatesUI", "LiveSearchCount", "{0} / {1}"),
			FText::AsNumber(Revealed),
			FText::AsNumber(Occupied));
		Next.RightProgress = Occupied > 0 ? static_cast<float>(Revealed) / static_cast<float>(Occupied) : 1.f;
		Next.RightSlots.Reserve(Contents.Num());
		for (int32 Index = 0; Index < Contents.Num(); ++Index)
		{
			FRuinmatesTerminalSlotView View;
			View.StableIndex = Index;
			const FRuinmatesItemStack& Stack = Contents[Index];
			if (Stack.IsEmpty())
			{
				View.State = ERuinmatesTerminalSlotState::Empty;
			}
			else if (!Stack.bRevealed)
			{
				View.State = Index == SearchingIndex
					? ERuinmatesTerminalSlotState::Searching
					: ERuinmatesTerminalSlotState::Unknown;
			}
			else
			{
				View.State = ERuinmatesTerminalSlotState::Revealed;
				View.Name = Stack.DisplayName;
				View.Count = Stack.Count;
				View.Quality = Stack.Quality;
				View.Category = Stack.bUpgradeMaterial ? FName(TEXT("Material")) : NAME_None;
			}
			Next.RightSlots.Add(View);
		}
		Next.FooterLeft = NSLOCTEXT("RuinMatesUI", "LiveSearchFooter", "ESC 关闭    点击已发现物品拾取");
		Next.FooterRight = NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停");
	}
	else
	{
		FText SurvivorName = NSLOCTEXT("RuinMatesUI", "Survivor", "幸存者");
		if (APlayerState* PlayerState = Character->GetPlayerState())
		{
			if (!PlayerState->GetPlayerName().IsEmpty())
			{
				SurvivorName = FText::FromString(PlayerState->GetPlayerName());
			}
		}
		Next.RightTitle = SurvivorName;
		if (ARuinmatesPlayerState* RuinState = Character->GetPlayerState<ARuinmatesPlayerState>())
		{
			Next.RightSubtitle = FText::Format(
				NSLOCTEXT("RuinMatesUI", "LiveLevel", "LV.{0}"),
				FText::AsNumber(RuinState->GetCampaignLevel()));
		}
		Next.FooterLeft = NSLOCTEXT("RuinMatesUI", "LiveInvFooter", "TAB 关闭");
	}

	Page = Next;
	ApplyPageText();
	RebuildSlots();
}

void URuinmatesTerminalScreen::ClosePreview()
{
	if (bClosing)
	{
		return;
	}
	bClosing = true;
	if (bLive)
	{
		if (ARuinmatesCharacter* Character = LiveCharacter.Get())
		{
			Character->CloseTerminal();
			return;
		}
	}
	RemoveFromParent();
	if (!GEngine)
	{
		return;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (!World || (World->WorldType != EWorldType::PIE && World->WorldType != EWorldType::Game))
		{
			continue;
		}
		if (APlayerController* Player = World->GetFirstPlayerController())
		{
			Player->SetShowMouseCursor(false);
			Player->SetInputMode(FInputModeGameOnly());
		}
	}
}

static void OpenTerminalPreview(bool bSearch)
{
	if (!GEngine)
	{
		return;
	}
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game))
		{
			World = Context.World();
			break;
		}
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("Ruinmates terminal preview needs a running PIE or game world."));
		return;
	}
	APlayerController* Player = World->GetFirstPlayerController();
	if (!Player)
	{
		return;
	}
	URuinmatesTerminalScreen* Screen = CreateWidget<URuinmatesTerminalScreen>(Player, URuinmatesTerminalScreen::StaticClass());
	if (!Screen)
	{
		return;
	}
	Screen->AddToViewport(40);
	Screen->ShowPreview(bSearch);
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetWidgetToFocus(Screen->TakeWidget());
	Player->SetInputMode(InputMode);
	Player->SetShowMouseCursor(true);
}

static FAutoConsoleCommand GPreviewInventory(
	TEXT("Ruinmates.PreviewInventory"),
	TEXT("Open the isolated terminal backpack preview. Does not use live inventory."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenTerminalPreview(false); }));

static FAutoConsoleCommand GPreviewSearch(
	TEXT("Ruinmates.PreviewSearch"),
	TEXT("Open the isolated terminal search preview. Does not use a live container."),
	FConsoleCommandDelegate::CreateStatic([]() { OpenTerminalPreview(true); }));
