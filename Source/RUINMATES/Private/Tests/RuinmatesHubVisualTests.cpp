#include "Misc/AutomationTest.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Hub/RuinmatesHubVisualStateActor.h"
#include "NiagaraComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Transient physics world; its engine context avoids misleading teardown warnings. */
	struct FHubTestWorld
	{
		UWorld* World = nullptr;

		FHubTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None);
			if (World)
			{
				World->AddToRoot();
				if (GEngine)
				{
					GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
				}
				// Actor ProcessEvent (including dynamic lifecycle delegates) is gated until
				// InitializeActorsForPlay. Do not substitute DispatchBeginPlay on one actor
				// for initialization of the world containing that actor.
				World->InitializeActorsForPlay(FURL());
			}
		}

		~FHubTestWorld()
		{
			if (World)
			{
				World->DestroyWorld(false);
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
				World->RemoveFromRoot();
			}
		}

		template <typename T>
		T* Spawn(const FVector& Location = FVector::ZeroVector)
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Parameters.ObjectFlags |= RF_Transient;
			return World ? World->SpawnActor<T>(Location, FRotator::ZeroRotator, Parameters) : nullptr;
		}
	};

	/** Actual query collision, without any content asset dependency. */
	UBoxComponent* AddBox(AActor* Actor, ECollisionEnabled::Type Collision, const FVector& Location = FVector::ZeroVector)
	{
		if (!Actor)
		{
			return nullptr;
		}
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Box->SetBoxExtent(FVector(40.f));
		Box->SetCanEverAffectNavigation(false);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Ignore);
		Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Box->SetCollisionEnabled(Collision);
		Actor->AddInstanceComponent(Box);
		Actor->SetRootComponent(Box);
		Box->RegisterComponent();
		Actor->SetActorLocation(Location);
		Box->UpdateBounds();
		Box->UpdateComponentToWorld();
		return Box;
	}

	/** Ray through an isolated target; identifies whether collision really remains in the scene. */
	bool HitsActor(UWorld* World, AActor* Target)
	{
		FHitResult Hit;
		const FVector Location = Target->GetActorLocation();
		return World->LineTraceSingleByChannel(Hit, Location + FVector(0,0,100),
			Location - FVector(0,0,100), ECC_Visibility) && Hit.GetActor() == Target;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubAuthorityStateTest, "RuinMates.Hub.StateAuthorityAndScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubAuthorityStateTest::RunTest(const FString& Parameters)
{
	FHubTestWorld Fixture;
	ARuinmatesHubStateActor* Source = Fixture.Spawn<ARuinmatesHubStateActor>();
	if (!TestNotNull(TEXT("Transient Hub source exists"), Source))
	{
		return false;
	}
	TestTrue(TEXT("Fixture world initializes the real actor lifecycle"), Fixture.World->AreActorsInitialized());
	TestFalse(TEXT("No per-frame state tick"), Source->PrimaryActorTick.bCanEverTick);
	TestFalse(TEXT("Uninitialized snapshot is gated"), Source->IsHubInitialized());
	TestTrue(TEXT("Authority initializes defaults"), Source->InitializeDefaultState());
	FRuinmatesHubRoomState Core;
	FRuinmatesHubRoomState Medical;
	TestTrue(TEXT("Core ID resolves"), Source->GetRoomState(RuinmatesHub::CoreId(), Core));
	TestTrue(TEXT("Medical ID resolves"), Source->GetRoomState(RuinmatesHub::MedicalId(), Medical));
	TestTrue(TEXT("Only Core starts unlocked and unblocked"), Core.Permanent.bUnlocked && !Core.Permanent.bStructureBlocked);
	TestTrue(TEXT("Medical starts blocked, locked, level zero"), Medical.Permanent.bStructureBlocked
		&& !Medical.Permanent.bUnlocked && Medical.Permanent.Level == 0);
	TestEqual(TEXT("Core max is two"), Source->GetRoomMaxLevel(Core.RoomId), 2);
	TestEqual(TEXT("Medical max is one"), Source->GetRoomMaxLevel(Medical.RoomId), 1);
	FRuinmatesHubRoomState Unknown;
	TestFalse(TEXT("Unknown room IDs do not resolve"), Source->GetRoomState(TEXT("Hub.Unknown"), Unknown));

	int32 Changes = 0;
	const FDelegateHandle Handle = Source->OnRoomStateChanged.AddLambda([&Changes](FName) { ++Changes; });
	TestTrue(TEXT("Identical snapshot is accepted"), Source->CommitRoomState(Medical));
	TestEqual(TEXT("Identical snapshot emits no duplicate event"), Changes, 0);
	Medical.Permanent.Level = 2;
	TestFalse(TEXT("Medical level two is rejected"), Source->CommitRoomState(Medical));
	Core.Permanent.Level = 3;
	TestFalse(TEXT("No level three"), Source->CommitRoomState(Core));
	Medical.Permanent.Level = 1;
	Medical.Runtime.bOperational = true;
	TestFalse(TEXT("Blocked locked service cannot operate"), Source->CommitRoomState(Medical));
	Medical.Runtime.bOperational = false;
	Source->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("Client role cannot commit construction facts"), Source->CommitRoomState(Medical));
	Source->SetRole(ROLE_Authority);
	Source->OnRoomStateChanged.Remove(Handle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubVisualCollisionTest, "RuinMates.Hub.VisualCollisionAndPowerSeparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubVisualCollisionTest::RunTest(const FString& Parameters)
{
	FHubTestWorld Fixture;
	ARuinmatesHubStateActor* Source = Fixture.Spawn<ARuinmatesHubStateActor>();
	ARuinmatesHubVisualStateActor* Visual = Fixture.Spawn<ARuinmatesHubVisualStateActor>();
	AActor* Debris = Fixture.Spawn<AActor>();
	AActor* Repaired = Fixture.Spawn<AActor>(FVector(200,0,0));
	AActor* Device = Fixture.Spawn<AActor>(FVector(400,0,0));
	UBoxComponent* DebrisBox = AddBox(Debris, ECollisionEnabled::QueryOnly);
	UBoxComponent* RepairedBox = AddBox(Repaired, ECollisionEnabled::NoCollision, FVector(200,0,0));
	UBoxComponent* DeviceBox = AddBox(Device, ECollisionEnabled::QueryOnly, FVector(400,0,0));
	if (!TestTrue(TEXT("State, adapter and asset-free boxes exist"), Source && Visual && DebrisBox && RepairedBox && DeviceBox))
	{
		return false;
	}
	DeviceBox->PrimaryComponentTick.bCanEverTick = true;
	DeviceBox->SetComponentTickEnabled(true);
	const bool bAuthoredDeviceTick = DeviceBox->IsComponentTickEnabled();
	UPointLightComponent* Light = NewObject<UPointLightComponent>(Device);
	Device->AddInstanceComponent(Light);
	Light->SetupAttachment(DeviceBox);
	Light->RegisterComponent();
	Light->SetVisibility(true);
	UAudioComponent* Audio = NewObject<UAudioComponent>(Device);
	Device->AddInstanceComponent(Audio);
	Audio->SetAutoActivate(false);
	Audio->RegisterComponent();
	UNiagaraComponent* Niagara = NewObject<UNiagaraComponent>(Device);
	Device->AddInstanceComponent(Niagara);
	Niagara->SetAutoActivate(false);
	Niagara->RegisterComponent();

	FRuinmatesHubVisualBinding Rubble;
	Rubble.Actors = { Debris, nullptr };
	Rubble.Cleared = ERuinmatesHubStateRequirement::MustBeFalse;
	FRuinmatesHubVisualBinding Restored;
	Restored.Actors = { Repaired };
	Restored.Cleared = ERuinmatesHubStateRequirement::MustBeTrue;
	FRuinmatesHubVisualBinding Running;
	Running.Actors = { Device };
	Running.Domain = ERuinmatesHubVisualDomain::Runtime;
	Running.bRequiresOperational = true;
	Running.bHideWhenInactive = false;
	Running.bDisableCollisionWhenInactive = false;
	Visual->RoomId = RuinmatesHub::MedicalId();
	Visual->Bindings = { Rubble, Restored, Running };
	Visual->BindStateSource(Source);
	TestFalse(TEXT("Uninitialized groups have no stale rubble collision"), HitsActor(Fixture.World, Debris));
	TestTrue(TEXT("Authority supplies initial state"), Source->InitializeDefaultState());
	TestTrue(TEXT("Uncleared debris is actually traceable"), HitsActor(Fixture.World, Debris));
	TestTrue(TEXT("Repair props hidden before clearing"), Repaired->IsHidden());
	TestFalse(TEXT("No visual adapter tick"), Visual->PrimaryActorTick.bCanEverTick);

	FRuinmatesHubRoomState State;
	Source->GetRoomState(Visual->RoomId, State);
	State.Permanent.bCleared = true;
	State.Permanent.bStructureBlocked = false;
	State.Permanent.bUnlocked = true;
	State.Permanent.Level = 1;
	State.Runtime.bPowered = true;
	State.Runtime.bOperational = true;
	TestTrue(TEXT("Authority commits restored powered room"), Source->CommitRoomState(State));
	TestTrue(TEXT("Cleared debris is hidden"), Debris->IsHidden());
	TestFalse(TEXT("Cleared debris has no invisible collision"), HitsActor(Fixture.World, Debris));
	TestEqual(TEXT("Hidden primitive collision is disabled"), DebrisBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Restored decoration is visible"), Repaired->IsHidden());
	TestEqual(TEXT("Authored no-collision is preserved"), RepairedBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Powered light restores authored visibility"), Light->GetVisibleFlag());
	TestEqual(TEXT("Powered equipment preserves authored component tick"), DeviceBox->IsComponentTickEnabled(), bAuthoredDeviceTick);
	const int32 ActorCount = Fixture.World->GetActorCount();
	const int32 ComponentCount = Device->GetComponents().Num();
	Visual->RefreshFromSource();
	Visual->RefreshFromSource();
	TestEqual(TEXT("Repeated application creates no actor"), Fixture.World->GetActorCount(), ActorCount);
	TestEqual(TEXT("Repeated application creates no component"), Device->GetComponents().Num(), ComponentCount);
	TestFalse(TEXT("Null sound remains stopped"), Audio->IsPlaying());
	TestFalse(TEXT("Null Niagara remains inactive"), Niagara->IsActive());

	State.Runtime.bPowered = false;
	State.Runtime.bOperational = false;
	TestTrue(TEXT("Outage commits independently"), Source->CommitRoomState(State));
	TestFalse(TEXT("Outage shuts normal lighting down"), Light->GetVisibleFlag());
	TestFalse(TEXT("Outage suspends equipment component tick"), DeviceBox->IsComponentTickEnabled());
	TestFalse(TEXT("Unpowered equipment body remains visible"), Device->IsHidden());
	TestTrue(TEXT("Unpowered stationary equipment retains collision"), HitsActor(Fixture.World, Device));
	TestFalse(TEXT("Outage does not resurrect debris"), HitsActor(Fixture.World, Debris));
	TestFalse(TEXT("Outage does not undo repaired geometry"), Repaired->IsHidden());
	FRuinmatesHubRoomState AfterOutage;
	Source->GetRoomState(State.RoomId, AfterOutage);
	TestTrue(TEXT("Permanent facts survived outage"), AfterOutage.Permanent == State.Permanent);

	// Late registration reads the current state without waiting for another change event.
	ARuinmatesHubVisualStateActor* Late = Fixture.Spawn<ARuinmatesHubVisualStateActor>();
	if (TestNotNull(TEXT("Late adapter exists"), Late))
	{
		Late->RoomId = State.RoomId;
		Late->BindStateSource(Source);
	}
	TestTrue(TEXT("Authority source destruction succeeds"), Source->Destroy());
	TestTrue(TEXT("Destroyed source suppresses repaired group without manual refresh"), Repaired->IsHidden());
	TestFalse(TEXT("Destroyed source disables device lighting without manual refresh"), Light->GetVisibleFlag());
	Debris->Destroy();
	Visual->RefreshBindings();
	Visual->RefreshFromSource();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubVisualLateSourceTest, "RuinMates.Hub.VisualLateStateSpawn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubVisualLateSourceTest::RunTest(const FString& Parameters)
{
	FHubTestWorld Fixture;
	ARuinmatesHubVisualStateActor* Visual = Fixture.Spawn<ARuinmatesHubVisualStateActor>();
	AActor* Debris = Fixture.Spawn<AActor>();
	UBoxComponent* Box = AddBox(Debris, ECollisionEnabled::QueryOnly);
	if (!TestTrue(TEXT("Visual and actual blocking geometry exist before the source"), Visual && Box))
	{
		return false;
	}
	FRuinmatesHubVisualBinding Rubble;
	Rubble.Actors = { Debris };
	Rubble.Cleared = ERuinmatesHubStateRequirement::MustBeFalse;
	Visual->RoomId = RuinmatesHub::MedicalId();
	Visual->Bindings = { Rubble };
	// Reproduce the client ordering: visual BeginPlay first, no source in the world.
	Visual->DispatchBeginPlay();
	Visual->BindStateSource(nullptr);
	TestFalse(TEXT("Visual starts without a source"), IsValid(Visual->StateSource));
	TestTrue(TEXT("Uninitialized late-source group stays hidden"), Debris->IsHidden());
	TestFalse(TEXT("Uninitialized late-source group cannot block"), HitsActor(Fixture.World, Debris));

	ARuinmatesHubStateActor* Source = Fixture.Spawn<ARuinmatesHubStateActor>();
	if (!TestNotNull(TEXT("Source arrives after visual BeginPlay"), Source))
	{
		return false;
	}
	TestTrue(TEXT("World spawn event automatically bound the late source"), Visual->StateSource == Source);
	TestFalse(TEXT("Source has not initialized its snapshot yet"), Source->IsHubInitialized());
	TestTrue(TEXT("Group remains hidden until the snapshot is ready"), Debris->IsHidden());
	TestTrue(TEXT("Late source initializes authority snapshot"), Source->InitializeDefaultState());
	TestFalse(TEXT("Initialization event restores the visible group without a manual refresh"), Debris->IsHidden());
	TestTrue(TEXT("Initialization event restores actual authored collision"), HitsActor(Fixture.World, Debris));
	TestFalse(TEXT("Late discovery requires no visual Tick"), Visual->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("Late authority source destruction succeeds"), Source->Destroy());
	TestFalse(TEXT("Destroyed source clears the subscription automatically"), IsValid(Visual->StateSource));
	TestTrue(TEXT("Source destruction hides its group immediately"), Debris->IsHidden());
	TestFalse(TEXT("Source destruction removes stale collision immediately"), HitsActor(Fixture.World, Debris));
	ARuinmatesHubStateActor* Replacement = Fixture.Spawn<ARuinmatesHubStateActor>();
	if (!TestNotNull(TEXT("Replacement source arrives later"), Replacement))
	{
		return false;
	}
	TestTrue(TEXT("Replacement automatically binds"), Visual->StateSource == Replacement);
	TestTrue(TEXT("Replacement supplies initial snapshot"), Replacement->InitializeDefaultState());
	TestFalse(TEXT("Replacement notification restores the group"), Debris->IsHidden());
	TestTrue(TEXT("Replacement restores real collision"), HitsActor(Fixture.World, Debris));
	Visual->Destroy();
	Replacement->Destroy();
	// The visual's EndPlay must remove its world subscription before another source appears.
	TestNotNull(TEXT("A source can spawn after visual teardown"), Fixture.Spawn<ARuinmatesHubStateActor>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesHubVisualDynamicComponentsTest, "RuinMates.Hub.VisualLateComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesHubVisualDynamicComponentsTest::RunTest(const FString& Parameters)
{
	FHubTestWorld Fixture;
	ARuinmatesHubStateActor* Source = Fixture.Spawn<ARuinmatesHubStateActor>();
	ARuinmatesHubVisualStateActor* Visual = Fixture.Spawn<ARuinmatesHubVisualStateActor>();
	AActor* Device = Fixture.Spawn<AActor>();
	UBoxComponent* Root = AddBox(Device, ECollisionEnabled::NoCollision);
	if (!TestTrue(TEXT("Asset-free dynamic component fixture exists"), Source && Visual && Root))
	{
		return false;
	}
	Source->InitializeDefaultState();
	FRuinmatesHubVisualBinding Running;
	Running.Actors = { Device };
	Running.Domain = ERuinmatesHubVisualDomain::Runtime;
	Visual->RoomId = RuinmatesHub::CoreId();
	Visual->Bindings = { Running };
	Visual->BindStateSource(Source);
	TestTrue(TEXT("Device starts hidden without electrical supply"), Device->IsHidden());

	// Add components after the original actor baseline exists. RefreshBindings itself must
	// capture them and apply the current gate; a manual RefreshFromSource would mask the bug.
	UPointLightComponent* Light = NewObject<UPointLightComponent>(Device);
	Device->AddInstanceComponent(Light);
	Light->SetupAttachment(Root);
	Light->RegisterComponent();
	Light->SetVisibility(true);
	UBoxComponent* DynamicBox = NewObject<UBoxComponent>(Device);
	Device->AddInstanceComponent(DynamicBox);
	DynamicBox->SetupAttachment(Root);
	DynamicBox->SetBoxExtent(FVector(20.f));
	DynamicBox->SetCanEverAffectNavigation(false);
	DynamicBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DynamicBox->RegisterComponent();
	DynamicBox->PrimaryComponentTick.bCanEverTick = true;
	DynamicBox->SetComponentTickEnabled(true);
	const bool bAuthoredTick = DynamicBox->IsComponentTickEnabled();
	UAudioComponent* Audio = NewObject<UAudioComponent>(Device);
	Device->AddInstanceComponent(Audio);
	Audio->SetAutoActivate(false);
	Audio->RegisterComponent();
	Audio->bAutoActivate = true;
	UNiagaraComponent* Niagara = NewObject<UNiagaraComponent>(Device);
	Device->AddInstanceComponent(Niagara);
	Niagara->SetAutoActivate(false);
	Niagara->RegisterComponent();
	Niagara->bAutoActivate = true;
	Visual->RefreshBindings();
	TestFalse(TEXT("Explicit binding refresh stops a newly added light"), Light->GetVisibleFlag());
	TestEqual(TEXT("Explicit binding refresh removes new primitive collision"), DynamicBox->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Explicit binding refresh suspends new component tick"), DynamicBox->IsComponentTickEnabled());
	TestFalse(TEXT("Explicit binding refresh suppresses late audio auto-activation"), Audio->bAutoActivate);
	TestFalse(TEXT("Explicit binding refresh suppresses late Niagara auto-activation"), Niagara->bAutoActivate);
	const int32 Count = Device->GetComponents().Num();
	Visual->RefreshBindings();
	TestEqual(TEXT("Repeated refresh creates no component"), Device->GetComponents().Num(), Count);

	FRuinmatesHubRoomState State;
	Source->GetRoomState(Visual->RoomId, State);
	State.Runtime.bPowered = true;
	Source->CommitRoomState(State);
	TestTrue(TEXT("Late light restores its first authored baseline"), Light->GetVisibleFlag());
	TestEqual(TEXT("Late collision restores QueryOnly after repeated refresh"), DynamicBox->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Late component tick restores its baseline"), DynamicBox->IsComponentTickEnabled(), bAuthoredTick);
	TestTrue(TEXT("Late audio auto-activation restores its authored value"), Audio->bAutoActivate);
	TestTrue(TEXT("Late Niagara auto-activation restores its authored value"), Niagara->bAutoActivate);
	TestFalse(TEXT("Missing audio asset is not played"), Audio->IsPlaying());
	TestFalse(TEXT("Missing Niagara asset is not activated"), Niagara->IsActive());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
