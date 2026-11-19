#include "Inventory/RuinmatesLootContainer.h"

#include "Character/RuinmatesCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
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
	// Default loot prefers the item database and only falls back to the literal template when the
	// database or the row is missing (e.g. bare automation worlds without a game instance).
	FRuinmatesItemStack MakeLootFromDatabase(
		const UObject* WorldContext,
		const FName& Id,
		const FText& Name,
		ERuinmatesItemQuality Quality,
		int32 Value,
		int32 Count,
		int32 MaxStack,
		bool bUpgrade,
		const FText& Description)
	{
		if (const URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(WorldContext))
		{
			if (Database->IsValidItemId(Id))
			{
				FRuinmatesItemStack Stack = Database->CreateItemStack(Id, Count);
				Stack.bRevealed = false;
				return Stack;
			}
		}
		return MakeLoot(Id, Name, Quality, Value, Count, MaxStack, bUpgrade, Description);
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
}

void ARuinmatesLootContainer::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (!LootPool.IsEmpty())
        {
            GenerateLoot();
        }
        else if (!AuthoredLoot.IsEmpty())
        {
			NormalizeAuthoredLoot();
        }
        else
        {
            EnsureDefaultLoot();
        }

        for (FRuinmatesItemStack& Stack : Contents)
        {
            Stack.bRevealed = false;
        }
		RefreshPublicContents();
    }

    if (IsValid(Label))
    {
        Label->SetText(ContainerName);
    }
}

void ARuinmatesLootContainer::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARuinmatesLootContainer, PublicContents);
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
	RefreshPublicContents();
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

void ARuinmatesLootContainer::NormalizeAuthoredLoot()
{
	Contents.Reset();
	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	for (FRuinmatesItemStack Stack : AuthoredLoot)
	{
		if (Database && Database->IsLoaded())
		{
			if (!Database->NormalizeStack(Stack, true))
			{
				UE_LOG(LogTemp, Warning, TEXT("RuinMates authored loot item could not be resolved in DT_Items: %s"), *Stack.ItemId.ToString());
				continue;
			}
			const int32 StackMax = FMath::Max(1, Stack.MaxStack);
			int32 Remaining = Stack.Count;
			while (Remaining > 0)
			{
				FRuinmatesItemStack Chunk = Stack;
				Chunk.Count = FMath::Min(StackMax, Remaining);
				Chunk.bRevealed = false;
				Contents.Add(Chunk);
				Remaining -= Chunk.Count;
			}
		}
		else if (!Stack.IsEmpty())
		{
			Stack.Count = FMath::Clamp(Stack.Count, 1, FMath::Max(1, Stack.MaxStack));
			Stack.bRevealed = false;
			Contents.Add(Stack);
		}
	}
}

void ARuinmatesLootContainer::RefreshPublicContents()
{
	PublicContents.SetNum(Contents.Num());
	for (int32 Index = 0; Index < Contents.Num(); ++Index)
	{
		const FRuinmatesItemStack& ServerStack = Contents[Index];
		if (ServerStack.IsEmpty())
		{
			PublicContents[Index] = FRuinmatesItemStack();
			PublicContents[Index].bRevealed = true;
		}
		else if (ServerStack.bRevealed)
		{
			PublicContents[Index] = ServerStack;
		}
		else
		{
			PublicContents[Index] = FRuinmatesItemStack();
			PublicContents[Index].bRevealed = false;
		}
	}
}

void ARuinmatesLootContainer::EnsureDefaultLoot()
{
	if (Contents.Num() > 0)
	{
		return;
	}

	Contents.Add(MakeLootFromDatabase(this, TEXT("Bandage"), NSLOCTEXT("RuinMates", "LootBandage", "绷带"), ERuinmatesItemQuality::Common, 120, 2, 5, false, NSLOCTEXT("RuinMates", "LootBandageDesc", "止血用的旧绷带。")));
	Contents.Add(MakeLootFromDatabase(this, TEXT("Cans"), NSLOCTEXT("RuinMates", "LootCans", "罐头"), ERuinmatesItemQuality::Common, 80, 3, 5, false, NSLOCTEXT("RuinMates", "LootCansDesc", "还能吃的应急口粮。")));
	Contents.Add(MakeLootFromDatabase(this, TEXT("AmmoBox"), NSLOCTEXT("RuinMates", "LootAmmo", "弹药箱"), ERuinmatesItemQuality::Fine, 480, 1, 1, false, NSLOCTEXT("RuinMates", "LootAmmoDesc", "没有开封的手枪弹。")));
	Contents.Add(MakeLootFromDatabase(this, TEXT("Circuit"), NSLOCTEXT("RuinMates", "LootCircuit", "电路板"), ERuinmatesItemQuality::Advanced, 1600, 1, 1, true, NSLOCTEXT("RuinMates", "LootCircuitDesc", "可以拿去升级基地的零件。")));
	Contents.Add(MakeLootFromDatabase(this, TEXT("Gland"), NSLOCTEXT("RuinMates", "LootGland", "异种腺体"), ERuinmatesItemQuality::Rare, 6400, 1, 1, true, NSLOCTEXT("RuinMates", "LootGlandDesc", "研究所留下的样本，不是只拿去卖的。")));
	Contents.Add(MakeLootFromDatabase(this, TEXT("Core"), NSLOCTEXT("RuinMates", "LootCore", "核心碎片"), ERuinmatesItemQuality::Precious, 18000, 1, 1, true, NSLOCTEXT("RuinMates", "LootCoreDesc", "打开高级装备锁需要的碎片。")));
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
			RefreshPublicContents();
			OnChanged.Broadcast();
			return;
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RevealTimer);
	}
}

void ARuinmatesLootContainer::GenerateLoot()
{
	if(!HasAuthority())
	{
		return;
	}
	if(LootPool.IsEmpty())
	{
		return;
	}
	const int32 LootCount = FMath::RandRange(FMath::Max(0,MinLootItems),MaxLootItems);
	TArray<FRuinmatesLootEntry> AvailablePool;
	URuinmatesItemDatabaseSubsystem* Database = URuinmatesItemDatabaseSubsystem::Get(this);
	const bool bHasDatabase = Database && Database->IsLoaded();
	for (const FRuinmatesLootEntry& Entry : LootPool)
	{
		FName EntryItemId = !Entry.ItemId.IsNone() ? Entry.ItemId : Entry.Item.ItemId;
		if (Entry.Weight <= 0 || EntryItemId.IsNone())
		{
			continue;
		}
		if (bHasDatabase)
		{
			EntryItemId = Database->ResolveItemId(EntryItemId, Entry.Item.DisplayName);
			if (EntryItemId.IsNone())
			{
				UE_LOG(LogTemp, Warning, TEXT("RuinMates loot pool item could not be resolved in DT_Items: %s"), *Entry.ItemId.ToString());
				continue;
			}
		}
		AvailablePool.Add(Entry);
	}

	for(int32 i = 0;i< LootCount;++i)
	{
		if(AvailablePool.IsEmpty())
		{
			break;
		}

		int32 TotalWeight = 0;

		for (const FRuinmatesLootEntry& Entry : AvailablePool)
		{
			if (Entry.Weight > 0)
			{
				TotalWeight += Entry.Weight;
			}
		}

		if(TotalWeight <= 0)
		{
			break;
		}

		const int32 Roll = FMath::RandRange(1,TotalWeight);

		int32 CurrentWeight = 0;
		int32 SelectedIndex = INDEX_NONE;

		for(int32 j = 0; j < AvailablePool.Num();++j)
		{
			const FRuinmatesLootEntry& Entry = AvailablePool[j];

			if (Entry.Weight <= 0)
			{
				continue;
			}
			
			CurrentWeight += Entry.Weight;
			
			if (Roll <= CurrentWeight)
			{
				SelectedIndex = j;
				break;
			}
		}

		if (AvailablePool.IsValidIndex(SelectedIndex))
        {
            const FRuinmatesLootEntry& Entry =
                AvailablePool[SelectedIndex];

            // Prefer the authoritative database; fall back to the legacy embedded template so
            // containers authored before the database keep generating the same items.
            FName EntryItemId = !Entry.ItemId.IsNone() ? Entry.ItemId : Entry.Item.ItemId;
            FRuinmatesItemStack Stack;

			if (bHasDatabase)
            {
				EntryItemId = Database->ResolveItemId(EntryItemId, Entry.Item.DisplayName);
                const int32 StackMax = FMath::Max(1, Database->GetMaxStack(EntryItemId));
                const int32 MinCount = FMath::Clamp(Entry.MinCount, 1, StackMax);
                const int32 MaxCount = FMath::Clamp(Entry.MaxCount, MinCount, StackMax);
                Stack = Database->CreateItemStack(EntryItemId, FMath::RandRange(MinCount, MaxCount));
            }
            else
            {
                Stack = Entry.Item;

                const int32 MaxStack =
                    FMath::Max(1, Stack.MaxStack);

                const int32 MinCount =
                    FMath::Clamp(Entry.MinCount, 1, MaxStack);

                const int32 MaxCount =
                    FMath::Clamp(Entry.MaxCount, MinCount, MaxStack);

                Stack.MaxStack = MaxStack;
                Stack.Count = FMath::RandRange(MinCount, MaxCount);
            }

            if (Stack.IsEmpty())
            {
                AvailablePool.RemoveAt(SelectedIndex);
                continue;
            }

            Stack.bRevealed = false;

            Contents.Add(Stack);

            // 防止同一个掉落项被重复抽取
            AvailablePool.RemoveAt(SelectedIndex);
        }
    }
	RefreshPublicContents();
    OnChanged.Broadcast();
}
