#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "RuinmatesTerminalScreen.generated.h"

class UTextBlock;
class UProgressBar;
class UUniformGridPanel;
class UWidgetSwitcher;
class UVerticalBox;
class UButton;
class ARuinmatesCharacter;
class ARuinmatesLootContainer;

// Shared terminal. The left backpack stays put; the right switcher shows the character or a container.
UCLASS()
class RUINMATES_API URuinmatesTerminalScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer);
	void ShowPreview(bool bSearchPage);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildChrome();
	void RebuildSlots();
	void ApplyPageText();

	UFUNCTION()
	void ClosePreview();

	UFUNCTION()
	void SelectAll();

	UFUNCTION()
	void SelectConsumable();

	UFUNCTION()
	void SelectMaterial();

	UFUNCTION()
	void SelectQuest();

	UFUNCTION()
	void HandleSlotClicked(int32 StableIndex);

	UFUNCTION()
	void HandleLootClicked(int32 StableIndex);

	UFUNCTION()
	void HandleTakeAll();

	void SetFilter(FName InFilter);
	void BindSources();
	void UnbindSources();
	void RefreshFromLive();

	void Refresh();

	FRuinmatesTerminalPreviewPage Page;
	FName ActiveFilter = NAME_None;
	int32 SelectedStableIndex = INDEX_NONE;
	bool bLive = false;
	bool bClosing = false;

	TWeakObjectPtr<ARuinmatesCharacter> LiveCharacter;
	TWeakObjectPtr<ARuinmatesLootContainer> LiveContainer;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PackTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> CapacityText;

	UPROPERTY()
	TObjectPtr<UProgressBar> CapacityBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> RightSwitcher;

	UPROPERTY()
	TObjectPtr<UTextBlock> RightTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> RightSubtitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> LootTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LootCountText;

	UPROPERTY()
	TObjectPtr<UProgressBar> SearchBar;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> LootGrid;

	UPROPERTY()
	TObjectPtr<UButton> TakeAllButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterLeft;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterRight;

	UPROPERTY()
	TArray<TObjectPtr<class URuinmatesTerminalItemSlot>> BackpackSlots;

	UPROPERTY()
	TArray<TObjectPtr<class URuinmatesTerminalItemSlot>> LootSlots;
};
