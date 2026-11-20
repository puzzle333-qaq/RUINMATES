
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
		Slot.UnitValue = 100 * (static_cast<int32>(Quality) + 1);
		return Slot;
	}

	FRuinmatesTerminalSlotView EmptySlot(int32 Index)
	{
		FRuinmatesTerminalSlotView Slot;
		Slot.StableIndex = Index;
		Slot.State = ERuinmatesTerminalSlotState::Empty;
		return Slot;
	}

	// Preview-only art lookup. Live pages read FRuinmatesItemRow::Icon instead.
	FSoftObjectPath PreviewIcon(const TCHAR* Name)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/RUINMATES/UI/AssetPackV4/Derived/Icons/%s.%s"), Name, Name));
	}

	void PadTo(TArray<FRuinmatesTerminalSlotView>& Slots, int32 Capacity)
	{
		for (int32 Index = Slots.Num(); Index < Capacity; ++Index)
		{
			Slots.Add(EmptySlot(Index));
		}
	}
}

TArray<FRuinmatesTerminalSlotView> RuinmatesTerminalDisplay::FilterBackpackSlots(
	const TArray<FRuinmatesTerminalSlotView>& Slots, FName Category)
{
	if (Category.IsNone())
	{
		return Slots;
	}

	TArray<FRuinmatesTerminalSlotView> Filtered;
	for (const FRuinmatesTerminalSlotView& Slot : Slots)
	{
		if (Slot.State == ERuinmatesTerminalSlotState::Revealed && Slot.Category == Category)
		{
			Filtered.Add(Slot);
		}
	}
	return Filtered;
}

FRuinmatesTerminalPreviewPage RuinmatesTerminalPreview::MakeInventoryPage()
{
	FRuinmatesTerminalPreviewPage Page;
	Page.PageTitle = NSLOCTEXT("RuinMatesUI", "PageInventory", "背包");
	Page.PackTitle = NSLOCTEXT("RuinMatesUI", "PackPreview", "探索背包");
	Page.Capacity = 20;
	// Isolated art samples. These names, values and counts never enter DT_Items or an inventory.
	const TCHAR* Names[] = {
		TEXT("野外工具包"), TEXT("弹药箱"), TEXT("绳索"), TEXT("5.56mm 弹药"), TEXT("9mm 弹药"),
		TEXT("医疗包"), TEXT("急救药瓶"), TEXT("止痛药"), TEXT("绷带"), TEXT("兴奋剂"),
		TEXT("烟雾弹"), TEXT("破片手雷"), TEXT("无人机信标"), TEXT("撤离信号棒"), TEXT("四倍镜"),
		TEXT("消音器"), TEXT("前握把"), TEXT("激光模块")
	};
	const TCHAR* Icons[] = {
		TEXT("T_Icon_FieldKit"), TEXT("T_Icon_AmmoCase"), TEXT("T_Icon_Rope"), TEXT("T_Icon_Ammo556"), TEXT("T_Icon_Ammo9mm"),
		TEXT("T_Icon_MedKit"), TEXT("T_Icon_FirstAid"), TEXT("T_Icon_Painkillers"), TEXT("T_Icon_Bandage"), TEXT("T_Icon_Stimulant"),
		TEXT("T_Icon_Smoke"), TEXT("T_Icon_FragGrenade"), TEXT("T_Icon_UAVBeacon"), TEXT("T_Icon_ExtractionFlare"), TEXT("T_Icon_Scope4x"),
		TEXT("T_Icon_Suppressor"), TEXT("T_Icon_Foregrip"), TEXT("T_Icon_LaserModule")
	};
	const int32 Counts[] = {1, 1, 1, 120, 45, 2, 4, 3, 5, 1, 2, 3, 1, 1, 1, 1, 1, 1};
	const ERuinmatesItemQuality Finishes[] = {ERuinmatesItemQuality::Common, ERuinmatesItemQuality::Fine,
		ERuinmatesItemQuality::Rare, ERuinmatesItemQuality::Precious};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		Page.Backpack.Add(Item(Index, FText::FromString(Names[Index]), Counts[Index], Finishes[Index % 4],
			Index >= 5 && Index <= 11 ? TEXT("Consumable") : TEXT("Material")));
	}
	for (int32 Index = 0; Index < Page.Backpack.Num() && Index < UE_ARRAY_COUNT(Icons); ++Index)
	{
		Page.Backpack[Index].Icon = PreviewIcon(Icons[Index]);
	}
	Page.Used = Page.Backpack.Num();
	Page.Funds = 9863;
	Page.BackpackTierName = NSLOCTEXT("RuinMatesUI", "PackTactical", "高级战术背包");
	Page.LootValue = 16800;
	PadTo(Page.Backpack, Page.Capacity);
	Page.Backpack[2].bSelected = true;
	Page.RightTitle = NSLOCTEXT("RuinMatesUI", "Survivor", "幸存者");
	Page.RightSubtitle = NSLOCTEXT("RuinMatesUI", "LevelPreview", "LV.06");
	Page.FooterLeft = NSLOCTEXT("RuinMatesUI", "InvFooter", "TAB 关闭    左键选择    右键丢弃");
	Page.FooterRight = NSLOCTEXT("RuinMatesUI", "SortPreview", "整理");
	return Page;
}

FRuinmatesTerminalPreviewPage RuinmatesTerminalPreview::MakeSearchPage()
{
	FRuinmatesTerminalPreviewPage Page = MakeInventoryPage();
	Page.PageTitle = NSLOCTEXT("RuinMatesUI", "PageSearch", "物资回收");
	Page.PackTitle = NSLOCTEXT("RuinMatesUI", "MyPack", "我的背包");
	Page.bRightIsContainer = true;
	Page.RightTitle = NSLOCTEXT("RuinMatesUI", "Crate", "废弃研究所储物箱");
	Page.RightSubtitle = NSLOCTEXT("RuinMatesUI", "SearchCount", "3 / 6");
	Page.RightProgress = 3.f / 6.f;
	Page.RightSlots = {
		Item(0, NSLOCTEXT("RuinMatesUI", "Wire", "电线"), 1, ERuinmatesItemQuality::Common, TEXT("Material")),
		Item(1, NSLOCTEXT("RuinMatesUI", "Meds2", "药品"), 2, ERuinmatesItemQuality::Fine, TEXT("Consumable")),
		Item(2, NSLOCTEXT("RuinMatesUI", "Gold2", "金条"), 1, ERuinmatesItemQuality::Precious, TEXT("Material"))
	};
	Page.RightSlots[0].Name = NSLOCTEXT("RuinMatesUI", "RopePreview", "绳索");
	Page.RightSlots[0].Icon = PreviewIcon(TEXT("T_Icon_Rope"));
	Page.RightSlots[1].Name = NSLOCTEXT("RuinMatesUI", "MedicalPreview", "绷带");
	Page.RightSlots[1].Icon = PreviewIcon(TEXT("T_Icon_Bandage"));
	Page.RightSlots[2].Name = NSLOCTEXT("RuinMatesUI", "BeaconPreview", "无人机信标");
	Page.RightSlots[2].Icon = PreviewIcon(TEXT("T_Icon_UAVBeacon"));
	FRuinmatesTerminalSlotView Scanning = EmptySlot(3);
	Scanning.State = ERuinmatesTerminalSlotState::Searching;
	FRuinmatesTerminalSlotView HiddenA = EmptySlot(4);
	HiddenA.State = ERuinmatesTerminalSlotState::Unknown;
	FRuinmatesTerminalSlotView HiddenB = EmptySlot(5);
	HiddenB.State = ERuinmatesTerminalSlotState::Unknown;
	Page.RightSlots.Add(Scanning);
	Page.RightSlots.Add(HiddenA);
	Page.RightSlots.Add(HiddenB);
	PadTo(Page.RightSlots, 12);
	Page.FooterLeft = NSLOCTEXT("RuinMatesUI", "SearchFooter", "ESC 关闭    点击已发现物品拾取");
	Page.FooterRight = NSLOCTEXT("RuinMatesUI", "NotPaused", "游戏不暂停");

	return Page;
}
