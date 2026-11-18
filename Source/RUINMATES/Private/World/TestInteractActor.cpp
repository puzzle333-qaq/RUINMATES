#include "World/TestInteractActor.h"
#include "Net/UnrealNetwork.h"

ATestInteractActor::ATestInteractActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ATestInteractActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATestInteractActor, InteractCount);
}

bool ATestInteractActor::CanInteract_Implementation(APawn* InteractingPawn) const
{
	++const_cast<ATestInteractActor*>(this)->CanInteractImplCalls;
	return IsValid(InteractingPawn);
}

FText ATestInteractActor::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}
	return NSLOCTEXT("RuinMates", "TestInteractPrompt", "Interact");
}

void ATestInteractActor::Interact_Implementation(APawn* InteractingPawn)
{
	// Server permission boundary: ignore client-side calls.
	if (!HasAuthority())
	{
		return;
	}
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return;
	}
	++InteractCount;
}
