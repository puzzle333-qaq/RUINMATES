#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "Hub/RuinmatesHubTypes.h"
#include "RuinmatesHubVisualStateActor.generated.h"

class ARuinmatesHubStateActor;
class UActorComponent;

/** Declarative group of already placed actors; applying state never spawns decorations. */
USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesHubVisualBinding
{
	GENERATED_BODY()

	/** Map actors controlled by this group. Assign each actor to only one binding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	TArray<TObjectPtr<AActor>> Actors;

	/** Permanent groups ignore power; runtime groups additionally evaluate supply/service gates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	ERuinmatesHubVisualDomain Domain = ERuinmatesHubVisualDomain::Permanent;

	/** Inclusive level interval in which the group exists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (ClampMin = "0", ClampMax = "2"))
	int32 MinimumLevel = 0;

	/** Inclusive upper level; Core currently stops at 2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub", meta = (ClampMin = "0", ClampMax = "2"))
	int32 MaximumLevel = 2;

	/** Optional permanent clearing condition, independent of current electricity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	ERuinmatesHubStateRequirement Cleared = ERuinmatesHubStateRequirement::Any;

	/** Optional permanent unlock condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	ERuinmatesHubStateRequirement Unlocked = ERuinmatesHubStateRequirement::Any;

	/** Optional permanent entrance obstruction condition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	ERuinmatesHubStateRequirement StructureBlocked = ERuinmatesHubStateRequirement::Any;

	/** Evaluated only for Runtime groups; never removes repaired structures on a power cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bRequiresPower = true;

	/** Optional Runtime service gate, for equipment that also needs permanent room access. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bRequiresOperational = false;

	/** Hardware gate applies independently of level/power for installed generator visuals. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	ERuinmatesHubStateRequirement GeneratorInstalled = ERuinmatesHubStateRequirement::Any;

	/** Hide inactive actors. Disable for equipment bodies that should remain visible without power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bHideWhenInactive = true;

	/** Disable inactive actors' collision. Disable for visible, stationary unpowered equipment bodies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bDisableCollisionWhenInactive = true;

	/** Stops actor/component ticks while this authored group is inactive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hub")
	bool bSuspendTickWhenInactive = true;

	/** Pure gate evaluation shared by preview, runtime and automation. */
	bool Matches(const FRuinmatesHubRoomState& State) const;
};

/**
 * Event-driven, room-local visual adapter. It owns no resources, construction outcomes or saves.
 * Hidden groups stop collision, ticks, lights, audio and Niagara; enabling restores authored values.
 */
UCLASS()
class RUINMATES_API ARuinmatesHubVisualStateActor : public AActor
{
	GENERATED_BODY()

public:
	/** Creates a non-ticking visual adapter. Static bindings exist on every map instance. */
	ARuinmatesHubVisualStateActor();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Stable facility ID; never derived from this actor's name or label. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RuinMates|Hub")
	FName RoomId = NAME_None;

	/** Explicit source preferred; BeginPlay can discover the sole source in the same world. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RuinMates|Hub")
	TObjectPtr<ARuinmatesHubStateActor> StateSource;

	/** Declarative environment groups. Permanent floors/walls need not be bound. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "RuinMates|Hub")
	TArray<FRuinmatesHubVisualBinding> Bindings;

	/** Editor-only visual preview; does not commit authority state or write campaign progress. */
	UPROPERTY(EditInstanceOnly, Category = "RuinMates|Hub|Preview")
	bool bPreviewInEditor = false;

	/** Local preview state, ignored by gameplay and replication. RoomId comes from this adapter. */
	UPROPERTY(EditInstanceOnly, Category = "RuinMates|Hub|Preview", meta = (EditCondition = "bPreviewInEditor"))
	FRuinmatesHubRoomState PreviewState;

	/** Binds a late-created state source and immediately applies its current snapshot. */
	void BindStateSource(ARuinmatesHubStateActor* NewSource);

	/** Re-queries the source; uninitialized/missing state suppresses only the explicitly bound groups. */
	void RefreshFromSource();

	/** Captures new bindings before applying. Does not overwrite cached authored baselines. */
	void RefreshBindings();

	/** Restores cached authored values; editor previews never become the next baseline. */
	void RestoreAuthoredState();

private:
	/** Per-component authored state; weak references safely tolerate deleted decoration actors. */
	struct FComponentBaseline
	{
		TWeakObjectPtr<UActorComponent> Component;
		bool bTickEnabled = false;
		bool bActive = false;
		bool bAutoActivate = false;
		bool bVisible = false;
		bool bHiddenInGame = false;
		bool bAudioPlaying = false;
		bool bAudioPaused = false;
		ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
	};

	/** One baseline and last-applied gate per unique bound actor. */
	struct FActorBaseline
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<FComponentBaseline> Components;
		int32 BindingIndex = INDEX_NONE;
		bool bHidden = false;
		bool bEditorHidden = false;
		bool bCollision = false;
		bool bTickEnabled = false;
		bool bApplied = false;
		bool bLastEnabled = false;
	};

	/** Captured once before a group is changed; no UObject is held outside GC tracking. */
	TArray<FActorBaseline> Baselines;

	/** Subscription belongs to one source only, including editor reconfiguration/late registration. */
	TWeakObjectPtr<ARuinmatesHubStateActor> BoundSource;
	FDelegateHandle StateChangedHandle;

	/** Event-based discovery also handles state actors that arrive after client BeginPlay. */
	TWeakObjectPtr<UWorld> SpawnObserverWorld;
	FDelegateHandle ActorSpawnedHandle;
	FDelegateHandle ActorDestroyedHandle;

	/** Installs native world lifecycle notifications once, without relying on script initialization. */
	void EnsureWorldObservers();

	/** Binds a newly spawned source before its later initial snapshot/RepNotify is applied. */
	void HandleWorldActorSpawned(AActor* SpawnedActor);

	/** Suppresses stale visuals immediately when the source is destroyed and seeks a replacement. */
	void HandleStateSourceDestroyed(AActor* DestroyedActor);

	/** Returns the sole live state actor, excluding destruction/teardown candidates. */
	ARuinmatesHubStateActor* FindStateSource() const;

	/** Responds only to this room or a complete replicated snapshot. */
	void HandleRoomStateChanged(FName ChangedRoomId);

	/** Computes all group gates before changing any actor. */
	void ApplySnapshot(const FRuinmatesHubRoomState* State);

	/** Applies one gate transition; identical repeats never restart sound or Niagara. */
	void ApplyActor(FActorBaseline& Baseline, const FRuinmatesHubVisualBinding& Binding, bool bEnabled);

	/** Unsubscribes without retaining an expired state source. */
	void UnbindStateSource();
};
