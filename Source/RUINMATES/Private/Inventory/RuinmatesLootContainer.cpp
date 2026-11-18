#include "Inventory/RuinmatesLootContainer.h"

#include "Character/RuinmatesCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
	FRuinmatesItemStack MakeLoot(const FName& Id, const FText& Name, ERuinmatesItemQuality Quality, int32 Value, int32 Count, int32 MaxStack, bool bUpgrade, const FText& Description)
	{
		FRuinmatesItemStack Stack;
		Stack.ItemId = Id;
		Stack.DisplayName = Name;
		Stack.Quality = Quality;
		Stack.UnitValue = Value;
		Stack.Count = Count;
		Stack.MaxStack = MaxStack;
		Stack.bUpgradeMaterial = bUpgrade;
		Stack.Description = Description;
		Stack.bRevealed = false;
		return Stack;
	}
}

ARuinmatesLootContainer::ARuinmatesLootContainer()
{
	bReplicates = true;
	PrimaryActorTick.bCanEverTick = false;

	SearchBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SearchBounds"));
	SetRootComponent(SearchBounds);
	SearchBounds->SetBoxExtent(FVector(40.f, 40.f, 40.f));
	SearchBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SearchBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	SearchBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(SearchBounds);
	Label->SetRelativeLocation(FVector(0.f, 0.f, 70.f));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetText(NSLOCTEXT("RuinMates", "LootLabel", "物资"));
	ContainerName = NSLOCTEXT("RuinMates", "DefaultContainer", "废弃研究所储物箱");
	EnsureDefaultLoot();
}

void ARuinmatesLootContainer::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && AuthoredLoot.Num() > 0)
	{
		Contents = AuthoredLoot;
		for (FRuinmatesItemStack& Stack : Contents)
		{
			Stack.bRevealed = false;
		}
	}
	else if (HasAuthority())
	{
		EnsureDefaultLoot();
	}
	if (IsValid(Label))
	{
		Label->SetText(ContainerName);
	}
}

void ARuinmatesLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesLootContainer, Contents);
}

bool ARuinmatesLootContainer::CanInteract_Implementation(APawn* InteractingPawn) const
{
	return IsValid(InteractingPawn) && FVector::Dist(GetActorLocation(), InteractingPawn->GetActorLocation()) <= CloseDistance;
}

FText ARuinmatesLootContainer::GetInteractPrompt_Implementation(APawn* InteractingPawn) const
{
	if (!CanInteract_Implementation(InteractingPawn))
	{
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("RuinMates", "SearchPrompt", "按 F 搜索 {0}"), ContainerName);
}

void ARuinmatesLootContainer::Interact_Implementation(APawn* InteractingPawn)
{
	if (!HasAuthority() || !CanInteract_Implementation(InteractingPawn))
	{
		return;
	}

	Searchers.AddUnique(InteractingPawn);
	if (ARuinmatesCharacter* Character = Cast<ARuinmatesCharacter>(InteractingPawn))
	{
		Character->BeginSearch(this);
	}
	StartRevealTimer();
}

int32 ARuinmatesLootContainer::GetRevealedCount() const
{
	int32 Revealed = 0;
	for (const FRuinmatesItemStack& Stack : Contents)
	{
		if (!Stack.IsEmpty() && Stack.bRevealed)
		{
			++Revealed;
		}
	}
	return Revealed;
}

void ARuinmatesLootContainer::StopSearch(APawn* Pawn)
{
	if (!HasAuthority())
	{
		return;
	}
	Searchers.Remove(Pawn);
	if (Searchers.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RevealTimer);
		}
	}
}

int32 ARuinmatesLootContainer::GiveTo(APawn* Pawn, int32 SlotIndex)
{
	if (!HasAuthority() || !IsValid(Pawn) || !Contents.IsValidIndex(SlotIndex))
	{
		return 0;
	}
	FRuinmatesItemStack& Stack = Contents[SlotIndex];
	if (Stack.IsEmpty() || !Stack.bRevealed)
	{
		return 0;
	}
	URuinmatesBackpackComponent* Backpack = Pawn->FindComponentByClass<URuinmatesBackpackComponent>();
	if (!Backpack)
	{
		return 0;
	}
	const int32 Accepted = Backpack->TryAdd(Stack);
	if (Accepted <= 0)
	{
		return 0;
	}
	Stack.Count -= Accepted;
	if (Stack.Count <= 0)
	{
		Stack = FRuinmatesItemStack();
	}
	OnChanged.Broadcast();
	return Accepted;
}

int32 ARuinmatesLootContainer::GiveAllRevealed(APawn* Pawn)
{
	if (!HasAuthority())
	{
		return 0;
	}
	int32 Moved = 0;
	for (int32 Index = 0; Index < Contents.Num(); ++Index)
	{
		Moved += GiveTo(Pawn, Index) > 0 ? 1 : 0;
	}
	return Moved;
}

void ARuinmatesLootContainer::OnRep_Contents()
{
	OnChanged.Broadcast();
}

void ARuinmatesLootContainer::EnsureDefaultLoot()
{
	if (Contents.Num() > 0)
	{
		return;
	}

	Contents.Add(MakeLoot(TEXT("Bandage"), NSLOCTEXT("RuinMates", "LootBandage", "绷带"), ERuinmatesItemQuality::Common, 120, 2, 5, false, NSLOCTEXT("RuinMates", "LootBandageDesc", "止血用的旧绷带。")));
	Contents.Add(MakeLoot(TEXT("Cans"), NSLOCTEXT("RuinMates", "LootCans", "罐头"), ERuinmatesItemQuality::Common, 80, 3, 5, false, NSLOCTEXT("RuinMates", "LootCansDesc", "还能吃的应急口粮。")));
	Contents.Add(MakeLoot(TEXT("AmmoBox"), NSLOCTEXT("RuinMates", "LootAmmo", "弹药箱"), ERuinmatesItemQuality::Fine, 480, 1, 1, false, NSLOCTEXT("RuinMates", "LootAmmoDesc", "没有开封的手枪弹。")));
	Contents.Add(MakeLoot(TEXT("Circuit"), NSLOCTEXT("RuinMates", "LootCircuit", "电路板"), ERuinmatesItemQuality::Advanced, 1600, 1, 1, true, NSLOCTEXT("RuinMates", "LootCircuitDesc", "可以拿去升级基地的零件。")));
	Contents.Add(MakeLoot(TEXT("Gland"), NSLOCTEXT("RuinMates", "LootGland", "异种腺体"), ERuinmatesItemQuality::Rare, 6400, 1, 1, true, NSLOCTEXT("RuinMates", "LootGlandDesc", "研究所留下的样本，不是只拿去卖的。")));
	Contents.Add(MakeLoot(TEXT("Core"), NSLOCTEXT("RuinMates", "LootCore", "核心碎片"), ERuinmatesItemQuality::Precious, 18000, 1, 1, true, NSLOCTEXT("RuinMates", "LootCoreDesc", "打开高级装备锁需要的碎片。")));
}

void ARuinmatesLootContainer::StartRevealTimer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimerManager().IsTimerActive(RevealTimer))
	{
		return;
	}
	World->GetTimerManager().SetTimer(RevealTimer, this, &ARuinmatesLootContainer::RevealNext, RevealInterval, true, 0.15f);
}

void ARuinmatesLootContainer::RevealNext()
{
	if (!HasAuthority())
	{
		return;
	}
	for (FRuinmatesItemStack& Stack : Contents)
	{
		if (!Stack.IsEmpty() && !Stack.bRevealed)
		{
			Stack.bRevealed = true;
			OnChanged.Broadcast();
			return;
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimer);
	}
}
