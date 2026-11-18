#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Weapon/WeaponMountComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesWeaponThirdPersonAttachTest, "RuinMates.Batch2.WeaponThirdPersonAttach",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesWeaponThirdPersonAttachTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* BodyMesh = LoadObject<USkeletalMesh>(nullptr,
		TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	if (!TestNotNull(TEXT("Template mannequin mesh is available"), BodyMesh))
	{
		return false;
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("RuinMatesWeaponAttachTest"));
	if (!TestNotNull(TEXT("Created a transient world"), World))
	{
		return false;
	}
	World->AddToRoot();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;

	ARuinmatesCharacter* Pawn = World->SpawnActor<ARuinmatesCharacter>(SpawnParams);
	AActor* WeaponActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!TestTrue(TEXT("Pawn spawned"), IsValid(Pawn)) || !TestTrue(TEXT("Weapon actor spawned"), IsValid(WeaponActor)))
	{
		if (IsValid(WeaponActor)) { WeaponActor->Destroy(); }
		if (IsValid(Pawn)) { Pawn->Destroy(); }
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	Pawn->GetMesh()->SetSkeletalMesh(BodyMesh);
	if (!TestTrue(TEXT("Body has a usable right hand"), UWeaponMountComponent::IsUsableAttachPoint(Pawn->GetMesh(), FName(TEXT("hand_r"))))
		|| !TestTrue(TEXT("Body has a usable left hand"), UWeaponMountComponent::IsUsableAttachPoint(Pawn->GetMesh(), FName(TEXT("hand_l")))))
	{
		WeaponActor->Destroy();
		Pawn->Destroy();
		World->DestroyWorld(false);
		World->RemoveFromRoot();
		return false;
	}

	USkeletalMeshComponent* Gun3P = NewObject<USkeletalMeshComponent>(WeaponActor, TEXT("Gun3P"));
	Gun3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Gun3P->RegisterComponent();
	WeaponActor->SetRootComponent(Gun3P);

	UWeaponMountComponent* Mount = NewObject<UWeaponMountComponent>(WeaponActor, TEXT("Mount"));
	Mount->RegisterComponent();
	Mount->SetPawnMesh(Pawn->GetMesh());
	Mount->SetWeaponMesh(Gun3P);
	Mount->Mesh3PAttachSocket = FName(TEXT("hand_r"));
	Mount->DefaultGripSocket = FName(TEXT("hand_r"));
	Mount->Mesh3PAttachTransform = FTransform(FRotator(12.f, -25.f, 8.f), FVector(3.f, -1.5f, 2.f), FVector::OneVector);
	TestFalse(TEXT("Calibrated mount includes an authored orientation"), Mount->Mesh3PAttachTransform.GetRotation().Equals(FQuat::Identity, 0.01f));

	Mount->AttachToOwningPawn();
	TestEqual(TEXT("Third-person parent is the body"), Gun3P->GetAttachParent(), static_cast<USceneComponent*>(Pawn->GetMesh()));
	TestEqual(TEXT("Third-person socket is hand_r"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
	TestTrue(TEXT("Full calibrated transform survives attachment"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	Mount->AttachToPawnSocket(Pawn->GetMesh(), Mount->DefaultGripSocket);
	TestEqual(TEXT("Grip notify restores the calibrated right hand"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
	TestTrue(TEXT("Grip notify does not accumulate offsets"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	// Re-grip repeatedly to prove the calibrated transform is not stacked.
	for (int32 i = 0; i < 3; ++i)
	{
		Mount->AttachToPawnSocket(Pawn->GetMesh(), FName(TEXT("hand_r")));
	}
	TestTrue(TEXT("Repeated right-hand re-grips do not accumulate offsets"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	Mount->AttachToPawnSocket(Pawn->GetMesh(), FName(TEXT("hand_l")));
	TestEqual(TEXT("Left-hand transfer is respected"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_l")));

	Mount->AttachToPawnSocket(Pawn->GetMesh(), FName(TEXT("hand_r")));
	TestTrue(TEXT("Right-hand return restores the full mount"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	Mount->AttachToPawnSocket(Pawn->GetMesh(), FName(TEXT("ik_hand_gun")));
	TestEqual(TEXT("Legacy gun IK notify uses the physical hand mount"), Gun3P->GetAttachSocketName(), FName(TEXT("hand_r")));
	TestTrue(TEXT("Legacy gun IK notify restores the full mount"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	Mount->DetachFromPawn();
	Mount->AttachToOwningPawn();
	TestTrue(TEXT("Re-equipping preserves the mount"), Gun3P->GetRelativeTransform().Equals(Mount->Mesh3PAttachTransform, 0.001f));

	// Null-safe: missing meshes must not crash.
	Mount->SetWeaponMesh(nullptr);
	Mount->AttachToOwningPawn();
	Mount->AttachToPawnSocket(Pawn->GetMesh(), FName(TEXT("hand_r")));
	Mount->DetachFromPawn();
	TestTrue(TEXT("Null weapon mesh attach/detach is safe"), true);

	WeaponActor->Destroy();
	Pawn->Destroy();
	World->DestroyWorld(false);
	World->RemoveFromRoot();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
