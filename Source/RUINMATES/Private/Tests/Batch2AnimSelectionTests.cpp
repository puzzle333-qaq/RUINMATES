#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/RuinmatesAnimInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesAnimSelectionTest, "RuinMates.Batch2.AnimSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesAnimSelectionTest::RunTest(const FString& Parameters)
{
	UBlendSpace* Unarmed = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	UAnimSequence* Jump = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
	UAnimSequence* Fall = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	UAnimSequence* PistolIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MF_Pistol_Idle_ADS.MF_Pistol_Idle_ADS"));
	UAnimSequence* PistolAim = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/Aim/MF_Pistol_Idle_ADS1.MF_Pistol_Idle_ADS1"));
	UBlendSpace* AimOffset = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/Aim/AO_Pistol.AO_Pistol"));

	if (!TestNotNull(TEXT("Unarmed blend space loads"), Unarmed)
		|| !TestNotNull(TEXT("Unarmed idle loads"), Idle)
		|| !TestNotNull(TEXT("Pistol idle loads"), PistolIdle)
		|| !TestNotNull(TEXT("Aim offset loads"), AimOffset))
	{
		return false;
	}

	FPlayerAnim Empty;
	const FRuinmatesAnimChoice Bare = URuinmatesAnimInstance::ChooseAnims(Empty, false, false, false, 0.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("No weapon data keeps the unarmed blend space"), Bare.BlendSpace, Unarmed);
	TestEqual(TEXT("No weapon data keeps the unarmed idle"), Bare.Sequence, Idle);
	TestFalse(TEXT("Standing does not drop the pelvis"), Bare.bApplyCrouchOffset);

	const FRuinmatesAnimChoice BareCrouch = URuinmatesAnimInstance::ChooseAnims(Empty, true, false, false, 0.f, Unarmed, Idle, Jump, Fall);
	TestTrue(TEXT("Crouch without a crouch clip asks for a pelvis drop"), BareCrouch.bApplyCrouchOffset);
	TestEqual(TEXT("Crouch without a crouch clip still has a blend space"), BareCrouch.BlendSpace, Unarmed);

	FPlayerAnim Pistol;
	Pistol.Idle = PistolIdle;
	Pistol.AimOffset = AimOffset;

	const FRuinmatesAnimChoice Armed = URuinmatesAnimInstance::ChooseAnims(Pistol, false, false, false, 0.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("Weapon idle replaces the unarmed idle"), Armed.Sequence, static_cast<UAnimSequence*>(PistolIdle));
	TestEqual(TEXT("Locomotion is global, not per weapon"), Armed.BlendSpace, Unarmed);
	TestEqual(TEXT("The weapon aim offset is used"), Armed.AimOffset, static_cast<UBlendSpace*>(AimOffset));

	const FRuinmatesAnimChoice Aimed = URuinmatesAnimInstance::ChooseAnims(Pistol, false, false, true, 0.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("Aiming keeps the same weapon idle"), Aimed.Sequence, static_cast<UAnimSequence*>(PistolIdle));
	TestEqual(TEXT("Aiming keeps the same aim offset"), Aimed.AimOffset, static_cast<UBlendSpace*>(AimOffset));

	const FRuinmatesAnimChoice CrouchClip = URuinmatesAnimInstance::ChooseAnims(Pistol, true, false, false, 0.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("Crouch keeps the global blend space"), CrouchClip.BlendSpace, Unarmed);
	TestTrue(TEXT("No per-weapon crouch clip asks for a pelvis drop"), CrouchClip.bApplyCrouchOffset);
	const FRuinmatesAnimChoice Air = URuinmatesAnimInstance::ChooseAnims(Pistol, false, true, false, 100.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("Rising uses the jump sequence"), Air.Sequence, Jump);
	const FRuinmatesAnimChoice Falling = URuinmatesAnimInstance::ChooseAnims(Pistol, false, true, false, -100.f, Unarmed, Idle, Jump, Fall);
	TestEqual(TEXT("Falling uses the fall sequence"), Falling.Sequence, Fall);

	return true;
}

#endif
