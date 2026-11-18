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
};

namespace RuinmatesTerminalPreview
{
	RUINMATES_API FRuinmatesTerminalPreviewPage MakeInventoryPage();
	RUINMATES_API FRuinmatesTerminalPreviewPage MakeSearchPage();
}
