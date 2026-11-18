#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "TestInteractActor.generated.h"

// Minimal interactable actor for compile / editor smoke tests. No assets required.
UCLASS()
class RUINMATES_API ATestInteractActor : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ATestInteractActor();

	virtual bool CanInteract_Implementation(APawn* InteractingPawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* InteractingPawn) const override;
	virtual void Interact_Implementation(APawn* InteractingPawn) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Interact")
	int32 GetInteractCount() const { return InteractCount; }

	int32 GetCanInteractImplCalls() const { return CanInteractImplCalls; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Interact")
	TObjectPtr<USceneComponent> SceneRoot;

	// How many times Interact was accepted on the server.
	UPROPERTY(VisibleAnywhere, Replicated, BlueprintReadOnly, Category = "RuinMates|Interact")
	int32 InteractCount = 0;

	UPROPERTY()
	int32 CanInteractImplCalls = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
