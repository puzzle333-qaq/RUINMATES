#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "UI/RuinmatesUIStyle.h"

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
#endif
