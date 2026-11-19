#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "StatsComponent.generated.h"

UENUM(BlueprintType)
enum class EDeathState : uint8
{
	NotDead,
	DeadStart,
	DeadFinished
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FHealthChanged, UStatsComponent*, StatsComponent, float, OldValue, float, NewValue, AActor*, Instigator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStaminaChanged, UStatsComponent*, StatsComponent, float, OldValue, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStaminaDepleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDeathStarted);

// Replicated health and stamina. CQB-only systems (hit-zone vitals, stamina trim) are not included.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RUINMATES_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetSprinting(bool bNewSprinting) { bIsSprinting = bNewSprinting; }
	bool IsSprinting() const { return bIsSprinting; }

	UPROPERTY(BlueprintAssignable)
	FStaminaDepleted OnStaminaDepleted;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Stamina")
	float StaminaDrainRate = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Stamina")
	float StaminaRegenRate = 10.f;

	// Jump stamina cost. Deduct on the server at the jump call site.
	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Stamina")
	float JumpStaminaCost = 15.f;

	bool HasStaminaForJump() const { return Stamina >= JumpStaminaCost; }

	UFUNCTION(BlueprintPure, Category = "RuinMates|Health")
	static UStatsComponent* FindStatsComponent(const AActor* Actor)
	{
		return IsValid(Actor) ? Actor->FindComponentByClass<UStatsComponent>() : nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "RuinMates|Health")
	float GetHealthNormalized() const;

	/** Read-only accessors used by consumables; healing still goes through ChangeHealthByAmount. */
	UFUNCTION(BlueprintPure, Category = "RuinMates|Health")
	float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "RuinMates|Health")
	float GetMaxHealth() const { return MaxHealth; }
	UFUNCTION(BlueprintPure, Category = "RuinMates|Health")
	bool IsDead() const { return DeathState != EDeathState::NotDead; }

	// Applies a signed health delta. Starts death when health reaches 0. Returns whether the owner is still alive.
	bool ChangeHealthByAmount(float Amount, AActor* Instigator);
	void ChangeMaxHealthByAmount(float Amount, AActor* Instigator);

	// Applies a signed stamina delta. Returns whether stamina remains above 0.
	bool ChangeStaminaByAmount(float Amount);
	void ChangeMaxStaminaByAmount(float Amount);

	void StartDeath();

	// True when there is no owner (tests / CDO) or the owner has network authority.
	bool CanMutateStats() const;

	UPROPERTY(ReplicatedUsing = OnRep_DeathState)
	EDeathState DeathState;

	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "RuinMates|Health")
	float Health;

	UPROPERTY(ReplicatedUsing = OnRep_MaxHealth, EditDefaultsOnly, Category = "RuinMates|Health")
	float MaxHealth;

	UPROPERTY(ReplicatedUsing = OnRep_Stamina, EditDefaultsOnly, Category = "RuinMates|Stamina")
	float Stamina;

	UPROPERTY(ReplicatedUsing = OnRep_MaxStamina, EditDefaultsOnly, Category = "RuinMates|Stamina")
	float MaxStamina;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FHealthChanged OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable)
	FStaminaChanged OnMaxStaminaChanged;

	UPROPERTY(BlueprintAssignable)
	FDeathStarted OnDeathStarted;

protected:
	virtual void BeginPlay() override;
	bool bIsSprinting = false;

	UFUNCTION()
	void OnRep_DeathState(EDeathState OldDeathState);

	UFUNCTION()
	void OnRep_Health(float OldValue);

	UFUNCTION()
	void OnRep_MaxHealth(float OldValue);

	UFUNCTION()
	void OnRep_Stamina(float OldValue);

	UFUNCTION()
	void OnRep_MaxStamina(float OldValue);
};
