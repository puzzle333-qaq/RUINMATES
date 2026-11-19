#include "RuinmatesCombatSetupCommandlet.h"
#include "Animation/RuinmatesAnimInstance.h"
#include "Character/RuinmatesCharacter.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_RotationOffsetBlendSpace.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_Fabrik.h"
#include "AnimStateNode.h"
#include "AnimStateEntryNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AnimationTransitionGraph.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_StateResult.h"
#include "Misc/PackageName.h"
#include "EdGraph/EdGraph.h"
#include "K2Node_VariableGet.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "FileHelpers.h"
#include "UObject/SavePackage.h"
#include "NiagaraSystem.h"
#include "NiagaraUserRedirectionParameterStore.h"

namespace CombatSetup
{
	template<class T> T* Add(UEdGraph* Graph)
	{
		T* Node = NewObject<T>(Graph);
		Graph->AddNode(Node, false, false);
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		Node->AllocateDefaultPins();
		Node->NodePosX = Graph->Nodes.Num() * 220;
		return Node;
	}
	void Link(UEdGraphNode* From, FName Out, UEdGraphNode* To, FName In)
	{
		UEdGraphPin* A = From->FindPin(Out, EGPD_Output);
		UEdGraphPin* B = To->FindPin(In, EGPD_Input);
		if (!A || !B || !From->GetGraph()->GetSchema()->TryCreateConnection(A, B))
		{
			UE_LOG(LogTemp, Error, TEXT("Combat graph connection failed: %s.%s -> %s.%s"),
				*From->GetName(), *Out.ToString(), *To->GetName(), *In.ToString());
		}
	}
	void Variable(UEdGraphNode* Target, FName Pin, FName Property)
	{
		UK2Node_VariableGet* Get = NewObject<UK2Node_VariableGet>(Target->GetGraph());
		Get->VariableReference.SetSelfMember(Property);
		Target->GetGraph()->AddNode(Get, false, false);
		Get->CreateNewGuid();
		Get->AllocateDefaultPins();
		Get->NodePosX = Target->NodePosX - 180;
		Get->NodePosY = Target->NodePosY + 180;
		Link(Get, Property, Target, Pin);
	}
	void Expose(UAnimGraphNode_Base* Node, const TArray<FName>& Names)
	{
		for (FOptionalPinFromProperty& Pin : Node->ShowPinForProperties)
		{
			if (Names.Contains(Pin.PropertyName)) Pin.bShowPin = true;
		}
		Node->ReconstructNode();
	}
	UAnimSequence* Sequence(const TCHAR* Path) { return LoadObject<UAnimSequence>(nullptr, Path); }
	void Save(UObject* Object)
	{
		Object->MarkPackageDirty();
		TArray<UPackage*> Packages = { Object->GetPackage() };
		UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
	}
}

URuinmatesCombatSetupCommandlet::URuinmatesCombatSetupCommandlet()
{
	IsEditor = true;
	LogToConsole = true;
}

int32 URuinmatesCombatSetupCommandlet::Main(const FString& Params)
{
	using namespace CombatSetup;
	UBlueprint* CharacterBP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Character/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"));
	ARuinmatesCharacter* Character = CharacterBP ? Cast<ARuinmatesCharacter>(CharacterBP->GeneratedClass->GetDefaultObject()) : nullptr;
	if (!Character || !Character->GetMesh() || !Character->GetMesh()->GetSkeletalMeshAsset()) return 1;
	if (Params.Contains(TEXT("EquipMontage")))
	{
		UWeaponData* Data = LoadObject<UWeaponData>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA.DA_WeaponDATA"));
		UAnimSequence* Clip = Sequence(TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Equip.MM_Pistol_Equip"));
		if (!Data || !Clip) return 1;
		const TCHAR* Path = TEXT("/Game/RUINMATES/Animation/Combat/AM_Pistol_Equip");
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/RUINMATES/Animation/Combat/AM_Pistol_Equip.AM_Pistol_Equip"));
		if (!Montage)
		{
			UAnimMontage* Dynamic = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Clip, TEXT("UpperBody"), 0.1f, 0.1f);
			Montage = DuplicateObject<UAnimMontage>(Dynamic, CreatePackage(Path), TEXT("AM_Pistol_Equip"));
			Montage->SetFlags(RF_Public | RF_Standalone);
			Save(Montage);
		}
		Data->ThirdPersonMontages.Equip = Montage;
		// AO center is mesh-space additive and cannot serve as the non-additive base pose.
		Save(Data);
		return 0;
	}
	if (Params.Contains(TEXT("Inspect")))
	{
		for (const TCHAR* Path : { TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire.NS_WeaponFire"), TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_Tracer.NS_WeaponFire_Tracer") })
		{
			if (UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, Path))
			{
				TArray<FNiagaraVariable> Variables;
				System->GetExposedParameters().GetUserParameters(Variables);
				for (const FNiagaraVariable& V : Variables) UE_LOG(LogTemp, Display, TEXT("Combat Niagara %s: %s (%s)"), *System->GetName(), *V.GetName().ToString(), *V.GetType().GetName());
			}
		}
		if (UWeaponData* Data = LoadObject<UWeaponData>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA.DA_WeaponDATA")))
		{
			for (UBlendSpace* Blend : { Data->ThirdPersonAnims.AimOffset.Get() })
			{
				if (!Blend) continue;
				for (int32 Axis = 0; Axis < 2; ++Axis)
				{
					const FBlendParameter& P = Blend->GetBlendParameter(Axis);
					UE_LOG(LogTemp, Display, TEXT("Combat blend %s axis %d: %s %f..%f"), *Blend->GetName(), Axis, *P.DisplayName, P.Min, P.Max);
				}
				for (const FBlendSample& Sample : Blend->GetBlendSamples()) if (Sample.Animation)
					UE_LOG(LogTemp, Display, TEXT("Combat sample %s: additive %d skeleton %s"), *Sample.Animation->GetName(), int32(Sample.Animation->AdditiveAnimType), *GetNameSafe(Sample.Animation->GetSkeleton()));
			}
		}
		return 0;
	}
	if (Params.Contains(TEXT("Camera")))
	{
		// All lateral/elevation offsets belong to the arm. A forward camera-child offset bypasses collision.
		Character->GetFollowCamera()->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Character->GetFollowCamera()->bUsePawnControlRotation = false;
		Character->GetCameraBoom()->bUsePawnControlRotation = true;
		Character->GetCameraBoom()->bDoCollisionTest = true;
		Character->GetCameraBoom()->TargetArmLength = 300.f;
		Character->GetCameraBoom()->SocketOffset = FVector(0.f, 50.f, 60.f);
		FKismetEditorUtilities::CompileBlueprint(CharacterBP);
		Save(CharacterBP);
		return 0;
	}
	const TCHAR* PackageName = TEXT("/Game/RUINMATES/Blueprint/Character/ABP_RuinmatesCombat");
	if (FPackageName::DoesPackageExist(PackageName))
	{
		UE_LOG(LogTemp, Error, TEXT("New combat ABP already exists; refusing to overwrite it."));
		return 1;
	}
	USkeleton* Skeleton = Character->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton();
	if (!Skeleton || Skeleton->GetReferenceSkeleton().FindBoneIndex(TEXT("spine_01")) == INDEX_NONE) return 1;
	UAnimBlueprint* BP = Cast<UAnimBlueprint>(FKismetEditorUtilities::CreateBlueprint(
		URuinmatesAnimInstance::StaticClass(), CreatePackage(PackageName), TEXT("ABP_RuinmatesCombat"),
		BPTYPE_Normal, UAnimBlueprint::StaticClass(), UAnimBlueprintGeneratedClass::StaticClass()));
	BP->TargetSkeleton = Skeleton;
	UEdGraph* Graph = nullptr;
	for (UEdGraph* G : BP->FunctionGraphs) if (G->GetFName() == TEXT("AnimGraph")) Graph = G;
	if (!Graph) return 1;
	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes) if (auto* R = Cast<UAnimGraphNode_Root>(Node)) Root = R;
	if (!Root) return 1;
	UBlendSpace* Locomotion = LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.BS_Idle_Walk_Run"));
	UAnimSequence* Idle = Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	UAnimSequence* Jump = Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Jump.MM_Jump"));
	UAnimSequence* Fall = Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Fall_Loop.MM_Fall_Loop"));
	UAnimSequence* Land = Sequence(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jump/MM_Land.MM_Land"));
	UAnimGraphNode_StateMachine* Machine = Add<UAnimGraphNode_StateMachine>(Graph);
	Machine->EditorStateMachineGraph->Rename(TEXT("Locomotion"), nullptr);
	TArray<UAnimStateNode*> States;
	const TCHAR* Names[] = { TEXT("Idle"), TEXT("Walk"), TEXT("Run"), TEXT("Sprint"), TEXT("Crouch"), TEXT("Jump"), TEXT("Falling"), TEXT("Landing") };
	for (int32 Index = 0; Index < 8; ++Index)
	{
		UAnimStateNode* State = Add<UAnimStateNode>(Machine->EditorStateMachineGraph);
		State->BoundGraph->Rename(Names[Index], nullptr);
		State->NodePosX = (Index % 4) * 250;
		State->NodePosY = (Index / 4) * 220;
		States.Add(State);
		if (Index > 0 && Index < 5)
		{
			auto* Player = Add<UAnimGraphNode_BlendSpacePlayer>(State->BoundGraph);
			Player->SetAnimationAsset(Locomotion);
			Expose(Player, { TEXT("BlendSpace"), TEXT("X"), TEXT("Y") });
			Variable(Player, TEXT("BlendSpace"), TEXT("WeaponBlendSpace"));
			Variable(Player, TEXT("X"), TEXT("BlendX"));
			Variable(Player, TEXT("Y"), TEXT("BlendY"));
			Link(Player, TEXT("Pose"), State->GetResultNodeInsideState(), TEXT("Result"));
		}
		else
		{
			auto* Player = Add<UAnimGraphNode_SequencePlayer>(State->BoundGraph);
			Player->SetAnimationAsset(Index == 5 ? Jump : (Index == 6 ? Fall : (Index == 7 ? Land : Idle)));
			Expose(Player, { TEXT("Sequence") });
			Variable(Player, TEXT("Sequence"), Index >= 5 ? TEXT("AirSequence") : TEXT("WeaponSequence"));
			Link(Player, TEXT("Pose"), State->GetResultNodeInsideState(), TEXT("Result"));
		}
	}
	for (UEdGraphNode* Node : Machine->EditorStateMachineGraph->Nodes)
		if (auto* Entry = Cast<UAnimStateEntryNode>(Node)) Link(Entry, TEXT("Entry"), States[0], TEXT("In"));
	for (int32 From = 0; From < 8; ++From) for (int32 To = 0; To < 8; ++To)
	{
		if (From == To) continue;
		auto* Transition = Add<UAnimStateTransitionNode>(Machine->EditorStateMachineGraph);
		Transition->CreateConnections(States[From], States[To]);
		Transition->CrossfadeDuration = 0.12f;
		auto* Rule = Cast<UAnimationTransitionGraph>(Transition->BoundGraph);
		auto* Equal = NewObject<UK2Node_CallFunction>(Rule);
		Equal->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("EqualEqual_IntInt")));
		Rule->AddNode(Equal, false, false);
		Equal->CreateNewGuid();
		Equal->AllocateDefaultPins();
		Equal->FindPinChecked(TEXT("B"))->DefaultValue = FString::FromInt(To);
		Variable(Equal, TEXT("A"), TEXT("LocomotionState"));
		Link(Equal, TEXT("ReturnValue"), Rule->GetResultNode(), TEXT("bCanEnterTransition"));
	}
	// Independent upper body pose -> aim additive -> montage -> spine mask -> left hand IK.
	auto* Upper = Add<UAnimGraphNode_SequencePlayer>(Graph);
	Upper->SetAnimationAsset(Idle);
	Expose(Upper, { TEXT("Sequence") });
	Variable(Upper, TEXT("Sequence"), TEXT("WeaponSequence"));
	auto* Aim = Add<UAnimGraphNode_RotationOffsetBlendSpace>(Graph);
	Aim->SetAnimationAsset(LoadObject<UBlendSpace>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/Aim/AO_Pistol.AO_Pistol")));
	Expose(Aim, { TEXT("BlendSpace"), TEXT("X"), TEXT("Y"), TEXT("Alpha") });
	Variable(Aim, TEXT("BlendSpace"), TEXT("WeaponAimOffset"));
	Variable(Aim, TEXT("X"), TEXT("AimYaw"));
	Variable(Aim, TEXT("Y"), TEXT("AimPitch"));
	Variable(Aim, TEXT("Alpha"), TEXT("AimOffsetAlpha"));
	Link(Upper, TEXT("Pose"), Aim, TEXT("BasePose"));
	auto* Slot = Add<UAnimGraphNode_Slot>(Graph);
	Slot->Node.SlotName = TEXT("UpperBody");
	Link(Aim, TEXT("Pose"), Slot, TEXT("Source"));
	auto* Layer = Add<UAnimGraphNode_LayeredBoneBlend>(Graph);
	if (Layer->Node.BlendPoses.IsEmpty()) Layer->AddPinToBlendByFilter();
	FBranchFilter Filter;
	Filter.BoneName = TEXT("spine_01");
	Filter.BlendDepth = 3;
	Layer->Node.LayerSetup[0].BranchFilters = { Filter };
	Layer->Node.bMeshSpaceRotationBlend = true;
	Layer->ReconstructNode();
	Link(Machine, TEXT("Pose"), Layer, TEXT("BasePose"));
	Link(Slot, TEXT("Pose"), Layer, TEXT("BlendPoses_0"));
	Variable(Layer, TEXT("BlendWeights_0"), TEXT("UpperBodyAlpha"));
	auto* Full = Add<UAnimGraphNode_Slot>(Graph);
	Full->Node.SlotName = TEXT("FullBody");
	Link(Layer, TEXT("Pose"), Full, TEXT("Source"));
	auto* ToComponent = Add<UAnimGraphNode_LocalToComponentSpace>(Graph);
	Link(Full, TEXT("Pose"), ToComponent, TEXT("LocalPose"));
	auto* IK = Add<UAnimGraphNode_Fabrik>(Graph);
	IK->Node.RootBone.BoneName = TEXT("upperarm_l");
	IK->Node.TipBone.BoneName = TEXT("hand_l");
	IK->Node.EffectorTransformSpace = BCS_BoneSpace;
	IK->Node.EffectorTarget.BoneReference.BoneName = TEXT("hand_r");
	Expose(IK, { TEXT("EffectorTransform"), TEXT("Alpha") });
	Variable(IK, TEXT("EffectorTransform"), TEXT("LeftHandTransform"));
	Variable(IK, TEXT("Alpha"), TEXT("LeftHandAlpha"));
	Link(ToComponent, TEXT("ComponentPose"), IK, TEXT("ComponentPose"));
	auto* ToLocal = Add<UAnimGraphNode_ComponentToLocalSpace>(Graph);
	Link(IK, TEXT("Pose"), ToLocal, TEXT("ComponentPose"));
	Link(ToLocal, TEXT("Pose"), Root, TEXT("Result"));
	Skeleton->RegisterSlotNode(TEXT("UpperBody"));
	Skeleton->RegisterSlotNode(TEXT("FullBody"));
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) return 1;
	auto* Defaults = Cast<URuinmatesAnimInstance>(BP->GeneratedClass->GetDefaultObject());
	Defaults->UnarmedBlendSpace = Locomotion;
	Defaults->UnarmedIdle = Idle;
	Defaults->UnarmedJump = Jump;
	Defaults->UnarmedFall = Fall;
	Defaults->UnarmedLand = Land;
	Save(BP);
	Save(Skeleton);
	Character->GetMesh()->SetAnimInstanceClass(BP->GeneratedClass);
	FKismetEditorUtilities::CompileBlueprint(CharacterBP);
	Save(CharacterBP);
	return CharacterBP->Status == BS_Error ? 1 : 0;
}
