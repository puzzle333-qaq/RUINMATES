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
	FName ItemId = NAME_None;

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

	/** Copied from the stack so the context menu can enable "use" without a database lookup. */
	UPROPERTY()
	ERuinmatesItemUseEffect UseEffect = ERuinmatesItemUseEffect::None;

	UPROPERTY()
	float UseAmount = 0.f;

	/** Legacy dimming flag; active category filtering now omits non-matching cells. */
	UPROPERTY()
	bool bDimmed = false;

	UPROPERTY()
	bool bDropTarget = false;

	UPROPERTY()
	bool bDropAllowed = false;
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

namespace RuinmatesTerminalDisplay
{
	RUINMATES_API TArray<FRuinmatesTerminalSlotView> FilterBackpackSlots(
		const TArray<FRuinmatesTerminalSlotView>& Slots, FName Category);
}

/** Market transaction state shared by the V4 market page: pending request + selected sell stack. */
struct RUINMATES_API FRuinmatesMarketTradeState
{
	bool bPending = false;
	FName SellItemId;
	ERuinmatesItemQuality SellQuality = ERuinmatesItemQuality::Common;
	int32 SellSlot = INDEX_NONE;

	/** True while a request is in flight and the confirm button must stay disabled. */
	bool IsPending() const { return bPending; }

	/** Starts a request; returns false when one is already in flight (double-click guard). */
	bool BeginRequest()
	{
		if (bPending)
		{
			return false;
		}
		bPending = true;
		return true;
	}

	/** Server answer received: the pending window ends but the selection stays for another try. */
	void CompleteRequest() { bPending = false; }

	/** Drops the selected sell stack; its identity can no longer be trusted. */
	void ClearSelection()
	{
		SellItemId = NAME_None;
		SellQuality = ERuinmatesItemQuality::Common;
		SellSlot = INDEX_NONE;
	}

	/** Full reset, used when the market is reopened or closed. */
	void Reset()
	{
		bPending = false;
		ClearSelection();
	}
};
