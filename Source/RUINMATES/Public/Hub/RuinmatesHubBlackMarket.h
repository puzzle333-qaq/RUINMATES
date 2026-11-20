#pragma once

#include "CoreMinimal.h"
#include "Inventory/RuinmatesMerchant.h"
#include "RuinmatesHubBlackMarket.generated.h"

class ARuinmatesHubStateActor;
class ARuinmatesCharacter;
class UBoxComponent;
class ULightComponent;
class UAudioComponent;

/** Unattended parcel trading post. One short-lived server session owns the shutter and existing market. */
UCLASS()
class RUINMATES_API ARuinmatesHubBlackMarket : public ARuinmatesMerchant
{
	GENERATED_BODY()
public:
	ARuinmatesHubBlackMarket();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual bool CanInteract_Implementation(APawn* Pawn) const override;
	virtual FText GetInteractPrompt_Implementation(APawn* Pawn) const override;
	virtual void Interact_Implementation(APawn* Pawn) override;
	/** Also called by existing buy/sell validation, so a stale client UI cannot trade after outage. */
	bool IsTradeAllowed(const ARuinmatesCharacter* Character) const;
	/** Authority close used for explicit trade end, death, departure and power loss. */
	void CloseSession();
	/** Read-only temporary shutter status for UI and development acceptance. */
	bool IsWindowClosed() const { return SessionPhase==0; }
	/** Single replicated construction source; no economic state is stored in this actor. */
	UPROPERTY(EditInstanceOnly, Category="Hub") TObjectPtr<ARuinmatesHubStateActor> Hub;
	/** Existing movable leaf actors, configured in the same map; no generated duplicate geometry. */
	UPROPERTY(EditInstanceOnly, Category="Hub") TArray<TObjectPtr<AActor>> ShutterLeaves;
	/** Session light is separate from permanent building lighting. */
	UPROPERTY(EditInstanceOnly, Category="Hub") TObjectPtr<AActor> WindowLight;
	/** Local vertical clearance used by each shutter leaf. */
	UPROPERTY(EditAnywhere, Category="Hub") float LiftHeight = 240.f;
	/** Finite opening/closing duration; interaction is blocked while either animation runs. */
	UPROPERTY(EditAnywhere, Category="Hub") float MotionDuration = 0.65f;
protected:
	/** Hidden query volume remains reachable for the first opening interaction. */
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionBox;
	/** Optional authored shutter motion sound, never a hard-coded external asset. */
	UPROPERTY(VisibleAnywhere, Category="Hub|Audio") TObjectPtr<UAudioComponent> MotionAudio;
	/** Owner is replicated together with phase so late joiners apply one coherent session state. */
	UPROPERTY(ReplicatedUsing=OnRep_Session) TObjectPtr<ARuinmatesCharacter> SessionOwner;
	/** 0 closed, 1 opening, 2 open, 3 closing. Temporary only; never saved. */
	UPROPERTY(ReplicatedUsing=OnRep_Session) uint8 SessionPhase = 0;
	UFUNCTION() void OnRep_Session();
private:
	FTimerHandle SessionTimer, MotionTimer;
	FDelegateHandle HubHandle;
	TArray<FVector> ClosedLocations;
	float MotionStart = 0.f, CurrentAlpha = 0.f, StartAlpha = 0.f;
	bool bTradeStarted = false;
	/** Low frequency authority checks close abandoned windows even before the second interaction. */
	void CheckSession();
	/** Motion timer exists only during travel; permanent room construction never ticks here. */
	void AnimateShutter();
	void ApplyShutter(float Alpha);
	bool HasAccess(APawn* Pawn) const;
};
