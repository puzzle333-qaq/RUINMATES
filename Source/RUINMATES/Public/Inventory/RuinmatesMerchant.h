#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "RuinmatesMerchant.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

// Simple trading post. First pass: no AI, no dialog, unlimited stock.
// MerchantItems only stores ItemIds; names, quality and prices come from the item database.
UCLASS()
class RUINMATES_API ARuinmatesMerchant : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARuinmatesMerchant();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Merchant")
	FText GetMerchantName() const { return MerchantName; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Merchant")
	const TArray<FName>& GetMerchantItems() const { return MerchantItems; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Merchant")
	bool AllowsSelling() const { return bAllowsSelling; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Merchant")
	bool SellsItem(FName ItemId) const { return MerchantItems.Contains(ItemId); }

	/** Editor/tooling helper: replaces the stock list. Prices still come from DT_Items. */
	void SetMerchantItems(const TArray<FName>& InItems) { MerchantItems = InItems; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Merchant")
	float GetInteractionDistance() const { return InteractionDistance; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "RuinMates|Merchant")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "RuinMates|Merchant")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Merchant")
	FText MerchantName;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Merchant", meta = (ClampMin = "50"))
	float InteractionDistance = 320.f;

	// ItemIds resolved through the item database. Never duplicate names or prices here.
	UPROPERTY(EditAnywhere, Category = "RuinMates|Merchant")
	TArray<FName> MerchantItems;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Merchant")
	bool bAllowsSelling = true;
};
