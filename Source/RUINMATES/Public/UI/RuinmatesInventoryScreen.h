#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesInventoryScreen.generated.h"

class ARuinmatesCharacter;
class ARuinmatesLootContainer;
class URuinmatesItemSlotWidget;
class UTextBlock;
class UWidgetSwitcher;
class UUniformGridPanel;
class UProgressBar;

// Shared backpack and search screen. The right side is the only panel that changes.
// WBP_InventoryScreen / Header / Equipment / Backpack / ItemDetail / LootContainer / Footer.
UCLASS()
class RUINMATES_API URuinmatesInventoryScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	void BuildLayout();
	void BindSources();
	void UnbindSources();
	void Refresh();

	UFUNCTION()
	void HandleTakeAll();

	UFUNCTION()
	void HandleDiscard();

	UFUNCTION()
	void HandleBackpackSlotClicked(int32 SlotIndex);

	UFUNCTION()
	void HandleLootSlotClicked(int32 SlotIndex);

	UPROPERTY()
	TObjectPtr<ARuinmatesCharacter> Character;

	UPROPERTY()
	TObjectPtr<ARuinmatesLootContainer> Container;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> MoneyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TimeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PackNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CapacityText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UProgressBar> CapacityBar;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> RightSwitcher;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailName;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailBody;

	UPROPERTY()
	TObjectPtr<UTextBlock> SearchTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> SearchProgress;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> LootGrid;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterText;

	int32 SelectedBackpackSlot = INDEX_NONE;
};
