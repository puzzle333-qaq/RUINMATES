#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "Inventory/RuinmatesItemTypes.h"
#include "RuinmatesLootContainer.generated.h"

class APawn;
class UBoxComponent;
class UTextRenderComponent;

DECLARE_MULTICAST_DELEGATE(FRuinmatesContainerChanged);

// Shared search container. Progress is server-owned so several players see the same reveal.
UCLASS(Blueprintable)
class RUINMATES_API ARuinmatesLootContainer : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ARuinmatesLootContainer();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	FRuinmatesContainerChanged OnChanged;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Loot")
	FText GetContainerName() const { return ContainerName; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Loot")
	float GetCloseDistance() const { return CloseDistance; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Loot")
	const TArray<FRuinmatesItemStack>& GetContents() const { return Contents; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Loot")
	int32 GetRevealedCount() const;

	void StopSearch(APawn* Pawn);

	/** Server only. Moves revealed items into the pawn backpack. Returns how many stacks were touched. */
	int32 GiveTo(APawn* Pawn, int32 SlotIndex);
	int32 GiveAllRevealed(APawn* Pawn);

protected:
	UPROPERTY(VisibleAnywhere, Category = "RuinMates|Loot")
	TObjectPtr<UBoxComponent> SearchBounds;

	UPROPERTY(VisibleAnywhere, Category = "RuinMates|Loot")
	TObjectPtr<UTextRenderComponent> Label;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Loot")
	FText ContainerName;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Loot", meta = (ClampMin = "50"))
	float CloseDistance = 320.f;

	UPROPERTY(EditAnywhere, Category = "RuinMates|Loot", meta = (ClampMin = "0.1"))
	float RevealInterval = 0.7f;

	// Optional authored loot. Empty containers get a small default table so a placed box is searchable.
	UPROPERTY(EditAnywhere, Category = "RuinMates|Loot")
	TArray<FRuinmatesItemStack> AuthoredLoot;

	UPROPERTY(ReplicatedUsing = OnRep_Contents)
	TArray<FRuinmatesItemStack> Contents;

	UFUNCTION()
	void OnRep_Contents();

	void EnsureDefaultLoot();
	void StartRevealTimer();
	void RevealNext();

	UPROPERTY()
	TArray<TObjectPtr<APawn>> Searchers;

	FTimerHandle RevealTimer;
};
