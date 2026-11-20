#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Interface/Interactable.h"
#include "RuinmatesHubInteraction.generated.h"

class ARuinmatesHubStateActor;
class ARuinmatesHubTerminal;
class URuinmatesHubScreen;
class UBoxComponent;
class UStaticMeshComponent;

/** One existing F-key interaction entry can serve construction, fuel or facility use. */
UENUM(BlueprintType)
enum class ERuinmatesHubTerminalType : uint8 { Construction, Generator, Medical, Workshop, Elevator, Exit };

/** Client submits intent only; authoritative quantities and results remain in the Hub. */
UENUM()
enum class ERuinmatesHubCommand : uint8 { Construct, StarterKit, Install, Fuel, Start, Stop, Deposit, Withdraw, Save };

/** Owned character component routes Hub RPCs through the existing pawn ownership channel. */
UCLASS(ClassGroup=(RuinMates))
class RUINMATES_API URuinmatesHubInteractionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URuinmatesHubInteractionComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	/** Server invokes a client window on this component's owning character. */
	UFUNCTION(Client, Reliable)
	void Client_OpenHub(ARuinmatesHubTerminal* Terminal);
	/** Owning client sends an action identifier and expected state revision, never a result. */
	UFUNCTION(Server, Reliable)
	void Server_RequestHub(ARuinmatesHubTerminal* Terminal, ERuinmatesHubCommand Command,
		FName ActionOrItem, int32 Count, int32 ExpectedRevision);
	/** Server result is rendered and controls whether a duplicate button request may proceed. */
	UFUNCTION(Client, Reliable)
	void Client_HubResult(bool bSuccess, const FText& Reason);
	/** Restores game input and closes the only Hub widget owned by this character. */
	void CloseHub();
	/** Native UI access to the owning character, with no global player search. */
	APawn* GetOwningPawn() const;
private:
	/** Retained widget participates in GC and is reused for this interaction session. */
	UPROPERTY()
	TObjectPtr<URuinmatesHubScreen> Screen;
	/** Current UI target, checked periodically for death, destruction or leaving the service range. */
	UPROPERTY() TObjectPtr<ARuinmatesHubTerminal> ActiveTerminal;
	FTimerHandle AccessTimer;
	double LastRequestTime = -1.0;
	void CheckLocalAccess();
};

/** Map-local terminal; no resource balances or upgrade outcomes are stored here. */
UCLASS()
class RUINMATES_API ARuinmatesHubTerminal : public AActor, public IInteractable
{
	GENERATED_BODY()
public:
	ARuinmatesHubTerminal();
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	/** Stable source of all room/resource facts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hub")
	TObjectPtr<ARuinmatesHubStateActor> Hub;
	/** Service category controls interaction text and service validation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hub")
	ERuinmatesHubTerminalType Type = ERuinmatesHubTerminalType::Construction;
	/** Configurable service range; server additionally reconstructs a visibility trace. */
	UPROPERTY(EditAnywhere, Category="Hub")
	float InteractionDistance = 300.f;
	/** Shared server/client availability check; authority is checked again before mutation. */
	bool ValidateAccess(APawn* Pawn) const;
private:
	/** Query-only F-key target; decorative meshes do not control authority. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> InteractionBox;
};
