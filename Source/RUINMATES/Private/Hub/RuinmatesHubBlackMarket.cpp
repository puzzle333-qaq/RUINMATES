#include "Hub/RuinmatesHubBlackMarket.h"

#include "Character/RuinmatesCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Net/UnrealNetwork.h"
#include "Stats/StatsComponent.h"
#include "TimerManager.h"

ARuinmatesHubBlackMarket::ARuinmatesHubBlackMarket()
{
	bAlwaysRelevant = true;
	MerchantName = FText::FromString(TEXT("无人值守黑市"));
	Label->SetText(FText::GetEmpty());
	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowInteraction"));
	InteractionBox->SetupAttachment(RootComponent);
	InteractionBox->SetBoxExtent(FVector(65,65,70));
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
	MotionAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("ShutterAudio"));
	MotionAudio->SetupAttachment(RootComponent); MotionAudio->bAutoActivate=false;
}

void ARuinmatesHubBlackMarket::BeginPlay()
{
	Super::BeginPlay();
	for (const auto& Leaf : ShutterLeaves) ClosedLocations.Add(IsValid(Leaf) ? Leaf->GetActorLocation() : FVector::ZeroVector);
	ApplyShutter(0.f);
	if (Hub) HubHandle = Hub->OnRoomStateChanged.AddWeakLambda(this,[this](FName){ if (HasAuthority()) CheckSession(); });
	if (HasAuthority()) GetWorldTimerManager().SetTimer(SessionTimer,this,&ThisClass::CheckSession,.25f,true);
}

void ARuinmatesHubBlackMarket::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Hub) Hub->OnRoomStateChanged.Remove(HubHandle);
	if (HasAuthority()) CloseSession();
	GetWorldTimerManager().ClearTimer(SessionTimer); GetWorldTimerManager().ClearTimer(MotionTimer);
	if (MotionAudio) MotionAudio->Stop();
	Super::EndPlay(Reason);
}

void ARuinmatesHubBlackMarket::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesHubBlackMarket, SessionOwner);
	DOREPLIFETIME(ARuinmatesHubBlackMarket, SessionPhase);
}

bool ARuinmatesHubBlackMarket::HasAccess(APawn* Pawn) const
{
	const auto* Character = Cast<ARuinmatesCharacter>(Pawn);
	if (!IsValid(Character) || !Character->GetController() || !Hub || !Hub->IsFacilityOperational(RuinmatesHub::BlackMarketId())
		|| Pawn->GetWorld()!=GetWorld() || !Super::CanInteract_Implementation(Pawn)
		|| (Character->GetStats() && Character->GetStats()->IsDead())) return false;
	FHitResult Hit;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(HubMarketAccess), false, Pawn);
	for (const auto& Leaf : ShutterLeaves) if (IsValid(Leaf)) Query.AddIgnoredActor(Leaf.Get());
	return !GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetPawnViewLocation(),GetActorLocation(),ECC_Visibility,Query)
		|| Hit.GetActor()==this;
}

bool ARuinmatesHubBlackMarket::CanInteract_Implementation(APawn* Pawn) const
{
	return HasAccess(Pawn) && (SessionPhase==0 || (SessionPhase==2 && SessionOwner==Pawn));
}

FText ARuinmatesHubBlackMarket::GetInteractPrompt_Implementation(APawn* Pawn) const
{
	if (!CanInteract_Implementation(Pawn)) return FText::GetEmpty();
	return FText::FromString(SessionPhase==0 ? TEXT("按 F 开启黑市窗口") : TEXT("按 F 与包裹交易"));
}

void ARuinmatesHubBlackMarket::Interact_Implementation(APawn* Pawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(Pawn)) return;
	if (SessionPhase==0)
	{
		SessionOwner = Cast<ARuinmatesCharacter>(Pawn); SessionPhase=1; bTradeStarted=false;
		ForceNetUpdate(); OnRep_Session();
	}
	else if (!bTradeStarted && SessionOwner)
	{
		bTradeStarted=true; SessionOwner->BeginTrade(this);
	}
}

bool ARuinmatesHubBlackMarket::IsTradeAllowed(const ARuinmatesCharacter* Character) const
{
	return Character && SessionOwner==Character && SessionPhase==2 && bTradeStarted
		&& HasAccess(const_cast<ARuinmatesCharacter*>(Character));
}

void ARuinmatesHubBlackMarket::CheckSession()
{
	if (!HasAuthority() || SessionPhase==0 || SessionPhase==3) return;
	if (!IsValid(SessionOwner) || !HasAccess(SessionOwner)
		|| (bTradeStarted && SessionOwner->GetActiveMerchant()!=this)) CloseSession();
}

void ARuinmatesHubBlackMarket::CloseSession()
{
	if (!HasAuthority() || SessionPhase==0 || SessionPhase==3) return;
	if (IsValid(SessionOwner) && SessionOwner->GetActiveMerchant()==this) SessionOwner->EndTrade();
	SessionOwner=nullptr; SessionPhase=3; bTradeStarted=false;
	if (Hub && !Hub->SaveProgress()) UE_LOG(LogTemp,Warning,TEXT("Black market session closed, but campaign save failed"));
	ForceNetUpdate(); OnRep_Session();
}

void ARuinmatesHubBlackMarket::OnRep_Session()
{
	if (!GetWorld()) return;
	GetWorldTimerManager().ClearTimer(MotionTimer);
	if (SessionPhase==0 || SessionPhase==2)
	{ if (MotionAudio) MotionAudio->Stop(); ApplyShutter(SessionPhase==2 ? 1.f : 0.f); return; }
	if (MotionAudio && IsValid(MotionAudio->Sound)) MotionAudio->Play();
	MotionStart=GetWorld()->GetTimeSeconds(); StartAlpha=CurrentAlpha;
	// Shutters are visual-only during motion. They never close across a player's collision capsule.
	GetWorldTimerManager().SetTimer(MotionTimer,this,&ThisClass::AnimateShutter,1.f/30.f,true);
}

void ARuinmatesHubBlackMarket::AnimateShutter()
{
	const float T=FMath::Clamp((GetWorld()->GetTimeSeconds()-MotionStart)/FMath::Max(.1f,MotionDuration),0.f,1.f);
	ApplyShutter(FMath::Lerp(StartAlpha, SessionPhase==1 ? 1.f : 0.f, T));
	if (T>=1.f)
	{
		GetWorldTimerManager().ClearTimer(MotionTimer);
		if (MotionAudio) MotionAudio->Stop();
		if (HasAuthority()) { SessionPhase=SessionPhase==1 ? 2 : 0; ForceNetUpdate(); }
	}
}

void ARuinmatesHubBlackMarket::ApplyShutter(float Alpha)
{
	CurrentAlpha=Alpha;
	for (int32 I=0; I<ShutterLeaves.Num(); ++I)
		if (IsValid(ShutterLeaves[I]) && ClosedLocations.IsValidIndex(I))
		{
			ShutterLeaves[I]->SetActorEnableCollision(false);
			ShutterLeaves[I]->SetActorLocation(ClosedLocations[I]+FVector(0,0,FMath::Max(0.f,LiftHeight)*Alpha));
		}
	if (IsValid(WindowLight))
	{
		TInlineComponentArray<ULightComponent*> Lights(WindowLight);
		for (auto* Light : Lights) Light->SetVisibility(Alpha>.5f && Hub && Hub->IsFacilityOperational(RuinmatesHub::BlackMarketId()));
	}
}
