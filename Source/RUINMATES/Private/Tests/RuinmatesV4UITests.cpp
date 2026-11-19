#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "UI/RuinmatesUIStyle.h"
#include "Engine/DataTable.h"
#include "Inventory/RuinmatesItemTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesV4ArtContract, "RuinMates.UI.V4.ImportedArtContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesV4ArtContract::RunTest(const FString& Parameters)
{
	const TCHAR* Required[] = {
		TEXT("Art/Buttons/T_UI_Button_Normal"), TEXT("Art/Buttons/T_UI_Button_Glow"),
		TEXT("Art/Buttons/T_UI_Button_Disabled"), TEXT("Art/Navigation/T_UI_Tab_Normal"),
		TEXT("Art/Navigation/T_UI_Tab_Glow"), TEXT("Art/Panels/T_UI_Panel_Large"),
		TEXT("Art/HUD/T_UI_Radio_Normal"), TEXT("Art/HUD/T_UI_Health_Fill"),
		TEXT("Art/HUD/T_UI_StaminaRing_Full"), TEXT("Derived/Slots/T_UI_Slot_White"),
		TEXT("Derived/Slots/T_UI_Slot_Green"), TEXT("Derived/Slots/T_UI_Slot_Purple"),
		TEXT("Derived/Slots/T_UI_Slot_Gold"), TEXT("Derived/Slots/T_UI_SlotOverlay_Selected"),
		TEXT("Derived/Icons/T_Icon_FieldKit"), TEXT("Derived/Icons/T_Icon_Bandage")
	};
	for (const TCHAR* Path : Required)
	{
		const FSlateBrush Brush = RuinmatesUI::V4(Path);
		UTexture2D* Texture = Cast<UTexture2D>(Brush.GetResourceObject());
		if (!TestNotNull(FString::Printf(TEXT("Imported art %s"),Path),Texture)) continue;
		TestEqual(TEXT("Texture group UI"),static_cast<int32>(Texture->LODGroup),static_cast<int32>(TEXTUREGROUP_UI));
		TestTrue(TEXT("sRGB artwork"),Texture->SRGB != 0);
	}
	const FSlateBrush Base = RuinmatesUI::V4(TEXT("Art/Buttons/T_UI_Button_Normal"));
	const FSlateBrush Glow = RuinmatesUI::V4(TEXT("Art/Buttons/T_UI_Button_Glow"));
	TestTrue(TEXT("Base and glow share geometry"),Base.GetImageSize().Equals(Glow.GetImageSize()) && Base.Margin == Glow.Margin);
	TestTrue(TEXT("Source colour is not multiplied"),Base.TintColor.GetSpecifiedColor() == FLinearColor::White);
	TestTrue(TEXT("Rectangular cell is authored at the exact 168x112 logical ratio"),
		RuinmatesUI::V4(TEXT("Derived/Slots/T_UI_Slot_White")).GetImageSize().Equals(FVector2D(168,112)));
	TestEqual(TEXT("Fine and Advanced retain backend identity but share the green finish"),
		FString(RuinmatesUI::QualityFinish(1)),FString(RuinmatesUI::QualityFinish(2)));
	TestTrue(TEXT("Missing optional icon is null safe"),
		RuinmatesUI::TextureAt(FSoftObjectPath()) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesV4HiddenViews, "RuinMates.UI.V4.HiddenPreviewPrivacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesV4HiddenViews::RunTest(const FString& Parameters)
{
	const FRuinmatesTerminalPreviewPage Search = RuinmatesTerminalPreview::MakeSearchPage();
	int32 Hidden = 0;
	for (const FRuinmatesTerminalSlotView& View : Search.RightSlots)
	{
		if (View.State != ERuinmatesTerminalSlotState::Unknown && View.State != ERuinmatesTerminalSlotState::Searching) continue;
		++Hidden;
		TestTrue(TEXT("Unknown view has no name"),View.Name.IsEmpty());
		TestTrue(TEXT("Unknown view has no icon"),View.Icon.IsNull());
		TestTrue(TEXT("Unknown view has no description"),View.Description.IsEmpty());
		TestEqual(TEXT("Unknown view has no count"),View.Count,0);
		TestEqual(TEXT("Unknown view has no price"),View.UnitValue,0);
	}
	TestTrue(TEXT("Search includes hidden cells"),Hidden > 0);
	const FRuinmatesTerminalPreviewPage Inventory = RuinmatesTerminalPreview::MakeInventoryPage();
	TestEqual(TEXT("Preview grid follows the actual capacity"),Inventory.Backpack.Num(),Inventory.Capacity);
	TSet<int32> StableSlots;
	for (const FRuinmatesTerminalSlotView& View : Inventory.Backpack) StableSlots.Add(View.StableIndex);
	TestEqual(TEXT("Every cell has a unique stable identity"),StableSlots.Num(),Inventory.Backpack.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesV4CategoryDisplay, "RuinMates.UI.V4.CategoryDisplayMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesV4CategoryDisplay::RunTest(const FString& Parameters)
{
	const FRuinmatesTerminalPreviewPage Inventory = RuinmatesTerminalPreview::MakeInventoryPage();
	const TArray<FRuinmatesTerminalSlotView> Consumables =
		RuinmatesTerminalDisplay::FilterBackpackSlots(Inventory.Backpack, TEXT("Consumable"));
	TestTrue(TEXT("Filtered category contains matching items"), Consumables.Num() > 0);
	TSet<int32> StableIndices;
	for (const FRuinmatesTerminalSlotView& View : Consumables)
	{
		TestEqual(TEXT("Filtered items retain their true category"), View.Category, FName(TEXT("Consumable")));
		StableIndices.Add(View.StableIndex);
	}
	TestEqual(TEXT("Filtered rows remain unique by real slot index"), StableIndices.Num(), Consumables.Num());
	TestEqual(TEXT("Overview preserves all real slots, including empties"),
		RuinmatesTerminalDisplay::FilterBackpackSlots(Inventory.Backpack, NAME_None).Num(), Inventory.Capacity);
	TestEqual(TEXT("An empty category returns no fake empty slots"),
		RuinmatesTerminalDisplay::FilterBackpackSlots(Inventory.Backpack, TEXT("Weapon")).Num(), 0);
	return true;
}
#

// Category filtering over the real item table: compact rows, true slot indices, no fake cells.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesRealItemFilterTest, "RuinMates.UI.V4.CategoryFilterRealItems",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesRealItemFilterTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	TArray<FName> RowNames = Table->GetRowNames();
	RowNames.Sort(FNameLexicalLess());

	// Real rows at scattered slots, with empty slots between them. The capacity grows with the
	// table so adding items to DT_Items never breaks this test.
	TArray<FRuinmatesTerminalSlotView> Slots;
	Slots.SetNum(FMath::Max(4, RowNames.Num() * 2 + 2));
	int32 Cursor = 0;
	for (const FName& RowName : RowNames)
	{
		const FRuinmatesItemRow* Row = Table->FindRow<FRuinmatesItemRow>(RowName, TEXT("FilterTest"), false);
		if (!Row || Slots.Num() <= Cursor)
		{
			continue;
		}
		FRuinmatesTerminalSlotView& View = Slots[Cursor];
		View.StableIndex = Cursor;
		View.State = ERuinmatesTerminalSlotState::Revealed;
		View.Name = Row->DisplayName;
		View.ItemId = RowName;
		View.Category = FName(*StaticEnum<ERuinmatesItemCategory>()->GetNameStringByValue(static_cast<int64>(Row->Category)));
		Cursor += 2;
	}

	const TArray<FRuinmatesTerminalSlotView> All = RuinmatesTerminalDisplay::FilterBackpackSlots(Slots, NAME_None);
	TestEqual(TEXT("Overview keeps every real slot"), All.Num(), Slots.Num());

	for (int32 RowIndex = 0; RowIndex < RowNames.Num(); ++RowIndex)
	{
		const FRuinmatesItemRow* Row = Table->FindRow<FRuinmatesItemRow>(RowNames[RowIndex], TEXT("FilterTest"), false);
		if (!Row)
		{
			continue;
		}
		const FName Category(*StaticEnum<ERuinmatesItemCategory>()->GetNameStringByValue(static_cast<int64>(Row->Category)));
		const TArray<FRuinmatesTerminalSlotView> Filtered = RuinmatesTerminalDisplay::FilterBackpackSlots(Slots, Category);
		for (const FRuinmatesTerminalSlotView& View : Filtered)
		{
			TestTrue(TEXT("Filtered slot keeps a real slot index"), Slots.IsValidIndex(View.StableIndex));
			if (Slots.IsValidIndex(View.StableIndex))
			{
				TestTrue(TEXT("Reported index points at the same item"),
					Slots[View.StableIndex].State == ERuinmatesTerminalSlotState::Revealed
					&& Slots[View.StableIndex].ItemId == View.ItemId);
			}
			TestEqual(TEXT("Filtered row carries the requested category"), View.Category, Category);
			TestTrue(TEXT("Filtered rows are revealed items, never empty cells"),
				View.State == ERuinmatesTerminalSlotState::Revealed);
		}
	}

	// Compaction: the filtered list must not carry the empty slots of the overview.
	int32 RevealedInTable = 0;
	for (const FRuinmatesTerminalSlotView& View : Slots)
	{
		RevealedInTable += View.State == ERuinmatesTerminalSlotState::Revealed ? 1 : 0;
	}
	// Sum once per distinct category: several rows share one category, so a per-row sum would
	// count the same cells repeatedly.
	TSet<FName> DistinctCategories;
	for (int32 RowIndex = 0; RowIndex < RowNames.Num(); ++RowIndex)
	{
		const FRuinmatesItemRow* Row = Table->FindRow<FRuinmatesItemRow>(RowNames[RowIndex], TEXT("FilterTest"), false);
		if (Row)
		{
			DistinctCategories.Add(FName(*StaticEnum<ERuinmatesItemCategory>()->GetNameStringByValue(static_cast<int64>(Row->Category))));
		}
	}
	int32 FilteredTotal = 0;
	for (const FName& Category : DistinctCategories)
	{
		FilteredTotal += RuinmatesTerminalDisplay::FilterBackpackSlots(Slots, Category).Num();
	}
	TestEqual(TEXT("Every revealed item is reachable through its own category"), FilteredTotal, RevealedInTable);
	// A synthetic key cannot collide with a future DT_Items category, unlike assuming a real
	// category (e.g. Weapon) stays empty.
	const FName UnusedCategory(TEXT("__NoSuchItemCategory"));
	TestEqual(TEXT("A category with no matching items returns no cells"),
		RuinmatesTerminalDisplay::FilterBackpackSlots(Slots, UnusedCategory).Num(), 0);
	return true;
}

// Bound icons must be valid project UI textures; rows without artwork stay on the placeholder.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesItemIconReferenceTest, "RuinMates.UI.V4.ItemIconReferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesItemIconReferenceTest::RunTest(const FString& Parameters)
{
	UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/RUINMATES/Data/Items/DT_Items.DT_Items"));
	if (!TestNotNull(TEXT("DT_Items exists"), Table))
	{
		return false;
	}

	int32 IconsBound = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FRuinmatesItemRow* Row = reinterpret_cast<const FRuinmatesItemRow*>(Pair.Value);
		if (!Row)
		{
			continue;
		}
		if (Row->Icon.IsNull())
		{
			// No artwork is a valid state: the item cell falls back to the neutral placeholder.
			continue;
		}
		++IconsBound;

		// Any project asset naming is acceptable; the icon only has to be a loadable UI texture.
		const FString IconPath = Row->Icon.ToSoftObjectPath().ToString();
		TestTrue(FString::Printf(TEXT("%s icon lives in project content (%s)"), *Pair.Key.ToString(), *IconPath),
			IconPath.StartsWith(TEXT("/Game/")));

		UTexture2D* Texture = Row->Icon.LoadSynchronous();
		if (TestNotNull(FString::Printf(TEXT("%s icon loads"), *Pair.Key.ToString()), Texture))
		{
			TestEqual(TEXT("Item icon is a UI texture"),
				static_cast<int32>(Texture->LODGroup), static_cast<int32>(TEXTUREGROUP_UI));
		}
	}

	TestTrue(TEXT("The seeded item set has bound artwork"), IconsBound >= 6);
	return true;
}
#
#endif