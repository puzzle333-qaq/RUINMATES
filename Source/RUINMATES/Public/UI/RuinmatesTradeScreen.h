#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesTradeScreen.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;
class ARuinmatesCharacter;
class ARuinmatesMerchant;

DECLARE_MULTICAST_DELEGATE_OneParam(FRuinmatesTradeRowSelected, FName);

// Small helper so a list of buttons can report which row was clicked through a dynamic delegate.
UCLASS()
class RUINMATES_API URuinmatesTradeRowProxy : public UObject
{
	GENERATED_BODY()

public:
	FRuinmatesTradeRowSelected OnRowSelected;

	FName Tag;

	UFUNCTION()
	void HandleClicked()
	{
		OnRowSelected.Broadcast(Tag);
	}
};

// Minimal trading screen: merchant stock / backpack on the left, detail + quantity on the right.
// Functional pass only - it reuses the terminal palette but does not add new art.
UCLASS()
class RUINMATES_API URuinmatesTradeScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	// Called by the owning character after the trade screen is in the viewport.
	void SetContext(ARuinmatesCharacter* InCharacter, ARuinmatesMerchant* InMerchant);

	// Server reply, shown as a one line status under the detail panel.
	void ShowResult(bool bSuccess, const FText& Message);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleClose();

	UFUNCTION()
	void HandleBuyMode();

	UFUNCTION()
	void HandleSellMode();

	UFUNCTION()
	void HandleQuantityDown();

	UFUNCTION()
	void HandleQuantityUp();

	UFUNCTION()
	void HandleConfirm();

	UFUNCTION()
	void HandleSelectAll();

	void BuildLayout();
	void RebuildList();
	void RefreshDetail();
	void RefreshFunds();

	// Receives the tag stored on a row button ("Bandage" in buy mode, "slot:3" in sell mode).
	void HandleRowSelected(FName Tag);

	void CloseTrade();

	UPROPERTY()
	TObjectPtr<UTextBlock> MerchantNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FundsText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ModeText;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> EntryList;

	UPROPERTY()
	TObjectPtr<UVerticalBox> DetailBox;

	UPROPERTY()
	TObjectPtr<UButton> BuyModeButton;

	UPROPERTY()
	TObjectPtr<UButton> SellModeButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DetailInfoText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY()
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> ConfirmLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> OwnedText;

	UPROPERTY()
	TArray<TObjectPtr<URuinmatesTradeRowProxy>> RowProxies;

	TWeakObjectPtr<ARuinmatesCharacter> LiveCharacter;
	TWeakObjectPtr<ARuinmatesMerchant> LiveMerchant;

	// Buy mode lists merchant ItemIds; sell mode lists backpack slot indices.
	bool bSellMode = false;
	FName SelectedItemId;
	int32 SelectedSlotIndex = INDEX_NONE;
	int32 Quantity = 1;
};
