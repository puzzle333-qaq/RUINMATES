#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Stats/StatsComponent.h"
#include "UObject/Object.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesCharacterMovementTest, "RuinMates.Batch2.CharacterMovement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesCharacterMovementTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesCharacterMovementTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Character = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	if (!TestTrue(TEXT("Character spawned"), IsValid(Character)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	UStatsComponent* Stats = Character->GetStats();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	const bool bOk = TestTrue(TEXT("Stats component exists"), IsValid(Stats))
		&& TestTrue(TEXT("Movement component exists"), IsValid(Movement));
	if (!bOk)
	{
		Character->Destroy();
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	TestEqual(TEXT("Default walk speed is run speed"), Movement->MaxWalkSpeed, 250.f);
	TestEqual(TEXT("Default crouched speed uses the 0.6 multiplier"), Movement->MaxWalkSpeedCrouched, 150.f);
	TestFalse(TEXT("Does not start sprinting"), Character->IsSprinting());

	if (!Character->HasActorBegunPlay())
	{
		FEditorScriptExecutionGuard BeginPlayGuard;
		Character->DispatchBeginPlay();
	}

	Character->StartSprint();
	TestTrue(TEXT("StartSprint is accepted on authority"), Character->IsSprinting());
	TestEqual(TEXT("Sprint speed applied"), Movement->MaxWalkSpeed, 350.f);
	TestTrue(TEXT("Stats component is marked sprinting"), Stats->IsSprinting());

	Character->StopSprint();
	TestFalse(TEXT("StopSprint clears the sprint flag"), Character->IsSprinting());
	TestEqual(TEXT("Run speed restored"), Movement->MaxWalkSpeed, 250.f);
	TestFalse(TEXT("Stats component is not sprinting"), Stats->IsSprinting());

	if (!Character->HasActorBegunPlay())
	{
		FEditorScriptExecutionGuard BeginPlayGuard;
		Character->DispatchBeginPlay();
	}

	Character->StartSprint();
	{
		FEditorScriptExecutionGuard ScriptGuard;
		Stats->OnStaminaDepleted.Broadcast();
	}
	TestFalse(TEXT("Stamina depletion stops sprint"), Character->IsSprinting());
	TestEqual(TEXT("Stamina depletion restores run speed"), Movement->MaxWalkSpeed, 250.f);

	TestTrue(TEXT("Jump stamina is available at spawn"), Stats->HasStaminaForJump());
	const float StaminaBeforeJump = Stats->Stamina;
	Character->OnJumped_Implementation();
	TestEqual(TEXT("Server jump deducts jump stamina cost"), Stats->Stamina, StaminaBeforeJump - Stats->JumpStaminaCost);

	Stats->ChangeStaminaByAmount(-Stats->Stamina);
	TestFalse(TEXT("Jump stamina is exhausted"), Stats->HasStaminaForJump());
	TestFalse(TEXT("Jump denied without stamina"), Character->CanJumpInternal_Implementation());

	Character->SetRole(ROLE_SimulatedProxy);
	const float ClientStamina = Stats->Stamina;
	Character->OnJumped_Implementation();
	TestEqual(TEXT("Client jump does not deduct stamina"), Stats->Stamina, ClientStamina);

	Character->Destroy();
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
