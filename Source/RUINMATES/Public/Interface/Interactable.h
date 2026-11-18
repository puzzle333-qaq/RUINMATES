#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class APawn;

// Generic interact interface (e.g. press a use key).
// Callers should invoke Interact only on the server. CanInteract / GetInteractPrompt
// may be queried locally for prompts and also on the server as a permission check.
UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class RUINMATES_API IInteractable
{
	GENERATED_BODY()

public:
	// Whether this pawn can interact now (queried locally and on the server).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RuinMates|Interact")
	bool CanInteract(APawn* InteractingPawn) const;

	// HUD prompt, e.g. "Press E to open".
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RuinMates|Interact")
	FText GetInteractPrompt(APawn* InteractingPawn) const;

	// Perform the interaction. Call on the server only.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "RuinMates|Interact")
	void Interact(APawn* InteractingPawn);
};
