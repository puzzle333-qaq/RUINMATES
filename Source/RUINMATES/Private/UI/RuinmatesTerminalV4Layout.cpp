#include "UI/RuinmatesTerminalScreen.h"

#include "Blueprint/WidgetTree.h"
#include "Character/RuinmatesCharacter.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesMerchant.h"
#include "Game/RuinmatesGameState.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WidgetSwitcher.h"
#include "UI/RuinmatesCharacterPreview.h"
#include "UI/RuinmatesTechButton.h"
#include "UI/RuinmatesGridScrollBox.h"
#include "UI/RuinmatesTerminalItemSlot.h"
#include "UI/RuinmatesUIStyle.h"
#include "UI/RuinmatesSquadRadio.h"
#include "UI/RuinmatesTechControls.h"

namespace V4Layout
{
	void At(UCanvasPanel* Canvas, UWidget* Widget, float X, float Y, float W, float H)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetPosition(FVector2D(X, Y));
			Slot->SetSize(FVector2D(W, H));
		}
	}

	UTextBlock* Text(UWidgetTree* Tree, const FName Name, const FText& Content, int32 Size = 18, bool bAccent = false)
	{
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Label->SetText(Content);
		Label->SetFont(RuinmatesUI::Font(Size));
		Label->SetColorAndOpacity(bAccent ? RuinmatesUI::Accent() : RuinmatesUI::TextPrimary());
		Label->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Label->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Label;
	}

	UImage* Art(UWidgetTree* Tree, const TCHAR* Path)
	{
		UImage* Image = Tree->ConstructWidget<UImage>();
		Image->SetBrush(RuinmatesUI::V4(Path));
		Image->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Image;
	}

	URuinmatesTechButton* Button(UWidgetTree* Tree, const FText& Label, bool bTab = false, bool bDanger = false)
	{
		URuinmatesTechButton* Hit = Tree->ConstructWidget<URuinmatesTechButton>();
		Hit->SetPresentation(bTab, bDanger);
		UTextBlock* Caption = Text(Tree, NAME_None, Label, bTab ? 16 : 18);
		Hit->AddChild(Caption);
		if (UButtonSlot* Slot = Cast<UButtonSlot>(Caption->Slot))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
			Slot->SetPadding(FMargin(8.f, 0.f));
		}
		return Hit;
	}

	UProgressBar* Bar(UWidgetTree* Tree, float Value = 0.f)
	{
		UProgressBar* Progress = Tree->ConstructWidget<UProgressBar>();
		Progress->SetWidgetStyle(RuinmatesUI::ProgressStyle());
		Progress->SetBarFillStyle(EProgressBarFillStyle::Mask);
		Progress->SetFillColorAndOpacity(RuinmatesUI::Accent());
		Progress->SetPercent(Value);
		Progress->SetBorderPadding(FVector2D::ZeroVector);
		Progress->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Progress;
	}

	UWidget* Equipment(UWidgetTree* Tree, const FText& Label, const TCHAR* Glyph, UTextBlock** Status = nullptr)
	{
		UCanvasPanel* Cell = Tree->ConstructWidget<UCanvasPanel>();
		At(Cell, Art(Tree, TEXT("Art/Panels/T_UI_Card_Base")), 0, 0, 144, 94);
		At(Cell, Art(Tree, Glyph), 52, 24, 40, 40);
		At(Cell, Text(Tree, NAME_None, Label, 14), 10, 7, 124, 20);
		UTextBlock* Value = Text(Tree, NAME_None, NSLOCTEXT("RuinMatesUI", "EmptyEquipment", "未装备"), 13);
		Value->SetColorAndOpacity(RuinmatesUI::TextSecondary());
		At(Cell, Value, 10, 69, 124, 19);
		if (Status) *Status = Value;
		return Cell;
	}

	UScrollBox* Grid(UWidgetTree* Tree, UCanvasPanel* Canvas, UUniformGridPanel*& OutGrid,
		float X, float Y, float W, float H)
	{
		UScrollBox* Scroll = Tree->ConstructWidget<URuinmatesGridScrollBox>();
		Scroll->SetWidgetBarStyle(RuinmatesUI::ScrollBarStyle());
		// The scrollbar is outside the grid's exact column budget.
		Scroll->SetScrollBarVisibility(ESlateVisibility::Visible);
		Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
		OutGrid = Tree->ConstructWidget<UUniformGridPanel>();
		OutGrid->SetSlotPadding(FMargin(6.f));
		Scroll->AddChild(OutGrid);
		At(Canvas, Scroll, X - 6, Y - 6, W + 12, H + 12);
		return Scroll;
	}

	void Scrollbar(UWidgetTree* Tree, UCanvasPanel* Canvas, UScrollBox* Scroll, float X, float Y, float H)
	{
		UScrollBar* BarWidget = Tree->ConstructWidget<UScrollBar>();
		BarWidget->SetWidgetStyle(RuinmatesUI::ScrollBarStyle());
		BarWidget->SetThickness(FVector2D(6.f, 6.f));
		At(Canvas, BarWidget, X, Y, 6, H);
		if (URuinmatesGridScrollBox* GridScroll = Cast<URuinmatesGridScrollBox>(Scroll))
		{
			GridScroll->SetExternalScrollbar(BarWidget);
		}
	}
}

void URuinmatesTerminalScreen::BuildV4Chrome()
{
	using namespace V4Layout;
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TerminalRootV4"));
	WidgetTree->RootWidget = Root;
	UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>();
	Dimmer->SetBrushColor(RuinmatesUI::Overlay());
	Dimmer->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(Dimmer))
	{
		DimmerSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		DimmerSlot->SetOffsets(FMargin(0.f));
	}
	DesignCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Design1920x1080"));
	if (UCanvasPanelSlot* DesignSlot = Root->AddChildToCanvas(DesignCanvas))
	{
		DesignSlot->SetAnchors(FAnchors(0.5f));
		DesignSlot->SetAlignment(FVector2D(0.5f));
		DesignSlot->SetSize(FVector2D(1920, 1080));
	}
	DesignCanvas->SetRenderTransformPivot(FVector2D(0.5f));
	UCanvasPanel* C = DesignCanvas;
	SquadRadio = CreateWidget<URuinmatesSquadRadio>(this);
	if (SquadRadio) At(C, SquadRadio, 24, 200, 208, 312);

	// Thin status strip and open world margins replace the old enclosing device bezel.
	At(C, Art(WidgetTree, TEXT("Art/Panels/T_UI_Panel_Small")), 32, 40, 1856, 52);
	At(C, Art(WidgetTree, TEXT("Art/Decorations/T_UI_Slashes")), 48, 55, 40, 14);
	At(C, Text(WidgetTree, TEXT("Brand"), FText::FromString(TEXT("RUINMATES")), 20), 104, 51, 210, 32);
	PreviewBadge = Text(WidgetTree, TEXT("PreviewBadge"), NSLOCTEXT("RuinMatesUI", "VisualPreviewBadge", "视觉预览 / 模拟数据"), 14, true);
	At(C, PreviewBadge, 344, 56, 400, 24);
	FundsText = Text(WidgetTree, TEXT("SharedFunds"), FText::GetEmpty(), 18);
	FundsText->SetJustification(ETextJustify::Right);
	FundsChip = FundsText;
	At(C, FundsText, 1470, 54, 320, 28);
	CloseButton = Button(WidgetTree, FText::GetEmpty());
	CloseButton->SetContent(Art(WidgetTree, TEXT("Icons/System/T_Icon_SYS_Close")));
	if (UButtonSlot* CloseSlot = Cast<UButtonSlot>(CloseButton->GetContent()->Slot))
	{
		CloseSlot->SetPadding(FMargin(12.f));
	}
	CloseButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::ClosePreview);
	CloseButton->SetToolTipText(NSLOCTEXT("RuinMatesUI", "CloseTip", "关闭终端（ESC）"));
	At(C, CloseButton, 1828, 44, 48, 44);
	PageTitle = Text(WidgetTree, TEXT("PageTitle"), FText::GetEmpty(), 32);
	At(C, Art(WidgetTree, TEXT("Icons/System/T_Icon_SYS_Inventory")), 280, 148, 36, 36);
	At(C, PageTitle, 328, 143, 700, 48);

	UHorizontalBox* Nav = WidgetTree->ConstructWidget<UHorizontalBox>();
	PreviewNavigation = Nav;
	At(C, Nav, 1100, 151, 764, 36);
	const FText Modes[] = {
		NSLOCTEXT("RuinMatesUI", "PreviewInvTab", "背包"),
		NSLOCTEXT("RuinMatesUI", "PreviewStorageTab", "仓库"),
		NSLOCTEXT("RuinMatesUI", "PreviewSearchTab", "搜索"),
		NSLOCTEXT("RuinMatesUI", "PreviewMarketTab", "商店"),
		NSLOCTEXT("RuinMatesUI", "PreviewControlsTab", "组件")
	};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		URuinmatesTechButton* Hit = Button(WidgetTree, Modes[Index], true);
		VisualTabs.Add(Hit);
		switch (Index)
		{
		case 0: Hit->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewInventory); break;
		case 1: Hit->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewStorage); break;
		case 2: Hit->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewSearch); break;
		case 3: Hit->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewMarket); break;
		default: Hit->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewComponents); break;
		}
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetWidthOverride(144.f);
		Size->SetContent(Hit);
		Nav->AddChildToHorizontalBox(Size)->SetPadding(FMargin(0, 0, 8, 0));
	}

	At(C, Art(WidgetTree, TEXT("Art/Panels/T_UI_Panel_Large")), 280, 224, 936, 696);
	PackTitle = Text(WidgetTree, TEXT("PackTitle"), FText::GetEmpty(), 24, true);
	At(C, PackTitle, 304, 248, 500, 32);
	CapacityUsedText = Text(WidgetTree, TEXT("CapacityUsed"), FText::GetEmpty(), 20);
	At(C, CapacityUsedText, 1000, 251, 64, 28);
	CapacityUsedText->SetJustification(ETextJustify::Right);
	CapacityText = Text(WidgetTree, TEXT("Capacity"), FText::GetEmpty(), 20);
	At(C, CapacityText, 1064, 251, 128, 28);
	CapacityBar = Bar(WidgetTree);
	At(C, CapacityBar, 304, 288, 888, 6);

	// Whole-backpack action, mirrored in the slot menu; the server owns the actual ordering.
	SortButton = Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "SortV4", "整理"));
	SortButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleSortBackpack);
	At(C, SortButton, 672, 858, 116, 32);

	TabButtons.Reset();
	TabFilters.Reset();
	TabLabels.Reset();
	TabIcons.Reset();
	TArray<FName> FilterIds;
	FilterIds.Add(NAME_None);
	if (const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this))
	{
		for (ERuinmatesItemCategory Category : Database->GetAvailableCategories())
		{
			const UEnum* CategoryEnum = StaticEnum<ERuinmatesItemCategory>();
			if (CategoryEnum)
			{
				FilterIds.Add(FName(*CategoryEnum->GetNameStringByValue(static_cast<int64>(Category))));
			}
		}
	}
	const UEnum* CategoryEnum = StaticEnum<ERuinmatesItemCategory>();
	for (int32 Index = 0; Index < FilterIds.Num(); ++Index)
	{
		const FName Filter = FilterIds[Index];
		const FText Label = Filter.IsNone()
			? NSLOCTEXT("RuinMatesUI", "OverviewV4", "總覽")
			: CategoryEnum ? CategoryEnum->GetDisplayNameTextByValue(CategoryEnum->GetValueByName(Filter)) : FText::FromName(Filter);
		URuinmatesTechButton* Tab = Button(WidgetTree, Label, true);
		Tab->SetSelected(Filter.IsNone());
		if (Filter.IsNone())
		{
			Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectAll);
		}
		else if (Filter == TEXT("Consumable")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectConsumable);
		else if (Filter == TEXT("Ammo")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectAmmo);
		else if (Filter == TEXT("Material")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectMaterial);
		else if (Filter == TEXT("Valuable")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectValuable);
		else if (Filter == TEXT("Quest")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectQuest);
		else if (Filter == TEXT("Weapon")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectWeapon);
		else if (Filter == TEXT("Equipment")) Tab->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::SelectEquipment);
		else Tab->SetIsEnabled(false);
		const float TabWidth = FMath::Min(144.f,
			(888.f - FMath::Max(0, FilterIds.Num() - 1) * 12.f) / FMath::Max(1, FilterIds.Num()));
		At(C, Tab, 304.f + Index * (TabWidth + 12.f), 306.f, TabWidth, 32.f);
		TabButtons.Add(Tab);
		TabFilters.Add(Filter);
	}
	UUniformGridPanel* PackGrid = nullptr;
	BackpackScroll = Grid(WidgetTree, C, PackGrid, 304, 344, 888, 484);
	BackpackGrid = PackGrid;
	CategoryEmptyHint = Text(WidgetTree, TEXT("CategoryEmptyHint"),
		NSLOCTEXT("RuinMatesUI", "NoItemsInCategory", "暂无该类物品"), 18);
	CategoryEmptyHint->SetColorAndOpacity(RuinmatesUI::WithAlpha(RuinmatesUI::TextWeak(), 0.68f));
	CategoryEmptyHint->SetVisibility(ESlateVisibility::Collapsed);
	At(C, CategoryEmptyHint, 304, 554, 888, 32);
	Scrollbar(WidgetTree, C, BackpackScroll, 1198, 344, 484);
	At(C, Text(WidgetTree, NAME_None, NSLOCTEXT("RuinMatesUI", "V4Value", "物资总价值"), 18), 304, 864, 360, 28);
	ValueText = Text(WidgetTree, TEXT("LootValue"), FText::GetEmpty(), 24);
	ValueText->SetJustification(ETextJustify::Right);
	At(C, ValueText, 800, 860, 392, 36);

	At(C, Art(WidgetTree, TEXT("Art/Panels/T_UI_Panel_Large")), 1240, 224, 624, 696);
	RightSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
	At(C, RightSwitcher, 1240, 224, 624, 696);
	UCanvasPanel* EquipmentPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	RightSwitcher->AddChild(EquipmentPage);
	RightTitle = Text(WidgetTree, TEXT("OperatorName"), FText::GetEmpty(), 24);
	RightSubtitle = Text(WidgetTree, TEXT("OperatorLevel"), FText::GetEmpty(), 18, true);
	RightSubtitle->SetJustification(ETextJustify::Right);
	At(EquipmentPage, RightTitle, 24, 24, 400, 32);
	At(EquipmentPage, RightSubtitle, 480, 24, 120, 32);
	UTextBlock* PackStatus = nullptr;
	UTextBlock* PrimaryStatus = nullptr;
	At(EquipmentPage, Equipment(WidgetTree, NSLOCTEXT("RuinMatesUI", "HeadV4", "头盔"), TEXT("Icons/System/T_Icon_SYS_Armor")), 24, 120, 144, 94);
	At(EquipmentPage, Equipment(WidgetTree, NSLOCTEXT("RuinMatesUI", "ArmorV4", "护甲"), TEXT("Icons/System/T_Icon_SYS_Armor")), 24, 236, 144, 94);
	At(EquipmentPage, Equipment(WidgetTree, NSLOCTEXT("RuinMatesUI", "PackV4", "背包"), TEXT("Icons/System/T_Icon_SYS_Backpack"), &PackStatus), 24, 352, 144, 94);
	At(EquipmentPage, Equipment(WidgetTree, NSLOCTEXT("RuinMatesUI", "PrimaryV4", "主武器"), TEXT("Icons/System/T_Icon_SYS_Ammo"), &PrimaryStatus), 456, 120, 144, 94);
	At(EquipmentPage, Equipment(WidgetTree, NSLOCTEXT("RuinMatesUI", "SecondaryV4", "副武器"), TEXT("Icons/System/T_Icon_SYS_Ammo")), 456, 236, 144, 94);
	EquipBackpackText = PackStatus;
	EquipPrimaryText = PrimaryStatus;
	CharacterPreview = CreateWidget<URuinmatesCharacterPreview>(this);
	if (CharacterPreview) At(EquipmentPage, CharacterPreview, 180, 98, 264, 520);

	UCanvasPanel* ContainerPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	RightSwitcher->AddChild(ContainerPage);
	LootTitleText = Text(WidgetTree, TEXT("ContainerName"), FText::GetEmpty(), 24, true);
	LootCountText = Text(WidgetTree, TEXT("SearchCount"), FText::GetEmpty(), 18);
	LootCountText->SetJustification(ETextJustify::Right);
	At(ContainerPage, LootTitleText, 24, 24, 400, 32);
	At(ContainerPage, LootCountText, 444, 24, 156, 32);
	SearchBar = Bar(WidgetTree);
	At(ContainerPage, SearchBar, 24, 64, 576, 6);
	SearchStatusText = Text(WidgetTree, TEXT("SearchStatus"), FText::GetEmpty(), 16, true);
	At(ContainerPage, SearchStatusText, 24, 82, 576, 32);
	UUniformGridPanel* ContainerGrid = nullptr;
	LootScroll = Grid(WidgetTree, ContainerPage, ContainerGrid, 48, 120, 528, 484);
	LootGrid = ContainerGrid;
	Scrollbar(WidgetTree, ContainerPage, LootScroll, 592, 120, 484);
	TakeAllLabel = Text(WidgetTree, TEXT("TakeAllLabel"), NSLOCTEXT("RuinMatesUI", "TakeFoundV4", "拾取已发现物品"), 18);
	URuinmatesTechButton* Take = Button(WidgetTree, FText::GetEmpty());
	Take->SetContent(TakeAllLabel);
	if (UButtonSlot* TakeSlot = Cast<UButtonSlot>(TakeAllLabel->Slot))
	{
		TakeSlot->SetHorizontalAlignment(HAlign_Center);
		TakeSlot->SetVerticalAlignment(VAlign_Center);
	}
	TakeAllButton = Take;
	Take->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleTakeAll);
	At(ContainerPage, Take, 344, 628, 256, 44);

	UCanvasPanel* MarketPage = WidgetTree->ConstructWidget<UCanvasPanel>();
	RightSwitcher->AddChild(MarketPage);
	At(MarketPage, Text(WidgetTree, NAME_None, NSLOCTEXT("RuinMatesUI", "MerchantPreview", "物资商人"), 24, true), 24, 24, 576, 32);
	URuinmatesTechButton* Buy = Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "BuyV4", "购买"), true);
	URuinmatesTechButton* Sell = Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "SellV4", "出售"), true);
	VisualBuy = Buy;
	VisualSell = Sell;
	Buy->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewBuy);
	Sell->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewSell);
	At(MarketPage, Buy, 24, 76, 280, 32);
	At(MarketPage, Sell, 320, 76, 280, 32);
	UUniformGridPanel* GoodsGrid = nullptr;
	MarketGoods = Grid(WidgetTree, MarketPage, GoodsGrid, 48, 120, 528, 360);
	MarketGrid = GoodsGrid;
	SellPrompt = Text(WidgetTree, TEXT("SellPrompt"), NSLOCTEXT("RuinMatesUI", "SellPromptV4", "选择背包中的物资"), 20);
	At(MarketPage, SellPrompt, 48, 240, 528, 40);
	At(MarketPage, Art(WidgetTree, TEXT("Art/Panels/T_UI_Card_Base")), 24, 500, 576, 176);
	MarketName = Text(WidgetTree, TEXT("TradeSelection"), FText::GetEmpty(), 20);
	MarketPrice = Text(WidgetTree, TEXT("TradeUnitPrice"), FText::GetEmpty(), 18);
	MarketPrice->SetJustification(ETextJustify::Right);
	At(MarketPage, MarketName, 48, 512, 320, 30);
	At(MarketPage, MarketPrice, 380, 512, 196, 30);
	URuinmatesTechButton* Minus = Button(WidgetTree, FText::FromString(TEXT("-")));
	URuinmatesTechButton* Plus = Button(WidgetTree, FText::FromString(TEXT("+")));
	Minus->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewQuantityDown);
	Plus->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewQuantityUp);
	At(MarketPage, Minus, 48, 550, 44, 40);
	At(MarketPage, Plus, 180, 550, 44, 40);
	MarketQuantity = Text(WidgetTree, TEXT("TradeQuantity"), FText::GetEmpty(), 20);
	MarketQuantity->SetJustification(ETextJustify::Center);
	MarketTotal = Text(WidgetTree, TEXT("TradeTotal"), FText::GetEmpty(), 20, true);
	MarketTotal->SetJustification(ETextJustify::Right);
	At(MarketPage, MarketQuantity, 100, 554, 72, 32);
	At(MarketPage, MarketTotal, 256, 554, 320, 32);
	MarketConfirmButton = Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "VisualOnlyTrade", "视觉预览"));
	MarketConfirmButton->SetIsEnabled(false);
	MarketConfirmButton->OnClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleMarketConfirm);
	At(MarketPage, MarketConfirmButton, 332, 600, 244, 52);
	MarketStatusText = Text(WidgetTree, TEXT("TradeStatus"), FText::GetEmpty(), 14, true);
	At(MarketPage, MarketStatusText, 24, 660, 576, 32);

	UCanvasPanel* Components = WidgetTree->ConstructWidget<UCanvasPanel>();
	RightSwitcher->AddChild(Components);
	At(Components, Text(WidgetTree, NAME_None, FText::FromString(TEXT("IMAGE CORE / V4")), 24, true), 24, 24, 576, 32);
	const float Widths[] = {144.f, 256.f, 320.f};
	for (int32 I = 0; I < 3; ++I)
	{
		URuinmatesTechButton* Hit = Button(WidgetTree, FText::AsNumber(static_cast<int32>(Widths[I])));
		Hit->SetSelected(I == 2);
		At(Components, Hit, 24, 80 + I * 64, Widths[I], 48);
	}
	URuinmatesTechButton* Disabled = Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "DisabledV4", "禁用"));
	Disabled->SetIsEnabled(false);
	At(Components, Disabled, 376, 80, 224, 48);
	At(Components, Button(WidgetTree, NSLOCTEXT("RuinMatesUI", "DangerV4", "危险操作"), false, true), 376, 144, 224, 48);
	for (int32 I = 0; I < 3; ++I)
	{
		At(Components, Text(WidgetTree, NAME_None, FText::AsPercent(I * 0.5f), 16), 24, 296 + I * 48, 60, 24);
		At(Components, Bar(WidgetTree, I * 0.5f), 104, 304 + I * 48, 496, 8);
	}
	URuinmatesTechSlider* Slider = CreateWidget<URuinmatesTechSlider>(this);
	At(Components, Slider, 24, 448, 576, 48);
	URuinmatesTechToggle* Toggle = CreateWidget<URuinmatesTechToggle>(this);
	At(Components, Toggle, 24, 512, 128, 52);
	At(Components, Text(WidgetTree, NAME_None, NSLOCTEXT("RuinMatesUI", "FourFinishes", "白 / 绿 / 紫 / 金"), 18), 24, 560, 576, 32);
	At(Components, Text(WidgetTree, NAME_None, NSLOCTEXT("RuinMatesUI", "PreviewNotice", "开发预览"), 14, true), 24, 632, 576, 24);

	// Footer hints are the actual existing input commands, never screenshot-only actions.
	FooterHints = WidgetTree->ConstructWidget<UHorizontalBox>();
	At(C, FooterHints, 280, 936, 1300, 40);
	FooterRight = Text(WidgetTree, TEXT("FooterStatus"), FText::GetEmpty(), 14);
	FooterRight->SetJustification(ETextJustify::Right);
	At(C, FooterRight, 1584, 948, 280, 24);
	BuildMenuLayer(Root);
}

void URuinmatesTerminalScreen::ShowVisualPreview(int32 Mode, int32 PreviewCapacity)
{
	CloseSlotMenu();
	UnbindSources();
	bLive = false;
	bMarketLive = false;
	LiveMerchant = nullptr;
	bClosing = false;
	LiveCharacter = nullptr;
	LiveContainer = nullptr;
	ActiveFilter = NAME_None;
	SelectedStableIndex = INDEX_NONE;
	Page = RuinmatesTerminalPreview::MakeInventoryPage();
	if (PreviewCapacity == 30)
	{
		Page.Capacity = 30;
		Page.Backpack.SetNum(30);
		for (int32 Index = 20; Index < 30; ++Index)
		{
			Page.Backpack[Index].StableIndex = Index;
			Page.Backpack[Index].State = ERuinmatesTerminalSlotState::Empty;
		}
	}
	SetVisualMode(Mode);
	if (PreviewNavigation) PreviewNavigation->SetVisibility(ESlateVisibility::Visible);
	if (PreviewBadge) PreviewBadge->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (SquadRadio) SquadRadio->SetPreview(true);
	BeginOpenFade();
}

void URuinmatesTerminalScreen::SetVisualMode(int32 Mode)
{
	if (bLive) return;
	CloseSlotMenu();
	VisualMode = FMath::Clamp(Mode, 0, 4);
	FRuinmatesTerminalPreviewPage RightPage = Mode == 1 ? RuinmatesTerminalPreview::MakeSearchPage() : RuinmatesTerminalPreview::MakeInventoryPage();
	Page.PageTitle = Mode == 2 ? NSLOCTEXT("RuinMatesUI", "StorageTitleV4", "基地仓库 / STORAGE")
		: Mode == 3 ? NSLOCTEXT("RuinMatesUI", "MarketTitleV4", "物资交易 / MARKET")
		: Mode == 4 ? NSLOCTEXT("RuinMatesUI", "ComponentTitleV4", "公共组件 / SHOWCASE")
		: Mode == 1 ? NSLOCTEXT("RuinMatesUI", "SearchTitleV4", "搜索容器 / SEARCH")
		: NSLOCTEXT("RuinMatesUI", "InventoryTitleV4", "个人背包 / INVENTORY");
	Page.bRightIsContainer = Mode == 1 || Mode == 2;
	Page.RightTitle = Mode == 2 ? NSLOCTEXT("RuinMatesUI", "StorageNameV4", "基地仓库") : RightPage.RightTitle;
	Page.RightSubtitle = RightPage.RightSubtitle;
	Page.RightProgress = Mode == 2 ? 18.f / 30.f : RightPage.RightProgress;
	Page.RightSlots = Mode == 2 ? RightPage.Backpack : RightPage.RightSlots;
	if (Mode == 2)
	{
		Page.RightSlots.SetNum(30);
		for (int32 Index = 20; Index < 30; ++Index)
		{
			Page.RightSlots[Index].StableIndex = Index;
			Page.RightSlots[Index].State = ERuinmatesTerminalSlotState::Empty;
		}
		Page.RightSubtitle = FText::FromString(TEXT("18 / 30"));
	}
	VisualSelected = INDEX_NONE;
	VisualQuantity = 1;
	PageFadeTime = 0.f;
	if (CharacterPreview)
	{
		CharacterPreview->SetCharacter(Mode == 0 ? Cast<ARuinmatesCharacter>(GetOwningPlayerPawn()) : nullptr);
	}
	ApplyPageText();
	RebuildSlots();
	if (Mode == 3 && MarketCells.IsEmpty())
	{
		TArray<FRuinmatesTerminalSlotView> Goods = RuinmatesTerminalPreview::MakeInventoryPage().Backpack;
		Goods.SetNum(9);
		SyncGrid(MarketGrid, MarketCells, Goods, 3, true);
		for (URuinmatesTerminalItemSlot* Cell : MarketCells)
		{
			Cell->Configure(FVector2D(168, 112), ERuinmatesTerminalSlotLayout::Grid);
			Cell->OnSlotClicked.RemoveAll(this);
			Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::PreviewMarketSelected);
		}
	}
}

void URuinmatesTerminalScreen::RefreshVisualPage()
{
	if (!DesignCanvas) return;
	const int32 NavigationMode = VisualMode == 1 ? 2 : VisualMode == 2 ? 1 : VisualMode;
	for (int32 Index = 0; Index < VisualTabs.Num(); ++Index)
	{
		if (VisualTabs[Index]) VisualTabs[Index]->SetSelected(Index == NavigationMode);
	}
	if (VisualBuy) VisualBuy->SetSelected(!bVisualSell);
	if (VisualSell) VisualSell->SetSelected(bVisualSell);
	if (VisualMode == 2)
	{
		if (SearchStatusText) SearchStatusText->SetText(NSLOCTEXT("RuinMatesUI", "StorageVisualOnly", "仓库视觉预览"));
		if (TakeAllButton) TakeAllButton->SetIsEnabled(false);
		if (TakeAllLabel) TakeAllLabel->SetText(NSLOCTEXT("RuinMatesUI", "StorageBackendMissing", "存取功能未接入"));
	}
	else if (TakeAllLabel)
	{
		TakeAllLabel->SetText(NSLOCTEXT("RuinMatesUI", "TakeFoundV4", "拾取已发现物品"));
	}
	if (FundsText && Page.Funds >= 0)
	{
		FundsText->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "SharedMoneyV4", "团队资金  {0}"), FText::AsNumber(Page.Funds)));
	}
	if (MarketGoods) MarketGoods->SetVisibility(bVisualSell ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	if (SellPrompt) SellPrompt->SetVisibility(bVisualSell ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const TArray<FRuinmatesTerminalSlotView> Goods = RuinmatesTerminalPreview::MakeInventoryPage().Backpack;
	const TArray<FRuinmatesTerminalSlotView>& Source = bVisualSell ? Page.Backpack : Goods;
	const FRuinmatesTerminalSlotView* Item = Source.IsValidIndex(VisualSelected) ? &Source[VisualSelected] : nullptr;
	const bool bValid = Item && Item->State == ERuinmatesTerminalSlotState::Revealed;
	if (MarketName) MarketName->SetText(bValid ? Item->Name : NSLOCTEXT("RuinMatesUI", "SelectTradeItem", "未选择物资"));
	const int32 Value = bValid ? Item->UnitValue : 0;
	if (MarketPrice) MarketPrice->SetText(FText::AsNumber(Value));
	if (MarketQuantity) MarketQuantity->SetText(FText::AsNumber(VisualQuantity));
	if (MarketTotal) MarketTotal->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "TradePreviewTotal", "合计  {0}"), FText::AsNumber(Value * VisualQuantity)));
}

void URuinmatesTerminalScreen::PreviewInventory() { SetVisualMode(0); }
void URuinmatesTerminalScreen::PreviewSearch() { SetVisualMode(1); }
void URuinmatesTerminalScreen::PreviewStorage() { SetVisualMode(2); }
void URuinmatesTerminalScreen::PreviewMarket() { SetVisualMode(3); }
void URuinmatesTerminalScreen::PreviewComponents() { SetVisualMode(4); }
void URuinmatesTerminalScreen::PreviewBuy() { if (bMarketLive) { HandleMarketBuyMode(); return; } bVisualSell = false; VisualSelected = INDEX_NONE; RefreshVisualPage(); }
void URuinmatesTerminalScreen::PreviewSell() { if (bMarketLive) { HandleMarketSellMode(); return; } bVisualSell = true; VisualSelected = INDEX_NONE; RefreshVisualPage(); }
void URuinmatesTerminalScreen::PreviewQuantityUp() { if (bMarketLive) { HandleMarketQuantityUp(); return; } VisualQuantity = FMath::Min(99, VisualQuantity + 1); RefreshVisualPage(); }
void URuinmatesTerminalScreen::PreviewQuantityDown() { if (bMarketLive) { HandleMarketQuantityDown(); return; } VisualQuantity = FMath::Max(1, VisualQuantity - 1); RefreshVisualPage(); }
void URuinmatesTerminalScreen::PreviewMarketSelected(int32 Index) { if (bMarketLive) { HandleMarketSelected(Index); return; } VisualSelected = Index; VisualQuantity = 1; RefreshVisualPage(); }

// ---------------------------------------------------------------------------
// Live market: the V4 market page bound to a real merchant and the shared backend.
// ---------------------------------------------------------------------------

void URuinmatesTerminalScreen::OpenMarket(ARuinmatesCharacter* InCharacter, ARuinmatesMerchant* InMerchant)
{
	if (!IsValid(InCharacter) || !IsValid(InMerchant))
	{
		return;
	}

	CloseSlotMenu();
	UnbindSources();
	bLive = false;
	bMarketLive = true;
	bClosing = false;
	LiveCharacter = InCharacter;
	LiveMerchant = InMerchant;
	LiveContainer = nullptr;
	ActiveFilter = NAME_None;
	// A reopened market never inherits a pending request, a selection or a stale result line.
	TradeState.Reset();
	VisualSelected = INDEX_NONE;
	VisualQuantity = 1;
	bVisualSell = false;
	if (MarketStatusText)
	{
		MarketStatusText->SetText(FText::GetEmpty());
	}
	if (SquadRadio)
	{
		SquadRadio->SetPreview(false);
	}
	if (CharacterPreview)
	{
		CharacterPreview->SetCharacter(nullptr);
	}

	SetVisualMode(3);
	BindSources();
	RefreshMarketLive();
	BeginOpenFade();
}

bool URuinmatesTerminalScreen::IsMarketSelectionValid() const
{
	if (bVisualSell)
	{
		return Page.Backpack.IsValidIndex(VisualSelected)
			&& Page.Backpack[VisualSelected].State == ERuinmatesTerminalSlotState::Revealed;
	}
	return MarketGoodsIds.IsValidIndex(VisualSelected);
}

void URuinmatesTerminalScreen::ClearMarketSelection()
{
	VisualSelected = INDEX_NONE;
	VisualQuantity = 1;
	TradeState.ClearSelection();
}

void URuinmatesTerminalScreen::RefreshMarketLive()
{
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	ARuinmatesMerchant* Merchant = LiveMerchant.Get();
	if (!bMarketLive || !IsValid(Character) || !IsValid(Merchant))
	{
		return;
	}

	URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	ARuinmatesGameState* GameState = GetWorld() ? GetWorld()->GetGameState<ARuinmatesGameState>() : nullptr;
	if (GameState)
	{
		LiveGameState = GameState;
	}

	Page.PageTitle = NSLOCTEXT("RuinMatesUI", "MarketTitleV4", "物资交易 / MARKET");
	Page.PackTitle = Backpack ? RuinmatesItems::GetBackpackTierName(Backpack->GetTier())
		: NSLOCTEXT("RuinMatesUI", "PackMissing", "背包");
	Page.BackpackTierName = Page.PackTitle;
	Page.Capacity = Backpack ? Backpack->GetCapacity() : 0;
	Page.Used = Backpack ? Backpack->GetUsedSlotCount() : 0;
	Page.LootValue = Backpack ? Backpack->GetLootValue() : 0;
	Page.Funds = GameState ? GameState->GetSharedFunds() : -1;
	Page.bRightIsContainer = false;

	// Left column always shows the real backpack, empty cells included.
	Page.Backpack.Reset();
	const TArray<FRuinmatesItemStack> NoSlots;
	const TArray<FRuinmatesItemStack>& Slots = Backpack ? Backpack->GetSlots() : NoSlots;
	const int32 SlotCount = FMath::Max(Page.Capacity, Slots.Num());
	for (int32 Index = 0; Index < SlotCount; ++Index)
	{
		FRuinmatesTerminalSlotView View;
		if (Slots.IsValidIndex(Index))
		{
			MakeSlotView(Slots[Index], Index, View);
		}
		else
		{
			View.StableIndex = Index;
			View.State = ERuinmatesTerminalSlotState::Empty;
		}
		View.bSelected = bVisualSell && Index == VisualSelected;
		Page.Backpack.Add(View);
	}

	ApplyPageText();
	RebuildSlots();
	if (RightSwitcher)
	{
		RightSwitcher->SetActiveWidgetIndex(2);
	}
	if (VisualBuy) VisualBuy->SetSelected(!bVisualSell);
	if (VisualSell) VisualSell->SetSelected(bVisualSell);

	// Right column: merchant goods in buy mode, the revealed backpack stacks in sell mode.
	TArray<FRuinmatesTerminalSlotView> Goods;
	MarketGoodsIds.Reset();
	if (!bVisualSell)
	{
		for (const FName& ItemId : Merchant->GetMerchantItems())
		{
			const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(ItemId) : nullptr;
			if (!Row)
			{
				continue;
			}
			FRuinmatesTerminalSlotView View;
			View.StableIndex = Goods.Num();
			View.State = ERuinmatesTerminalSlotState::Revealed;
			View.ItemId = ItemId;
			View.Name = Row->DisplayName;
			View.Count = 1;
			View.Quality = Row->Quality;
			View.Description = Row->Description;
			View.UnitValue = Database->GetItemBuyPrice(ItemId);
			View.Icon = Row->Icon.ToSoftObjectPath();
			View.bSelected = VisualSelected == View.StableIndex;
			Goods.Add(View);
			MarketGoodsIds.Add(ItemId);
		}
	}
	else
	{
		for (const FRuinmatesTerminalSlotView& View : Page.Backpack)
		{
			if (View.State == ERuinmatesTerminalSlotState::Revealed)
			{
				Goods.Add(View);
			}
		}
	}

	SyncGrid(MarketGrid, MarketCells, Goods, 3, true);
	for (URuinmatesTerminalItemSlot* Cell : MarketCells)
	{
		if (!Cell)
		{
			continue;
		}
		Cell->Configure(FVector2D(168.f, 112.f), ERuinmatesTerminalSlotLayout::Grid);
		Cell->OnSlotClicked.RemoveAll(this);
		Cell->OnSlotClicked.AddDynamic(this, &URuinmatesTerminalScreen::HandleMarketSelected);
	}

	if (MarketGoods)
	{
		MarketGoods->SetVisibility(bVisualSell ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (SellPrompt)
	{
		SellPrompt->SetVisibility(bVisualSell ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		SellPrompt->SetText(NSLOCTEXT("RuinMatesUI", "SellPromptLive", "选择左侧背包中的物资"));
	}

	// A stale selection (item consumed elsewhere, sold out, slot changed) is cleared, never kept.
	if (bVisualSell && TradeState.SellSlot != INDEX_NONE && Page.Backpack.IsValidIndex(TradeState.SellSlot))
	{
		const FRuinmatesTerminalSlotView& SellView = Page.Backpack[TradeState.SellSlot];
		if (SellView.State != ERuinmatesTerminalSlotState::Revealed
			|| SellView.ItemId != TradeState.SellItemId || SellView.Quality != TradeState.SellQuality)
		{
			// The stack under the remembered index is a different item now; drop the selection.
			ClearMarketSelection();
		}
	}
	if (VisualSelected != INDEX_NONE && !IsMarketSelectionValid())
	{
		ClearMarketSelection();
	}

	FText SelectedName = NSLOCTEXT("RuinMatesUI", "SelectTradeItem", "未选择物资");
	int32 UnitPrice = 0;
	int32 MaxQuantity = 1;
	if (bVisualSell && Page.Backpack.IsValidIndex(VisualSelected))
	{
		const FRuinmatesTerminalSlotView& View = Page.Backpack[VisualSelected];
		SelectedName = View.Name;
		UnitPrice = Database ? Database->GetItemSellPrice(View.ItemId) : 0;
		MaxQuantity = FMath::Max(1, View.Count);
	}
	else if (!bVisualSell && MarketGoodsIds.IsValidIndex(VisualSelected))
	{
		const FRuinmatesItemRow* Row = Database ? Database->FindItemDefinition(MarketGoodsIds[VisualSelected]) : nullptr;
		if (Row)
		{
			SelectedName = Row->DisplayName;
			UnitPrice = Database->GetItemBuyPrice(MarketGoodsIds[VisualSelected]);
			MaxQuantity = 99;
		}
	}

	const bool bHasSelection = IsMarketSelectionValid();
	VisualQuantity = FMath::Clamp(VisualQuantity, 1, FMath::Max(1, MaxQuantity));
	// int64 keeps large quantity x price combinations from overflowing before they reach the server.
	const int64 TotalPrice = static_cast<int64>(UnitPrice) * static_cast<int64>(VisualQuantity);
	if (MarketName) MarketName->SetText(bHasSelection ? SelectedName : NSLOCTEXT("RuinMatesUI", "SelectTradeItem", "未选择物资"));
	if (MarketPrice) MarketPrice->SetText(FText::AsNumber(UnitPrice));
	if (MarketQuantity) MarketQuantity->SetText(FText::AsNumber(VisualQuantity));
	if (MarketTotal) MarketTotal->SetText(FText::Format(NSLOCTEXT("RuinMatesUI", "TradeLiveTotal", "合计  {0}"),
		FText::AsNumber(TotalPrice)));
	if (MarketConfirmButton)
	{
		MarketConfirmButton->SetIsEnabled(bHasSelection && !TradeState.bPending);
		if (UTextBlock* Label = Cast<UTextBlock>(MarketConfirmButton->GetContent()))
		{
			Label->SetText(bVisualSell ? NSLOCTEXT("RuinMatesUI", "SellV4", "出售") : NSLOCTEXT("RuinMatesUI", "BuyV4", "购买"));
		}
	}
	if (!TradeState.bPending && MarketStatusText && MarketStatusText->GetText().IsEmpty())
	{
		MarketStatusText->SetText(bHasSelection
			? FText::Format(NSLOCTEXT("RuinMatesUI", "TradeHintQty", "单价 {0} × {1}"), FText::AsNumber(UnitPrice), FText::AsNumber(VisualQuantity))
			: NSLOCTEXT("RuinMatesUI", "TradeHintPick", "选择商品后确认交易"));
		MarketStatusText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextWeak()));
	}
}

void URuinmatesTerminalScreen::ShowTradeResult(bool bSuccess, const FText& Message)
{
	if (!bMarketLive)
	{
		return;
	}
	// The server answer ends the pending window and is the last word on the status line.
	TradeState.CompleteRequest();
	if (MarketStatusText)
	{
		MarketStatusText->SetText(Message);
		MarketStatusText->SetColorAndOpacity(FSlateColor(bSuccess ? RuinmatesUI::Success() : RuinmatesUI::Error()));
	}
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketBuyMode()
{
	if (!bMarketLive)
	{
		return;
	}
	bVisualSell = false;
	ClearMarketSelection();
	if (MarketStatusText) MarketStatusText->SetText(FText::GetEmpty());
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketSellMode()
{
	if (!bMarketLive)
	{
		return;
	}
	bVisualSell = true;
	ClearMarketSelection();
	if (MarketStatusText) MarketStatusText->SetText(FText::GetEmpty());
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketSelected(int32 Index)
{
	if (!bMarketLive)
	{
		return;
	}
	VisualSelected = Index;
	VisualQuantity = 1;
	if (bVisualSell && Page.Backpack.IsValidIndex(Index)
		&& Page.Backpack[Index].State == ERuinmatesTerminalSlotState::Revealed)
	{
		// Remember what the slot holds so a later refresh can prove it is still the same stack.
		TradeState.SellItemId = Page.Backpack[Index].ItemId;
		TradeState.SellQuality = Page.Backpack[Index].Quality;
		TradeState.SellSlot = Index;
	}
	else
	{
		TradeState.SellItemId = NAME_None;
		TradeState.SellSlot = INDEX_NONE;
	}
	if (MarketStatusText) MarketStatusText->SetText(FText::GetEmpty());
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketQuantityUp()
{
	if (!bMarketLive)
	{
		return;
	}
	++VisualQuantity;
	if (MarketStatusText) MarketStatusText->SetText(FText::GetEmpty());
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketQuantityDown()
{
	if (!bMarketLive)
	{
		return;
	}
	VisualQuantity = FMath::Max(1, VisualQuantity - 1);
	if (MarketStatusText) MarketStatusText->SetText(FText::GetEmpty());
	RefreshMarketLive();
}

void URuinmatesTerminalScreen::HandleMarketConfirm()
{
	if (!bMarketLive || !IsMarketSelectionValid())
	{
		return;
	}
	ARuinmatesCharacter* Character = LiveCharacter.Get();
	if (!IsValid(Character))
	{
		return;
	}

	// Mark pending and paint it before sending: a listen-server host receives the result
	// synchronously inside the request, so anything written afterwards would overwrite it.
	if (!TradeState.BeginRequest())
	{
		return;
	}
	if (MarketStatusText)
	{
		MarketStatusText->SetText(NSLOCTEXT("RuinMatesUI", "TradePending", "正在处理…"));
		MarketStatusText->SetColorAndOpacity(FSlateColor(RuinmatesUI::TextWeak()));
	}
	RefreshMarketLive();

	// Client only sends id/slot + quantity; prices and validation stay server side.
	if (bVisualSell)
	{
		const FRuinmatesTerminalSlotView* SellView = Page.Backpack.IsValidIndex(VisualSelected) ? &Page.Backpack[VisualSelected] : nullptr;
		const URuinmatesBackpackComponent* Backpack = Character->GetBackpack();
		if (SellView && Backpack)
		{
			// Snapshot so a stale sale can never move a different stack.
			Character->RequestSellItem(VisualSelected, VisualQuantity, SellView->ItemId, SellView->Quality,
				SellView->Count, Backpack->GetRevision());
		}
	}
	else if (MarketGoodsIds.IsValidIndex(VisualSelected))
	{
		Character->RequestBuyItem(MarketGoodsIds[VisualSelected], VisualQuantity);
	}
}
