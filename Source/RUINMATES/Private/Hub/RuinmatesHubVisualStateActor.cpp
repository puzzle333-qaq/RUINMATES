#include "Hub/RuinmatesHubVisualStateActor.h"

#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Sound/SoundBase.h"

namespace
{
	bool MatchesRequirement(ERuinmatesHubStateRequirement Requirement, bool bValue)
	{
		return Requirement == ERuinmatesHubStateRequirement::Any
			|| (Requirement == ERuinmatesHubStateRequirement::MustBeTrue ? bValue : !bValue);
	}
}

bool FRuinmatesHubVisualBinding::Matches(const FRuinmatesHubRoomState& State) const
{
	const FRuinmatesHubPermanentState& Permanent = State.Permanent;
	const bool bPermanentMatch = Permanent.Level >= MinimumLevel && Permanent.Level <= MaximumLevel
		&& MatchesRequirement(Cleared, Permanent.bCleared)
		&& MatchesRequirement(Unlocked, Permanent.bUnlocked)
		&& MatchesRequirement(StructureBlocked, Permanent.bStructureBlocked);
	return bPermanentMatch && MatchesRequirement(GeneratorInstalled, State.Runtime.bGeneratorInstalled)
		&& (Domain == ERuinmatesHubVisualDomain::Permanent
		|| ((!bRequiresPower || State.Runtime.bPowered) && (!bRequiresOperational || State.Runtime.bOperational)));
}

ARuinmatesHubVisualStateActor::ARuinmatesHubVisualStateActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("HubVisualRoot")));
}

void ARuinmatesHubVisualStateActor::BeginPlay()
{
	Super::BeginPlay();
	// Restore any editor preview before capturing a gameplay baseline.
	RestoreAuthoredState();
	RefreshBindings();
	EnsureWorldObservers();
	ARuinmatesHubStateActor* Source = StateSource;
	if (!IsValid(Source))
	{
		Source = FindStateSource();
	}
	BindStateSource(Source);
}

void ARuinmatesHubVisualStateActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = SpawnObserverWorld.Get())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
		World->RemoveOnActorDestroyedHandler(ActorDestroyedHandle);
	}
	ActorSpawnedHandle.Reset();
	ActorDestroyedHandle.Reset();
	SpawnObserverWorld.Reset();
	UnbindStateSource();
	Super::EndPlay(EndPlayReason);
}

void ARuinmatesHubVisualStateActor::EnsureWorldObservers()
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld() && !ActorSpawnedHandle.IsValid())
	{
		SpawnObserverWorld = World;
		ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateUObject(this, &ThisClass::HandleWorldActorSpawned));
		// Native notification executes before AActor::Destroyed/ProcessEvent. Dynamic
		// OnDestroyed UFUNCTION callbacks can be suppressed in an uninitialized world.
		ActorDestroyedHandle = World->AddOnActorDestroyedHandler(
			FOnActorDestroyed::FDelegate::CreateUObject(this, &ThisClass::HandleStateSourceDestroyed));
	}
}

void ARuinmatesHubVisualStateActor::HandleWorldActorSpawned(AActor* SpawnedActor)
{
	ARuinmatesHubStateActor* Source = Cast<ARuinmatesHubStateActor>(SpawnedActor);
	if (!BoundSource.IsValid() && IsValid(Source) && !Source->IsActorBeingDestroyed()
		&& Source->GetWorld() == GetWorld())
	{
		// The source may not have received its replicated snapshot or run BeginPlay yet.
		// Bind now; its initialization event/RepNotify will refresh the hidden groups later.
		BindStateSource(Source);
	}
}

ARuinmatesHubStateActor* ARuinmatesHubVisualStateActor::FindStateSource() const
{
	ARuinmatesHubStateActor* Found = nullptr;
	if (GetWorld())
	{
		for (TActorIterator<ARuinmatesHubStateActor> It(GetWorld()); It; ++It)
		{
			if (!IsValid(*It) || It->IsActorBeingDestroyed())
			{
				continue;
			}
			if (Found)
			{
				UE_LOG(LogTemp, Warning, TEXT("Hub visual %s found multiple state sources; assign one explicitly"), *GetName());
				return nullptr;
			}
			Found = *It;
		}
	}
	return Found;
}

void ARuinmatesHubVisualStateActor::HandleStateSourceDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor == StateSource.Get())
	{
		// Clear stale operating effects immediately, without a manual refresh or a Tick.
		BindStateSource(nullptr);
		if (ARuinmatesHubStateActor* Replacement = FindStateSource())
		{
			BindStateSource(Replacement);
		}
	}
}

void ARuinmatesHubVisualStateActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		RestoreAuthoredState();
		RefreshBindings();
		if (bPreviewInEditor)
		{
			FRuinmatesHubRoomState LocalPreview = PreviewState;
			LocalPreview.RoomId = RoomId;
			ApplySnapshot(&LocalPreview);
		}
	}
#endif
}

void ARuinmatesHubVisualStateActor::BindStateSource(ARuinmatesHubStateActor* NewSource)
{
	EnsureWorldObservers();
	UnbindStateSource();
	StateSource = IsValid(NewSource) ? NewSource : nullptr;
	if (IsValid(StateSource))
	{
		BoundSource = StateSource;
		StateChangedHandle = StateSource->OnRoomStateChanged.AddUObject(this, &ThisClass::HandleRoomStateChanged);
	}
	RefreshBindings();
	RefreshFromSource();
}

void ARuinmatesHubVisualStateActor::UnbindStateSource()
{
	// The native world notification runs before the source's UObject becomes invalid.
	if (ARuinmatesHubStateActor* Source = StateSource.Get())
	{
		Source->OnRoomStateChanged.Remove(StateChangedHandle);
	}
	StateChangedHandle.Reset();
	BoundSource.Reset();
}

void ARuinmatesHubVisualStateActor::HandleRoomStateChanged(FName ChangedRoomId)
{
	if (ChangedRoomId.IsNone() || ChangedRoomId == RoomId)
	{
		RefreshFromSource();
	}
}

void ARuinmatesHubVisualStateActor::RefreshFromSource()
{
	FRuinmatesHubRoomState State;
	const ARuinmatesHubStateActor* Source = BoundSource.Get();
	ApplySnapshot(IsValid(Source) && Source->GetRoomState(RoomId, State) ? &State : nullptr);
}

void ARuinmatesHubVisualStateActor::RefreshBindings()
{
	TSet<AActor*> SeenActors;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		for (AActor* Target : Bindings[Index].Actors)
		{
			if (!IsValid(Target) || Target == this || Target->IsA<ARuinmatesHubStateActor>())
			{
				continue;
			}
			if (SeenActors.Contains(Target))
			{
				UE_LOG(LogTemp, Warning, TEXT("Hub visual %s ignores duplicate binding for %s"), *GetName(), *Target->GetName());
				continue;
			}
			SeenActors.Add(Target);
			FActorBaseline* EntryPtr = Baselines.FindByPredicate(
				[Target](const FActorBaseline& Entry) { return Entry.Actor.Get() == Target; });
			if (!EntryPtr)
			{
				EntryPtr = &Baselines.AddDefaulted_GetRef();
				EntryPtr->Actor = Target;
				EntryPtr->bHidden = Target->IsHidden();
#if WITH_EDITOR
				EntryPtr->bEditorHidden = Target->IsTemporarilyHiddenInEditor();
#endif
				EntryPtr->bCollision = Target->GetActorEnableCollision();
				EntryPtr->bTickEnabled = Target->IsActorTickEnabled();
			}
			FActorBaseline& Entry = *EntryPtr;
			Entry.BindingIndex = Index;
			Entry.bApplied = false;
			Entry.Components.RemoveAll([](const FComponentBaseline& Saved) { return !Saved.Component.IsValid(); });
			TInlineComponentArray<UActorComponent*> Components(Target);
			for (UActorComponent* Component : Components)
			{
				if (!IsValid(Component) || Entry.Components.ContainsByPredicate(
					[Component](const FComponentBaseline& Saved) { return Saved.Component.Get() == Component; }))
				{
					continue;
				}
				FComponentBaseline& Saved = Entry.Components.AddDefaulted_GetRef();
				Saved.Component = Component;
				Saved.bTickEnabled = Component->IsComponentTickEnabled();
				Saved.bActive = Component->IsActive();
				Saved.bAutoActivate = Component->bAutoActivate;
				if (const USceneComponent* Scene = Cast<USceneComponent>(Component))
				{
					Saved.bVisible = Scene->GetVisibleFlag();
					Saved.bHiddenInGame = Scene->bHiddenInGame;
				}
				if (const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
				{
					// The actor may already be hidden/disabled when a new primitive arrives.
					// Read its authored mode without the owner's temporary collision override.
					Saved.Collision = Primitive->BodyInstance.GetCollisionEnabled(false);
				}
				if (const UAudioComponent* Audio = Cast<UAudioComponent>(Component))
				{
					Saved.bAudioPlaying = Audio->IsPlaying();
					Saved.bAudioPaused = Audio->bIsPaused;
				}
			}
		}
	}
	for (int32 Index = Baselines.Num() - 1; Index >= 0; --Index)
	{
		if (!Baselines[Index].Actor.IsValid() || !SeenActors.Contains(Baselines[Index].Actor.Get()))
		{
			FRuinmatesHubVisualBinding Restore;
			Baselines[Index].bApplied = false;
			ApplyActor(Baselines[Index], Restore, true);
			Baselines.RemoveAt(Index);
		}
	}
	if (GetWorld() && GetWorld()->IsGameWorld())
	{
		// Explicit refresh covers components added after the first actor baseline was captured.
		RefreshFromSource();
	}
}

void ARuinmatesHubVisualStateActor::ApplySnapshot(const FRuinmatesHubRoomState* State)
{
	TArray<bool> Gates;
	Gates.Reserve(Bindings.Num());
	for (const FRuinmatesHubVisualBinding& Binding : Bindings)
	{
		Gates.Add(State && State->RoomId == RoomId && Binding.Matches(*State));
	}
	for (FActorBaseline& Entry : Baselines)
	{
		if (Bindings.IsValidIndex(Entry.BindingIndex) && Gates.IsValidIndex(Entry.BindingIndex))
		{
			ApplyActor(Entry, Bindings[Entry.BindingIndex], Gates[Entry.BindingIndex]);
		}
	}
}

void ARuinmatesHubVisualStateActor::ApplyActor(FActorBaseline& Baseline,
	const FRuinmatesHubVisualBinding& Binding, bool bEnabled)
{
	AActor* Target = Baseline.Actor.Get();
	if (!IsValid(Target) || (Baseline.bApplied && Baseline.bLastEnabled == bEnabled))
	{
		return;
	}
	const bool bGroupHidden = !bEnabled && Binding.bHideWhenInactive;
	const bool bCollisionOff = !bEnabled && Binding.bDisableCollisionWhenInactive;
	const bool bTicksOff = !bEnabled && Binding.bSuspendTickWhenInactive;
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		// Temporary editor visibility is not serialized into the authored actor/component state.
		// Preview power-only lamps as their own bindings; equipment bodies may stay visible.
		Target->SetIsTemporarilyHiddenInEditor(Baseline.bEditorHidden || bGroupHidden);
		Baseline.bApplied = true;
		Baseline.bLastEnabled = bEnabled;
		return;
	}
#endif
	Target->SetActorHiddenInGame(Baseline.bHidden || bGroupHidden);
	Target->SetActorEnableCollision(Baseline.bCollision && !bCollisionOff);
	Target->SetActorTickEnabled(Baseline.bTickEnabled && !bTicksOff);
	for (const FComponentBaseline& Saved : Baseline.Components)
	{
		UActorComponent* Component = Saved.Component.Get();
		if (!IsValid(Component))
		{
			continue;
		}
		// Editor preview changes geometry/lights only and never starts sounds or effects.
		const bool bGameWorld = GetWorld() && GetWorld()->IsGameWorld();
		if (bGameWorld)
		{
			// SetAutoActivate is construction-only and logs warnings after registration.
			Component->bAutoActivate = bEnabled && Saved.bAutoActivate;
		}
		if (UAudioComponent* Audio = Cast<UAudioComponent>(Component))
		{
			if (bGameWorld)
			{
				if (!bEnabled)
				{
					Audio->Stop();
				}
				else if (Saved.bAudioPlaying && IsValid(Audio->Sound) && !Audio->IsPlaying())
				{
					Audio->Play();
					Audio->SetPaused(Saved.bAudioPaused);
				}
			}
		}
		else if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
		{
			if (bGameWorld)
			{
				if (!bEnabled)
				{
					Niagara->DeactivateImmediate();
				}
				else if (Saved.bActive && IsValid(Niagara->GetAsset()) && !Niagara->IsActive())
				{
					Niagara->Activate(false);
				}
			}
		}
		else if (bGameWorld && Component->IsActive() != (bEnabled && Saved.bActive))
		{
			Component->SetActive(bEnabled && Saved.bActive, false);
		}
		if (USceneComponent* Scene = Cast<USceneComponent>(Component))
		{
			const bool bLightOff = !bEnabled && Component->IsA<ULightComponent>();
			Scene->SetVisibility(Saved.bVisible && !bGroupHidden && !bLightOff, false);
			Scene->SetHiddenInGame(Saved.bHiddenInGame || bGroupHidden, false);
		}
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Primitive->SetCollisionEnabled(bCollisionOff ? ECollisionEnabled::NoCollision : Saved.Collision);
		}
		Component->SetComponentTickEnabled(Saved.bTickEnabled && !bTicksOff);
	}
	Baseline.bApplied = true;
	Baseline.bLastEnabled = bEnabled;
}

void ARuinmatesHubVisualStateActor::RestoreAuthoredState()
{
	FRuinmatesHubVisualBinding Restore;
	for (FActorBaseline& Entry : Baselines)
	{
		if (Entry.bApplied)
		{
			Entry.bApplied = false;
			ApplyActor(Entry, Restore, true);
			Entry.bApplied = false;
		}
	}
}
