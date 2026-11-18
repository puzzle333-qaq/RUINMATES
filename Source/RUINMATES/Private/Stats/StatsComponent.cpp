#include "Stats/StatsComponent.h"
#include "Net/UnrealNetwork.h"

UStatsComponent::UStatsComponent()
{
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.bCanEverTick = true;
	DeathState = EDeathState::NotDead;
	SetIsReplicatedByDefault(true);

	Health = 100.f;
	MaxHealth = 100.f;
	Stamina = 100.f;
	MaxStamina = 100.f;
}

void UStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UStatsComponent, DeathState);
	DOREPLIFETIME_CONDITION(UStatsComponent, Health, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, MaxHealth, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, Stamina, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UStatsComponent, MaxStamina, COND_OwnerOnly);
}

void UStatsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Stamina is simulated on the server and replicated to clients.
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}
	if (DeathState != EDeathState::NotDead)
	{
		return;
	}

	// Only horizontal movement while sprinting drains stamina. Standing still or jumping in place does not.
	const bool bIsMoving = Owner->GetVelocity().Size2D() > 10.f;
	const bool bIsDraining = bIsSprinting && bIsMoving;

	const float Amount = bIsDraining ? -StaminaDrainRate * DeltaTime : StaminaRegenRate * DeltaTime;
	const bool bHasStamina = ChangeStaminaByAmount(Amount);

	if (bIsDraining && !bHasStamina)
	{
		bIsSprinting = false;
		OnStaminaDepleted.Broadcast();
	}
}


bool UStatsComponent::CanMutateStats() const
{
	const AActor* Owner = GetOwner();
	return !IsValid(Owner) || Owner->HasAuthority();
}
bool UStatsComponent::ChangeHealthByAmount(float Amount, AActor* Instigator)
{
	if (!CanMutateStats())
	{
		return Health > 0.f;
	}

	const float OldValue = Health;
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
	if (OldValue != Health)
	{
		OnHealthChanged.Broadcast(this, OldValue, Health, Instigator);
	}

	if (Health <= 0.f && OldValue > 0.f)
	{
		StartDeath();
	}

	return Health > 0.f;
}

void UStatsComponent::StartDeath()
{
	if (!CanMutateStats())
	{
		return;
	}

	if (DeathState != EDeathState::NotDead)
	{
		return;
	}

	DeathState = EDeathState::DeadStart;
	OnDeathStarted.Broadcast();

	if (AActor* Owner = GetOwner())
	{
		Owner->ForceNetUpdate();
	}
}

void UStatsComponent::ChangeMaxHealthByAmount(float Amount, AActor* Instigator)
{
	if (!CanMutateStats())
	{
		return;
	}

	const float OldValue = MaxHealth;
	MaxHealth = FMath::Max(0.f, MaxHealth + Amount);
	if (Health > MaxHealth)
	{
		Health = MaxHealth;
	}
	if (OldValue != MaxHealth)
	{
		OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, Instigator);
	}
}

bool UStatsComponent::ChangeStaminaByAmount(float Amount)
{
	if (!CanMutateStats())
	{
		return Stamina > 0.f;
	}

	const float OldValue = Stamina;
	Stamina = FMath::Clamp(Stamina + Amount, 0.f, MaxStamina);
	if (OldValue != Stamina)
	{
		OnStaminaChanged.Broadcast(this, OldValue, Stamina);
	}
	return Stamina > 0.f;
}

void UStatsComponent::ChangeMaxStaminaByAmount(float Amount)
{
	if (!CanMutateStats())
	{
		return;
	}

	const float OldValue = MaxStamina;
	MaxStamina = FMath::Max(0.f, MaxStamina + Amount);
	if (Stamina > MaxStamina)
	{
		Stamina = MaxStamina;
	}
	if (OldValue != MaxStamina)
	{
		OnMaxStaminaChanged.Broadcast(this, OldValue, MaxStamina);
	}
}

float UStatsComponent::GetHealthNormalized() const
{
	return (MaxHealth > 0.f) ? (Health / MaxHealth) : 0.f;
}

void UStatsComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UStatsComponent::OnRep_DeathState(EDeathState OldDeathState)
{
	if (DeathState == EDeathState::DeadStart && OldDeathState != EDeathState::DeadStart)
	{
		OnDeathStarted.Broadcast();
	}
}

void UStatsComponent::OnRep_Health(float OldValue)
{
	OnHealthChanged.Broadcast(this, OldValue, Health, nullptr);
}

void UStatsComponent::OnRep_MaxHealth(float OldValue)
{
	OnMaxHealthChanged.Broadcast(this, OldValue, MaxHealth, nullptr);
}

void UStatsComponent::OnRep_Stamina(float OldValue)
{
	OnStaminaChanged.Broadcast(this, OldValue, Stamina);
}

void UStatsComponent::OnRep_MaxStamina(float OldValue)
{
	OnMaxStaminaChanged.Broadcast(this, OldValue, MaxStamina);
}
