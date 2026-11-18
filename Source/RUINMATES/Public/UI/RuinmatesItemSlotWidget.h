#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/RuinmatesItemTypes.h"
#include "RuinmatesItemSlotWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuinmatesItemSlotClicked, int32, SlotIndex);

// One uniform cell. Used by the backpack, the search container, and later the stash.
UCLASS()
class RUINMATES_API URuinmatesItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	FRuinmatesItemSlotClicked OnSlotClicked;

	void SetIndex(int32 InIndex) { SlotIndex = InIndex; }
	void ShowStack(const FRuinmatesItemStack& Stack, bool bUnknown);

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClicked();

	UPROPERTY()
	TObjectPtr<class UButton> ClickButton;

	UPROPERTY()
	TObjectPtr<class UBorder> QualityBorder;

	UPROPERTY()
	TObjectPtr<class UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<class UTextBlock> CountText;

	int32 SlotIndex = INDEX_NONE;
};
