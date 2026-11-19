// Co-op friendly fire: teammate damage is scaled on the server, enemies take full damage.
#include "Misc/AutomationTest.h"
#include "Game/RuinmatesGameMode.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesFriendlyFireTest, "RuinMates.Combat.FriendlyFire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesFriendlyFireTest::RunTest(const FString& Parameters)
{
	const ARuinmatesGameMode* Defaults = GetDefault<ARuinmatesGameMode>();
	if (!TestNotNull(TEXT("Game mode defaults exist"), Defaults))
	{
		return false;
	}
	TestEqual(TEXT("Default friendly fire multiplier is 0.25"), Defaults->GetFriendlyFireMultiplier(), 0.25f);

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesFriendlyFireTest"));
	if (!TestNotNull(TEXT("World"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	ARuinmatesCharacter* First = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	ARuinmatesCharacter* Second = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	AStaticMeshActor* Enemy = World->SpawnActor<AStaticMeshActor>(SpawnParams);
	if (!TestTrue(TEXT("Characters and enemy spawned"), IsValid(First) && IsValid(Second) && IsValid(Enemy)))
	{
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	TestTrue(TEXT("Two player characters are teammates"), ARuinmatesGameMode::AreTeammates(First, Second));
	TestFalse(TEXT("A character is not its own teammate"), ARuinmatesGameMode::AreTeammates(First, First));
	TestFalse(TEXT("A non-character target is not a teammate"), ARuinmatesGameMode::AreTeammates(First, Enemy));
	TestFalse(TEXT("A null instigator is not a teammate"), ARuinmatesGameMode::AreTeammates(nullptr, Second));

	TestEqual(TEXT("Teammate damage is scaled by the default multiplier"),
		Defaults->ScaleDamageForTarget(100.f, First, Second), 25.f);
	TestEqual(TEXT("Enemy damage keeps the full value"),
		Defaults->ScaleDamageForTarget(100.f, First, Enemy), 100.f);
	TestEqual(TEXT("Zero damage stays zero"), Defaults->ScaleDamageForTarget(0.f, First, Second), 0.f);

	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}
#endif