#include "Inventory/RuinmatesMerchant.h"

#include "Character/RuinmatesCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"

ARuinmatesMerchant::ARuinmatesMerchant()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(true);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(Body);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetText(NSLOCTEXT("RuinMates", "MerchantLabel", "商人"));

	MerchantName = NSLOCTEXT("RuinMates", "DefaultMerchant", "补给商人");
}

bool ARuinmatesMerchant::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn)
		&& FVector::Dist(GetActorLocation(), InteractingPawn->GetActorLocation()) <= InteractionDistance;
}

FText ARuinmatesMerchant::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RuinMates", "MerchantPrompt", "按 F 与 {0} 交易"), MerchantName);
}

void ARuinmatesMerchant::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}
	if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(InteractingPawn))
	{
		Character->BeginTrade(this);
	}
}
