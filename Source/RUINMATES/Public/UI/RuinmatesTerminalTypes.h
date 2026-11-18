#pragma once

#include "CoreMinimal.h"
#include "Inventory/RuinmatesItemTypes.h"
#include "RuinmatesTerminalTypes.generated.h"

// Display-only states for the terminal preview. Not a gameplay authority model.
UENUM()
enum class ERuinmatesTerminalSlotState : uint8
{
	Empty,
	Unknown,
	Searching,
	Revealed,
	Pending,
	Taken
};

/** Visual size class of a terminal cell. */
enum class ERuinmatesTerminalSlotLayout : uint8
{
	Grid,	// backpack grid, square
	Card	// search container card, taller than wide
};

USTRUCT()
struct FRuinmatesTerminalSlotView
{
	GENERATED_BODY()

	UPROPERTY()
	int32 StableIndex = INDEX_NONE;

	UPROPERTY()
	FText Name;

	UPROPERTY()
	int32 Count = 0;

	UPROPERTY()
	ERuinmatesItemQuality Quality = ERuinmatesItemQuality::Common;

	UPROPERTY()
	FName Category = NAME_None;

	UPROPERTY()
	ERuinmatesTerminalSlotState State = ERuinmatesTerminalSlotState::Empty;

	UPROPERTY()
	bool bSelected = false;

	/** Item icon from DT_Items (FRuinmatesItemRow::Icon). Null falls back to the generic icon. */
	UPROPERTY()
	FSoftObjectPath Icon;

	UPROPERTY()
	FText Description;

	UPROPERTY()
	int32 UnitValue = 0;

	/** Hidden by the active category filter: drawn dimmed and not clickable, index unchanged. */
	UPROPERTY()
	bool bDimmed = false;
};

struct FRuinmatesTerminalPreviewPage
{
	FText PageTitle;
	FText PackTitle;
	int32 Used = 0;
	int32 Capacity = 20;
	int32 LootValue = 0;
	TArray<FRuinmatesTerminalSlotView> Backpack;
	FText RightTitle;
	FText RightSubtitle;
	float RightProgress = 0.f;
	bool bRightIsContainer = false;
	TArray<FRuinmatesTerminalSlotView> RightSlots;
	FText FooterLeft;
	FText FooterRight;
	FText StatusText;
	/** Team wallet from ARuinmatesGameState; INDEX_NONE hides the funds chip. */
	int32 Funds = INDEX_NONE;
	/** Right panel (character) extras. */
	FText BackpackTierName;
	bool bHasWeapon = false;
};

namespace RuinmatesTerminalPreview
{
	RUINMATES_API FRuinmatesTerminalPreviewPage MakeInventoryPage();
	RUINMATES_API FRuinmatesTerminalPreviewPage MakeSearchPage();
}
