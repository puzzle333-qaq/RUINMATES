#include "RuinmatesAnimSetupCommandlet.h"

#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Fabrik.h"
#include "AnimGraphNode_ModifyBone.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Weapon/WeaponData.h"

namespace
{
	bool BindAnimProperty(UAnimGraphNode_Base* Node, FName PinName, const FString& VariableName)
	{
		if (!Node)
		{
			return false;
		}
		if (Node->HasBinding(PinName))
		{
			UE_LOG(LogTemp, Display, TEXT("Already bound %s.%s"), *Node->GetName(), *PinName.ToString());
			return true;
		}

		FObjectProperty* BindingProperty = FindFProperty<FObjectProperty>(UAnimGraphNode_Base::StaticClass(), TEXT("Binding"));
		UClass* BindingClass = FindObject<UClass>(nullptr, TEXT("/Script/AnimGraph.AnimGraphNodeBinding_Base"));
		if (!BindingProperty || !BindingClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Anim graph binding type is not loaded"));
			return false;
		}

		UObject* Binding = BindingProperty->GetObjectPropertyValue_InContainer(Node);
		if (!Binding)
		{
			Binding = NewObject<UObject>(Node, BindingClass);
			BindingProperty->SetObjectPropertyValue_InContainer(Node, Binding);
		}

		FMapProperty* MapProperty = FindFProperty<FMapProperty>(Binding->GetClass(), TEXT("PropertyBindings"));
		if (!MapProperty)
		{
			UE_LOG(LogTemp, Error, TEXT("PropertyBindings map missing on %s"), *Binding->GetClass()->GetName());
			return false;
		}

		FScriptMapHelper MapHelper(MapProperty, MapProperty->ContainerPtrToValuePtr<void>(Binding));
		const int32 Index = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
		*reinterpret_cast<FName*>(MapHelper.GetKeyPtr(Index)) = PinName;

		FAnimGraphNodePropertyBinding* Value = reinterpret_cast<FAnimGraphNodePropertyBinding*>(MapHelper.GetValuePtr(Index));
		Value->PropertyName = PinName;
		Value->bIsBound = true;
		Value->Type = EAnimGraphNodePropertyBindingType::Property;
		Value->PropertyPath = { VariableName };
		Value->PathAsText = FText::FromString(VariableName);
		MapHelper.Rehash();

		Node->Modify();
		UE_LOG(LogTemp, Display, TEXT("Bound %s.%s -> %s"), *Node->GetName(), *PinName.ToString(), *VariableName);
		return true;
	}
}

URuinmatesAnimSetupCommandlet::URuinmatesAnimSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 URuinmatesAnimSetupCommandlet::Main(const FString& Params)
{
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Character/ABP_Unarmed.ABP_Unarmed"));
	UAnimMontage* FireMontage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Pistol/MM_Pistol_Fire_Montage.MM_Pistol_Fire_Montage"));
	UWeaponData* WeaponData = LoadObject<UWeaponData>(nullptr, TEXT("/Game/RUINMATES/Blueprint/Data/DA_WeaponDATA.DA_WeaponDATA"));
	if (!AnimBP || !FireMontage || !WeaponData)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing ABP, fire montage, or weapon data"));
		return 1;
	}

	if (FireMontage->SlotAnimTracks.Num() > 0 && FireMontage->SlotAnimTracks[0].SlotName != TEXT("DefaultSlot"))
	{
		FireMontage->Modify();
		FireMontage->SlotAnimTracks[0].SlotName = TEXT("DefaultSlot");
		UE_LOG(LogTemp, Display, TEXT("Fire montage slot set to DefaultSlot"));
	}
	if (!WeaponData->ThirdPersonMontages.Fire)
	{
		WeaponData->Modify();
		WeaponData->ThirdPersonMontages.Fire = FireMontage;
		UE_LOG(LogTemp, Display, TEXT("Assigned fire montage on DA_WeaponDATA"));
	}

	bool bHasCrouchBone = false;
	UAnimGraphNode_Fabrik* Fabrik = nullptr;
	for (TObjectIterator<UAnimGraphNode_Base> It; It; ++It)
	{
		UAnimGraphNode_Base* Object = *It;
		if (!Object->IsIn(AnimBP))
		{
			continue;
		}
		if (UAnimGraphNode_ModifyBone* Modify = Cast<UAnimGraphNode_ModifyBone>(Object))
		{
			if (const FAnimNode_ModifyBone* Bone = static_cast<const FAnimNode_ModifyBone*>(Modify->GetFNode()))
			{
				if (Bone->BoneToModify.BoneName == TEXT("pelvis"))
				{
					bHasCrouchBone = true;
				}
			}
		}
		else if (UAnimGraphNode_BlendSpacePlayer* Blend = Cast<UAnimGraphNode_BlendSpacePlayer>(Object))
		{
			BindAnimProperty(Blend, TEXT("BlendSpace"), TEXT("WeaponBlendSpace"));
		}
		else if (UAnimGraphNode_SequencePlayer* Sequence = Cast<UAnimGraphNode_SequencePlayer>(Object))
		{
			const UAnimationAsset* Asset = Sequence->GetAnimationAsset();
			if (Asset && Asset->GetName() == TEXT("MM_Idle"))
			{
				BindAnimProperty(Sequence, TEXT("Sequence"), TEXT("WeaponSequence"));
			}
		}
		else if (!Fabrik)
		{
			if (UAnimGraphNode_Fabrik* Found = Cast<UAnimGraphNode_Fabrik>(Object))
			{
				if (Found->GetPathName().Contains(TEXT("ABP_Unarmed:AnimGraph.")))
				{
					Fabrik = Found;
				}
			}
		}
	}

	if (!bHasCrouchBone && Fabrik)
	{
		UEdGraphPin* FabrikPose = Fabrik->FindPin(TEXT("ComponentPose"), EGPD_Input);
		if (FabrikPose && FabrikPose->LinkedTo.Num() > 0)
		{
			UEdGraphPin* Previous = FabrikPose->LinkedTo[0];
			UEdGraph* Graph = Fabrik->GetGraph();
			UAnimGraphNode_ModifyBone* BoneNode = NewObject<UAnimGraphNode_ModifyBone>(Graph, TEXT("RuinmatesCrouchPelvis"));
			Graph->AddNode(BoneNode, false, false);
			BoneNode->CreateNewGuid();
			BoneNode->PostPlacedNewNode();
			BoneNode->AllocateDefaultPins();

			if (FAnimNode_ModifyBone* Bone = static_cast<FAnimNode_ModifyBone*>(BoneNode->GetFNode()))
			{
				Bone->BoneToModify.BoneName = TEXT("pelvis");
				Bone->TranslationMode = BMM_Additive;
				Bone->TranslationSpace = BCS_ComponentSpace;
				Bone->RotationMode = BMM_Ignore;
				Bone->ScaleMode = BMM_Ignore;
			}

			BindAnimProperty(BoneNode, TEXT("Translation"), TEXT("CrouchPelvisOffset"));

			UEdGraphPin* BoneIn = BoneNode->FindPin(TEXT("ComponentPose"), EGPD_Input);
			UEdGraphPin* BoneOut = BoneNode->FindPin(TEXT("Pose"), EGPD_Output);
			const bool bConnected = BoneIn && BoneOut
				&& Graph->GetSchema()->TryCreateConnection(Previous, BoneIn)
				&& Graph->GetSchema()->TryCreateConnection(BoneOut, FabrikPose);
			if (bConnected)
			{
				Previous->BreakLinkTo(FabrikPose);
				UE_LOG(LogTemp, Display, TEXT("Inserted pelvis crouch before FABRIK"));
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Crouch bone pins did not connect"));
			}
		}
	}
	else if (!Fabrik)
	{
		UE_LOG(LogTemp, Warning, TEXT("FABRIK node was not found; crouch offset is not in the graph"));
	}

	FKismetEditorUtilities::CompileBlueprint(AnimBP);

	TArray<UPackage*> Packages = { AnimBP->GetPackage(), FireMontage->GetPackage(), WeaponData->GetPackage() };
	if (!UEditorLoadingAndSavingUtils::SavePackages(Packages, false))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to save anim setup packages"));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Anim setup saved. Blueprint status %d"), static_cast<int32>(AnimBP->Status));
	return AnimBP->Status == BS_Error ? 1 : 0;
}
