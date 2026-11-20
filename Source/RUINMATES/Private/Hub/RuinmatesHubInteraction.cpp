#include "Hub/RuinmatesHubInteraction.h"

#include "Character/RuinmatesCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Hub/RuinmatesHubDefinition.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Stats/StatsComponent.h"
#include "Online/RuinmatesSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "UI/RuinmatesHubScreen.h"

URuinmatesHubInteractionComponent::URuinmatesHubInteractionComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

APawn* URuinmatesHubInteractionComponent::GetOwningPawn() const { return Cast<APawn>(GetOwner()); }

void URuinmatesHubInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	CloseHub();
	Super::EndPlay(Reason);
}

void URuinmatesHubInteractionComponent::Client_OpenHub_Implementation(ARuinmatesHubTerminal* Terminal)
{
	ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(GetOwner());
	APlayerController* PC = Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
	if (!PC || !PC->IsLocalController() || !IsValid(Terminal) || !Terminal->ValidateAccess(Character)) return;
	Character->CloseTerminal();
	Character->EndTrade();
	Character->StopAim();
	CloseHub();
	Screen = CreateWidget<URuinmatesHubScreen>(PC, URuinmatesHubScreen::StaticClass());
	if (!Screen) return;
	Screen->SetContext(this, Terminal);
	Screen->AddToViewport(50);
	ActiveTerminal=Terminal;
	GetWorld()->GetTimerManager().SetTimer(AccessTimer,this,&ThisClass::CheckLocalAccess,.5f,true);
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(Screen->TakeWidget());
	PC->SetInputMode(Mode); PC->bShowMouseCursor = true;
}

void URuinmatesHubInteractionComponent::CloseHub()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AccessTimer);
	ActiveTerminal=nullptr;
	if (!Screen) return;
	Screen->RemoveFromParent(); Screen = nullptr;
	if (APawn* Pawn = GetOwningPawn())
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()); PC && PC->IsLocalController())
		{ PC->SetInputMode(FInputModeGameOnly()); PC->bShowMouseCursor = false; }
	}
}

void URuinmatesHubInteractionComponent::Client_HubResult_Implementation(bool bSuccess, const FText& Reason)
{
	UE_LOG(LogTemp, Log, TEXT("Hub command result: success=%d reason=%s"),bSuccess,*Reason.ToString());
	if (Screen) Screen->ShowResult(bSuccess, Reason);
}

void URuinmatesHubInteractionComponent::CheckLocalAccess()
{
	if (!IsValid(ActiveTerminal) || !ActiveTerminal->ValidateAccess(GetOwningPawn())) CloseHub();
}

void URuinmatesHubInteractionComponent::Server_RequestHub_Implementation(ARuinmatesHubTerminal* Terminal,
	ERuinmatesHubCommand Command, FName ActionOrItem, int32 Count, int32 ExpectedRevision)
{
	APawn* Pawn = GetOwningPawn();
	const double Now=GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (LastRequestTime >= 0.0 && Now-LastRequestTime<.15)
	{ Client_HubResult(false,FText::FromString(TEXT("操作过于频繁，请稍后重试"))); return; }
	LastRequestTime=Now;
	if (!IsValid(Pawn) || !IsValid(Terminal) || !Terminal->ValidateAccess(Pawn) || !Terminal->Hub)
	{ Client_HubResult(false, FText::FromString(TEXT("交互距离或视线无效"))); return; }
	ARuinmatesHubStateActor* Hub = Terminal->Hub;
	if (Terminal->Type != ERuinmatesHubTerminalType::Construction && Terminal->Type != ERuinmatesHubTerminalType::Generator)
	{ Client_HubResult(false, FText::FromString(TEXT("请使用中央建设终端"))); return; }
	FText Reason;
	bool bSuccess = false;
	switch (Command)
	{
	case ERuinmatesHubCommand::Construct: bSuccess = Hub->TryExecuteAction(Pawn, ActionOrItem, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::StarterKit: bSuccess = Hub->TryClaimStarterKit(Pawn, Reason); break;
	case ERuinmatesHubCommand::Install: bSuccess = Hub->TryInstallGenerator(Pawn, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Fuel: bSuccess = Hub->TryAddFuel(Pawn, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Start: bSuccess = Hub->TrySetGeneratorStarted(Pawn, true, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Stop: bSuccess = Hub->TrySetGeneratorStarted(Pawn, false, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Deposit: bSuccess = Hub->TryTransferWarehouse(Pawn, ActionOrItem, Count, true, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Withdraw: bSuccess = Hub->TryTransferWarehouse(Pawn, ActionOrItem, Count, false, ExpectedRevision, Reason); break;
	case ERuinmatesHubCommand::Save:
		bSuccess = Hub->SaveProgress();
		Reason = FText::FromString(bSuccess ? TEXT("基地进度已保存") : TEXT("保存失败，请重试")); break;
	default: Reason = FText::FromString(TEXT("无效基地操作")); break;
	}
	if (Reason.IsEmpty()) Reason = FText::FromString(TEXT("操作未完成，请刷新条件"));
	Client_HubResult(bSuccess, Reason);
}

ARuinmatesHubTerminal::ARuinmatesHubTerminal()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("Interaction"));
	SetRootComponent(InteractionBox); InteractionBox->SetBoxExtent(FVector(45,35,60));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

bool ARuinmatesHubTerminal::ValidateAccess(APawn* Pawn) const
{
	const ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(Pawn);
	if (!Character || !Character->GetController() || !Hub || !Hub->IsHubInitialized() || IsHidden()
		|| !GetWorld() || Pawn->GetWorld() != GetWorld()
		|| FVector::Dist(Pawn->GetActorLocation(), GetActorLocation()) > FMath::Clamp(InteractionDistance, 50.f, 350.f)) return false;
	if (Character->GetStats() && Character->GetStats()->IsDead()) return false;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HubTerminalVisibility), false, Pawn);
	// Server owns the Pawn eye position and reconstructs this trace; no client HitResult is used.
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Pawn->GetPawnViewLocation(),
		GetActorLocation(), ECC_Visibility, Query);
	return !bHit || Hit.GetActor() == this;
}

bool ARuinmatesHubTerminal::CanInteract_Implementation(APawn* Pawn) const { return ValidateAccess(Pawn); }

FText ARuinmatesHubTerminal::GetInteractPrompt_Implementation(APawn* Pawn) const
{
	if (!ValidateAccess(Pawn)) return FText::GetEmpty();
	if (Type==ERuinmatesHubTerminalType::Generator)
		return FText::FromString(Hub->GetSnapshot().Generator.bInstalled ? TEXT("按 F 管理燃油与供电") : TEXT("按 F 安装发电机"));
	if (Type==ERuinmatesHubTerminalType::Medical)
		return FText::FromString(Hub->IsFacilityOperational(RuinmatesHub::MedicalId()) ? TEXT("按 F 接受基础治疗") : TEXT("医疗室尚未解锁或缺少供电"));
	if (Type==ERuinmatesHubTerminalType::Exit)
		return FText::FromString(Hub->IsFacilityOperational(RuinmatesHub::ExitId()) ? TEXT("按 F 查看任务出口") : TEXT("出口需要修复 · 使用中央建设终端"));
	if (Type==ERuinmatesHubTerminalType::Elevator) return FText::FromString(TEXT("按 F 查看已开放楼层"));
	return FText::FromString(TEXT("按 F 使用基地终端"));
}

void ARuinmatesHubTerminal::Interact_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !ValidateAccess(Pawn)) return;
	URuinmatesHubInteractionComponent* Component = Pawn->FindComponentByClass<URuinmatesHubInteractionComponent>();
	if (!Component) return;
	if (Type == ERuinmatesHubTerminalType::Medical)
	{
		if (Hub->IsFacilityOperational(RuinmatesHub::MedicalId()))
		{
			if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(Pawn); Character && Character->GetStats())
			{
				const float Amount=Hub->Definition ? Hub->Definition->MedicalHealAmount : 25.f;
				if (FMath::IsFinite(Amount) && Amount>0.f) Character->GetStats()->ChangeHealthByAmount(Amount, this);
			}
		}
		else { Component->Client_HubResult(false, FText::FromString(TEXT("医疗室需要解锁并恢复供电"))); }
		return;
	}
	if (Type == ERuinmatesHubTerminalType::Workshop || Type == ERuinmatesHubTerminalType::Elevator)
	{
		const FName Id = Type == ERuinmatesHubTerminalType::Workshop ? RuinmatesHub::WorkshopId() : RuinmatesHub::ElevatorId();
		const FText Message = !Hub->IsFacilityOperational(Id) ? FText::FromString(TEXT("设施需要解锁并恢复供电")) :
			FText::FromString(Type == ERuinmatesHubTerminalType::Elevator ? TEXT("当前仅开放一层，二三层尚未开放") : TEXT("工作区已恢复；维修服务等待配置"));
		Component->Client_OpenHub(this); Component->Client_HubResult(true, Message);
		return;
	}
	if (Type == ERuinmatesHubTerminalType::Exit)
	{
		if (Hub->IsFacilityOperational(RuinmatesHub::ExitId()) && Hub->Definition && !Hub->Definition->DestinationMap.IsNull())
		{
			auto* Session = GetGameInstance() ? GetGameInstance()->GetSubsystem<URuinmatesSessionSubsystem>() : nullptr;
			if (Session && Hub->SaveProgress())
			{
				Session->DangerMapName = FName(*Hub->Definition->DestinationMap.ToSoftObjectPath().GetLongPackageName());
				Session->BaseMapName = FName(TEXT("/Game/Hub/Maps/L_Hub_Blockout_01"));
				Session->HostListenParty();
				if (Session->TravelToDangerZone()) return;
			}
		}
		Component->Client_OpenHub(this);
		Component->Client_HubResult(false, FText::FromString(Hub->IsFacilityOperational(RuinmatesHub::ExitId()) ?
			TEXT("任务出口已恢复，探索地图尚未配置或旅行失败") : TEXT("任务出口尚未恢复，请使用中央建设终端")));
		return;
	}
	Component->Client_OpenHub(this);
}
