#include "RuinmatesBuildTestMapCommandlet.h"

#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Game/RuinmatesGameMode.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "Inventory/RuinmatesMerchant.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"

namespace
{
	const TCHAR* TestMapPath = TEXT("/Game/RUINMATES/Maps/Lvl_TestPhase1B");
	const TCHAR* CommonContainerPath = TEXT("/Game/RUINMATES/Blueprint/RuinmatesLootContainer/BP_LootContainer_Common.BP_LootContainer_Common_C");
	const TCHAR* DefaultContainerPath = TEXT("/Game/RUINMATES/Blueprint/RuinmatesLootContainer/BP_RuinmatesLootContainer.BP_RuinmatesLootContainer_C");
	const TCHAR* CubeMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");

	UStaticMesh* LoadCube()
	{
		return LoadObject<UStaticMesh>(nullptr, CubeMeshPath);
	}

	AStaticMeshActor* SpawnBlock(UWorld* World, UStaticMesh* Cube, const FVector& Location,
		const FVector& Scale, const FLinearColor& Color)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
		if (!Actor || !Cube)
		{
			return Actor;
		}
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		Mesh->SetStaticMesh(Cube);
		Mesh->SetMobility(EComponentMobility::Movable);
		Actor->SetActorScale3D(Scale);
		// A dedicated material is not needed; colour the block so regions read at a glance.
		Mesh->SetVectorParameterValueOnMaterials(TEXT("Color"), FVector(Color.R, Color.G, Color.B));
		return Actor;
	}

	ATextRenderActor* SpawnLabel(UWorld* World, const FString& Text, const FVector& Location, float Size = 90.f)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ATextRenderActor* Actor = World->SpawnActor<ATextRenderActor>(Location, FRotator(0.f, 90.f, 0.f), Params);
		if (Actor && Actor->GetTextRender())
		{
			Actor->GetTextRender()->SetText(FText::FromString(Text));
			Actor->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
			Actor->GetTextRender()->SetWorldSize(Size);
			Actor->GetTextRender()->SetTextRenderColor(FColor(120, 226, 244));
		}
		return Actor;
	}

	AActor* SpawnBlueprint(UWorld* World, const TCHAR* ClassPath, const FVector& Location)
	{
		UClass* ActorClass = LoadClass<AActor>(nullptr, ClassPath);
		if (!ActorClass)
		{
			UE_LOG(LogTemp, Error, TEXT("Could not load %s"), ClassPath);
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return World->SpawnActor<AActor>(ActorClass, Location, FRotator::ZeroRotator, Params);
	}

	// Region header + a floor light so the area reads without extra art.
	void MarkRegion(UWorld* World, const FString& Name, const FVector& Center)
	{
		SpawnLabel(World, Name, Center + FVector(0.f, 0.f, 320.f), 110.f);
	}
}

URuinmatesBuildTestMapCommandlet::URuinmatesBuildTestMapCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 URuinmatesBuildTestMapCommandlet::Main(const FString& Params)
{
	UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("Could not create a blank map"));
		return 1;
	}

	UStaticMesh* Cube = LoadCube();

	// The map must run the project's configured game mode and character blueprint: the C++ classes
	// alone spawn a meshless pawn with no input assets, which cannot be used for a functional test.
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
	AGameModeBase* GameModeDefaults = GameModeClass ? Cast<AGameModeBase>(GameModeClass->GetDefaultObject()) : nullptr;
	UClass* PawnClass = GameModeDefaults ? GameModeDefaults->DefaultPawnClass : nullptr;
	const bool bGameModeCompatible = GameModeClass && GameModeClass->IsChildOf(ARuinmatesGameMode::StaticClass());
	const bool bPawnCompatible = PawnClass && PawnClass->IsChildOf(ACharacter::StaticClass());
	ACharacter* PawnDefaults = bPawnCompatible ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
	const bool bPawnHasMesh = PawnDefaults && PawnDefaults->GetMesh() && PawnDefaults->GetMesh()->GetSkeletalMeshAsset();
	UE_LOG(LogTemp, Display, TEXT("candidate game mode=%s parent=%s pawn=%s pawnMesh=%s"),
		GameModeClass ? *GameModeClass->GetName() : TEXT("<missing>"),
		GameModeDefaults ? *GameModeDefaults->GetClass()->GetSuperClass()->GetName() : TEXT("<missing>"),
		PawnClass ? *PawnClass->GetName() : TEXT("<missing>"),
		bPawnHasMesh ? TEXT("yes") : TEXT("no"));

	if (AWorldSettings* Settings = World->GetWorldSettings())
	{
		if (bGameModeCompatible && bPawnCompatible && bPawnHasMesh)
		{
			Settings->DefaultGameMode = GameModeClass;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BP_ThirdPersonGameMode is not a compatible ARuinmatesGameMode with a meshed character; falling back to ARuinmatesGameMode"));
			Settings->DefaultGameMode = ARuinmatesGameMode::StaticClass();
		}
	}

	// Lighting: movable, so PIE does not need a lighting build.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(FVector(0.f, 0.f, 1500.f), FRotator(-50.f, 30.f, 0.f), SpawnParams))
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->GetLightComponent()->SetIntensity(4.f);
	}
	if (ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 600.f), FRotator::ZeroRotator, SpawnParams))
	{
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			SkyComponent->SetIntensity(1.2f);
		}
	}

	// Shared floor for every region (one long strip keeps the regions on the same plane).
	SpawnBlock(World, Cube, FVector(3600.f, 0.f, -60.f), FVector(96.f, 40.f, 0.6f), FLinearColor(0.1f, 0.16f, 0.2f));
	// Low divider walls between regions so the test area reads as distinct zones.
	for (int32 Region = 1; Region <= 6; ++Region)
	{
		SpawnBlock(World, Cube, FVector(Region * 1200.f - 600.f, 0.f, 60.f), FVector(0.6f, 40.f, 1.2f), FLinearColor(0.25f, 0.4f, 0.5f));
	}

	// 1) Spawn / movement.
	MarkRegion(World, TEXT("01 出生与移动"), FVector(0.f, 0.f, 0.f));
	for (int32 Player = 0; Player < 2; ++Player)
	{
		const float Offset = Player == 0 ? -220.f : 220.f;
		World->SpawnActor<APlayerStart>(FVector(0.f, Offset, 120.f), FRotator(0.f, 0.f, 0.f), SpawnParams);
	}
	SpawnLabel(World, TEXT("2x PlayerStart (listen server: 2 players)"), FVector(0.f, 0.f, 200.f), 55.f);

	// 2) Shooting range.
	MarkRegion(World, TEXT("02 武器射击"), FVector(1200.f, 0.f, 0.f));
	// The project has one configured weapon blueprint; place it as a pickup source for the range.
	SpawnBlueprint(World, TEXT("/Game/RUINMATES/Blueprint/Weapon/BP_Glock18.BP_Glock18_C"), FVector(1180.f, 0.f, 70.f));
	SpawnBlueprint(World, TEXT("/Game/RUINMATES/Blueprint/Weapon/BP_Glock18.BP_Glock18_C"), FVector(1180.f, 140.f, 70.f));
	for (int32 Target = 0; Target < 3; ++Target)
	{
		SpawnBlock(World, Cube, FVector(1400.f, (Target - 1) * 400.f, 90.f), FVector(0.8f, 0.8f, 1.8f),
			FLinearColor(0.4f, 0.32f, 0.2f));
	}

	// 3) Ordinary loot containers (built-in default table).
	MarkRegion(World, TEXT("03 普通搜刮"), FVector(2400.f, 0.f, 0.f));
	SpawnBlueprint(World, DefaultContainerPath, FVector(2400.f, -220.f, 60.f));
	SpawnBlueprint(World, DefaultContainerPath, FVector(2400.f, 220.f, 60.f));

	// 4) Multi-quality containers (weighted pool covers all five qualities).
	MarkRegion(World, TEXT("04 多品质搜刮"), FVector(3600.f, 0.f, 0.f));
	SpawnBlueprint(World, CommonContainerPath, FVector(3600.f, 0.f, 60.f));

	// 5) Adjacent containers: shared reveal and player-vs-player pickup checks.
	MarkRegion(World, TEXT("05 相邻容器"), FVector(4800.f, 0.f, 0.f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		SpawnBlueprint(World, CommonContainerPath, FVector(4800.f, (Index - 1) * 180.f, 60.f));
	}

	// 6) Merchant: the C++ actor has no mesh, so give it a visible, traceable block here.
	MarkRegion(World, TEXT("06 商人交易"), FVector(6000.f, 0.f, 0.f));
	if (ARuinmatesMerchant* Merchant = World->SpawnActor<ARuinmatesMerchant>(FVector(6000.f, 0.f, 60.f), FRotator::ZeroRotator, SpawnParams))
	{
		if (UStaticMeshComponent* Body = Merchant->FindComponentByClass<UStaticMeshComponent>())
		{
			Body->SetStaticMesh(Cube);
			Body->SetMobility(EComponentMobility::Movable);
			Merchant->SetActorScale3D(FVector(0.7f, 0.7f, 1.6f));
		}
		// Stock only items the item table already marks as purchasable; no prices are touched.
		Merchant->SetMerchantItems({TEXT("Bandage"), TEXT("Cans"), TEXT("AmmoBox"), TEXT("Circuit")});
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Could not spawn the merchant"));
	}

	// 7) Full bag / stacking: several containers in a row so the bag can be filled quickly.
	MarkRegion(World, TEXT("07 满载与堆叠"), FVector(7200.f, 0.f, 0.f));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SpawnBlueprint(World, CommonContainerPath, FVector(7200.f, (Index - 1.5f) * 220.f, 60.f));
	}

	if (!UEditorLoadingAndSavingUtils::SaveMap(World, TestMapPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not save %s"), TestMapPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("Test map saved: %s"), TestMapPath);
	return 0;
}
