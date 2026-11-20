#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Hub/RuinmatesHubTypes.h"
#include "RuinmatesHubStateActor.generated.h"

class URuinmatesHubDefinition;
class URuinmatesBackpackComponent;
class APawn;
class UAudioComponent;

/** RoomId is None when an entire initial/replicated snapshot should be refreshed. */
DECLARE_MULTICAST_DELEGATE_OneParam(FRuinmatesHubStateChanged, FName);

/**
 * Single replicated Hub state source. Phase 5 extends this actor with construction transactions.
 * Environment actors never consume resources or own another copy of campaign progress.
 */
UCLASS()
class RUINMATES_API ARuinmatesHubStateActor : public AActor
{
	GENERATED_BODY()

public:
	/** Creates a non-ticking, always relevant state source. */
	ARuinmatesHubStateActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Event consumed by room visuals and, later, the construction UI. */
	FRuinmatesHubStateChanged OnRoomStateChanged;

	/** True only after an authoritative initial/loaded snapshot is available. */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Hub")
	bool IsHubInitialized() const { return Snapshot.bInitialized; }

	/** Returns an initialized room snapshot; unknown IDs fail without inventing rooms. */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Hub")
	bool GetRoomState(FName RoomId, FRuinmatesHubRoomState& OutState) const;

	/** Current scope cap: Core 2, other first-floor facilities 1; unknown IDs return -1. */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Hub")
	static int32 GetRoomMaxLevel(FName RoomId);

	/**
	 * Authority-only bootstrap, called by BeginPlay unless a future save restore ran first.
	 * This is not a player construction endpoint and does not grant resources or write a save.
	 */
	bool InitializeDefaultState();

	/**
	 * Internal authority commit for an already validated manager transaction. No client RPC or
	 * Blueprint upgrade bypass is exposed. Rejects unknown IDs, invalid levels and impossible
	 * operational states; an identical commit emits no duplicate change event.
	 */
	bool CommitRoomState(const FRuinmatesHubRoomState& NewState);

	/** Single editable construction, generator and resource-source configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RuinMates|Hub")
	TObjectPtr<URuinmatesHubDefinition> Definition;

	/** Shared storage uses the same canonical item stacks and inventory primitives as players. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RuinMates|Hub")
	TObjectPtr<URuinmatesBackpackComponent> Warehouse;

	/** Optional editor-configured generator loop; empty sound assets safely produce silence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hub|Audio")
	TObjectPtr<UAudioComponent> GeneratorAudio;
	/** Optional restored-core loop, activated only when repaired and actually powered. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Hub|Audio")
	TObjectPtr<UAudioComponent> CoreAudio;

	/** Read-only snapshot for UI and persistence; environment adapters use GetRoomState. */
	const FRuinmatesHubSnapshot& GetSnapshot() const { return Snapshot; }

	/** Builds a current quote using only authoritative configuration and eligible inventories. */
	FRuinmatesHubActionQuote GetActionQuote(APawn* Initiator, FName ActionId) const;

	/** Executes one intent exactly once for the expected revision; no client result is accepted. */
	bool TryExecuteAction(APawn* Initiator, FName ActionId, int32 ExpectedRevision, FText& OutReason);

	/** Installs the configured existing item using the same atomic resource transaction. */
	bool TryInstallGenerator(APawn* Initiator, int32 ExpectedRevision, FText& OutReason);

	/** Adds one whole fuel item, rejecting insufficient capacity instead of wasting fuel. */
	bool TryAddFuel(APawn* Initiator, int32 ExpectedRevision, FText& OutReason);

	/** Starts/stops installed hardware on the authority; does not alter any repaired structure. */
	bool TrySetGeneratorStarted(APawn* Initiator, bool bStarted, int32 ExpectedRevision, FText& OutReason);

	/** Transfers exact quantities between shared storage and the initiating pawn's inventory. */
	bool TryTransferWarehouse(APawn* Initiator, FName ItemId, int32 Count, bool bDeposit,
		int32 ExpectedRevision, FText& OutReason);

	/** One-time server grant from configured starter kit; persisted together with warehouse. */
	bool TryClaimStarterKit(APawn* Initiator, FText& OutReason);

	/** True only for unlocked, structurally accessible, operational facilities. */
	bool IsFacilityOperational(FName RoomId) const;

	/** Saves current permanent state, fuel, warehouse and party as one validated campaign generation. */
	bool SaveProgress();

#if WITH_EDITOR
	/** Editor-only transient comparison helper; never consumes resources or writes campaign data. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "RuinMates|Hub|Development")
	void PreviewAllRoomsForCapture(int32 CoreLevel, bool bPowered);
#endif

protected:
	/** RepNotify only applies received facts; clients never decide construction outcomes. */
	UFUNCTION()
	void OnRep_Snapshot();

	/** Room state and initialization gate travel as one replicated property. */
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FRuinmatesHubSnapshot Snapshot;

	/** Prevents notification callbacks from entering another construction transaction. */
	bool bTransactionInProgress = false;
	/** Editor comparison sessions must never autosave preview state into a real campaign. */
	bool bPreviewSession = false;
	FTimerHandle SimulationTimer;
	float LastSimulationTime = 0.f;
	float LastAutosaveTime = 0.f;

	/** Shared candidate commit used by actions and generator operations. */
	bool CommitWithResources(APawn* Initiator, const TMap<FName, int32>& Costs,
		const FRuinmatesHubSnapshot& Candidate, FText& OutReason);
	/** Recomputes runtime only; all permanent room facts are preserved. */
	void RecomputeOperatingState();
	/** Applies optional authored loops on authority and received client snapshots. */
	void RefreshOperatingAudio();
	/** The only fuel simulation timer in the Hub world. */
	void SimulatePower();
	/** Eligible inventories always belong to this Hub and this requesting pawn. */
	URuinmatesBackpackComponent* GetInitiatorBackpack(APawn* Initiator) const;
	bool IsRequestAllowed(APawn* Initiator, int32 ExpectedRevision, FText& OutReason) const;
	/** Phase8 connects this hook to existing campaign persistence before publication. */
	bool PersistCandidate();
};
