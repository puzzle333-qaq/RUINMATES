#include "UI/RuinmatesTerminalTypes.h"

namespace
{
	FRuinmatesTerminalSlotView Item(int32 Index, const FText& Name, int32 Count, ERuinmatesItemQuality Quality, const FName& Category)
	{
		FRuinmatesTerminalSlotView Slot;
		Slot.StableIndex = Index;
		Slot.Name = Name;
		Slot.Count = Count;
		Slot.Quality = Quality;
		Slot.Category = Category;
		Slot.State = ERuinmatesTerminalSlotState::Revealed;
		return Slot;
	}

	FRuinmatesTerminalSlotView EmptySlot(int32 Index)
	{
		FRuinmatesTerminalSlotView Slot;
		Slot.StableIndex = Index;
		Slot.State = ERuinmatesTerminalSlotState::Empty;
		return Slot;
	}

	void PadTo(TArray<FRuinmatesTerminalSlotView>& Slots, int32 Capacity)
	{
		for (int32 Index = Slots.Num(); Index < Capacity; ++Index)
		{
			Slots.Add(EmptySlot(Index));
		}
	}
}

FRuinmatesTerminalPreviewPage RuinmatesTerminalPreview::MakeInventoryPage()
{
	FRuinmatesTerminalPreviewPage Page;
	Page.PageTitle = NSLOCTEXT("RuinMatesUI", "PageInventory", "背包");
	Page.PackTitle = NSLOCTEXT("RuinMatesUI", "PackPreview", "探索背包");
	Page.Capacity = 20;
	Page.Backpack = {
		Item(0, NSLOCTEXT("RuinMatesUI", "Ammo", "弹药"), 12, ERuinmatesItemQuality::Common, TEXT("Material")),
		Item(1, NSLOCTEXT("RuinMatesUI", "Meds", "药品"), 4, ERuinmatesItemQuality::Fine, TEXT("Consumable")),
		Item(2, NSLOCTEXT("RuinMatesUI", "Chip", "实验基因芯片"), 1, ERuinmatesItemQuality::Rare, TEXT("Material")),
		Item(3, NSLOCTEXT("RuinMatesUI", "Sample", "样本"), 1, ERuinmatesItemQuality::Advanced, TEXT("Quest")),
		Item(4, NSLOCTEXT("RuinMatesUI", "Battery", "电池"), 2, ERuinmatesItemQuality::Fine, TEXT("Material")),
		Item(5, NSLOCTEXT("RuinMatesUI", "Gold", "金条"), 1, ERuinmatesItemQuality::Precious, TEXT("Material")),
		Item(6, NSLOCTEXT("RuinMatesUI", "Bandage", "绷带"), 6, ERuinmatesItemQuality::Common, TEXT("Consumable"))
	};
	Page.Used = 7;
	Page.LootValue = 16800;
	PadTo(Page.Backpack, Page.Capacity);
	Page.Backpack[2].bSelected = true;
	Page.RightTitle = NSLOCTEXT("RuinMatesUI", "Survivor", "幸存者");
	Page.RightSubtitle = NSLOCTEXT("RuinMatesUI", "LevelPreview", "LV.06");
	Page.FooterLeft = NSLOCTEXT("RuinMatesUI", "InvFooter", "TAB 关闭    右键查看    拖动转移");
	Page.FooterRight = NSLOCTEXT("RuinMatesUI", "SortPreview", "整理");
	return Page;
}

FRuinmatesTerminalPreviewPage RuinmatesTerminalPreview::MakeSearchPage()
{
	FRuinmatesTerminalPreviewPage Page = MakeInventoryPage();
	Page.PageTitle = NSLOCTEXT("RuinMatesUI", "PageSearch", "物资回收");
	Page.PackTitle = NSLOCTEXT("RuinMatesUI", "MyPack", "我的背包");
	Page.Used = 8;
	Page.LootValue = 3680;
	Page.Backpack[7] = Item(7, NSLOCTEXT("RuinMatesUI", "Watch", "金表"), 1, ERuinmatesItemQuality::Rare, TEXT("Material"));
	Page.bRightIsContainer = true;
	Page.RightTitle = NSLOCTEXT("RuinMatesUI", "Crate", "废弃研究所储物箱");
	Page.RightSubtitle = NSLOCTEXT("RuinMatesUI", "SearchCount", "3 / 6");
	Page.RightProgress = 3.f / 6.f;
	Page.RightSlots = {
		Item(0, NSLOCTEXT("RuinMatesUI", "Wire", "电线"), 1, ERuinmatesItemQuality::Common, TEXT("Material")),
		Item(1, NSLOCTEXT("RuinMatesUI", "Meds2", "药品"), 2, ERuinmatesItemQuality::Fine, TEXT("Consumable")),
		Item(2, NSLOCTEXT("RuinMatesUI", "Gold2", "金条"), 1, ERuinmatesItemQuality::Precious, TEXT("Material"))
	};
	FRuinmatesTerminalSlotView Scanning = EmptySlot(3);
	Scanning.State = ERuinmatesTerminalSlotState::Searching;
	FRuinmatesTerminalSlotView HiddenA = EmptySlot(4);
	HiddenA.State = ERuinmatesTerminalSlotState::Unknown;
	FRuinmatesTerminalSlotView HiddenB = EmptySlot(5);
	HiddenB.State = ERuinmatesTerminalSlotState::Unknown;
	Page.RightSlots.Add(Scanning);
	Page.RightSlots.Add(HiddenA);
	Page.RightSlots.Add(HiddenB);
	Page.FooterLeft = NSLOCTEXT("RuinMatesUI", "SearchFooter", "ESC 关闭    Shift+左键 快速转移    拖动");
	Page.FooterRight = NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停");
	Page.StatusText = NSLOCTEXT("RuinMatesUI", "NightSoon", "即将入夜");
	return Page;
}
