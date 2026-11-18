#include "Misc/AutomationTest.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Game/RuinmatesGameMode.h"
#include "Game/RuinmatesPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Online/RuinmatesSessionSubsystem.h"
#include "Save/RuinmatesSaveGame.h"
#include "Save/RuinmatesSaveSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesGameModeLimitsTest, "RuinMates.Batch3.GameModeLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesGameModeLimitsTest::RunTest(const FString& Parameters)
{
	const ARuinmatesGameMode* CDO = GetDefault<ARuinmatesGameMode>();
	if (!TestNotNull(TEXT("GameMode CDO exists"), CDO))
	{
		return false;
	}
	TestEqual(TEXT("Listen-server party cap is 4"), CDO->GetMaxPartySize(), 4);
	TestEqual(TEXT("Default pawn is the new third-person character"), CDO->DefaultPawnClass, TSubclassOf<APawn>(ARuinmatesCharacter::StaticClass()));
	TestEqual(TEXT("Player state carries campaign identity"), CDO->PlayerStateClass, TSubclassOf<APlayerState>(ARuinmatesPlayerState::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesSaveCampaignTest, "RuinMates.Batch3.SaveCampaign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesSaveCampaignTest::RunTest(const FString& Parameters)
{
	const FString Slot = FString::Printf(TEXT("RuinMatesTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	URuinmatesSaveGame* Save = NewObject<URuinmatesSaveGame>();
	if (!TestTrue(TEXT("Save object created"), IsValid(Save)))
	{
		return false;
	}

	Save->SharedFunds = 350;
	FRuinmatesMemberSave& Host = Save->FindOrAddMember(TEXT("member-host"));
	Host.Level = 3;
	Host.TalentIds = { FName(TEXT("Scavenger")) };
	Host.EquippedLoadoutId = TEXT("Rifle_Standard");
	Save->FindOrAddMember(TEXT("member-guest")).Level = 2;

	TestTrue(TEXT("Save writes a slot"), UGameplayStatics::SaveGameToSlot(Save, Slot, 0));
	URuinmatesSaveGame* Loaded = Cast<URuinmatesSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	const bool bLoaded = TestTrue(TEXT("Save reloads from the slot"), IsValid(Loaded));
	if (bLoaded)
	{
		TestEqual(TEXT("Shared funds survive a reload"), Loaded->SharedFunds, 350);
		const FRuinmatesMemberSave* HostLoaded = Loaded->FindMember(TEXT("member-host"));
		TestTrue(TEXT("Host member survives"), HostLoaded != nullptr);
		if (HostLoaded)
		{
			TestEqual(TEXT("Host level survives"), HostLoaded->Level, 3);
			TestEqual(TEXT("Host loadout survives"), HostLoaded->EquippedLoadoutId, FString(TEXT("Rifle_Standard")));
			TestTrue(TEXT("Host talent survives"), HostLoaded->TalentIds.Contains(FName(TEXT("Scavenger"))));
		}
		TestTrue(TEXT("Guest member survives"), Loaded->FindMember(TEXT("member-guest")) != nullptr);
	}
	UGameplayStatics::DeleteGameInSlot(Slot, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRuinmatesSessionBoundaryTest, "RuinMates.Batch3.SessionBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRuinmatesSessionBoundaryTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("GEngine exists"), GEngine))
	{
		return false;
	}

	// GameInstanceSubsystem cannot be NewObject'd into the transient package.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	if (!TestTrue(TEXT("Temp game instance created"), IsValid(GameInstance)))
	{
		return false;
	}
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(TEXT("RuinMatesSessionBoundaryTest"));

	auto Cleanup = [GameInstance]()
	{
		if (UWorld* World = GameInstance->GetWorld())
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}
		GameInstance->RemoveFromRoot();
	};

	URuinmatesSessionSubsystem* Session = GameInstance->GetSubsystem<URuinmatesSessionSubsystem>();
	if (!TestTrue(TEXT("Session subsystem is created with the game instance"), IsValid(Session)))
	{
		Cleanup();
		return false;
	}

	TestEqual(TEXT("Party size is 4"), Session->GetMaxPartySize(), 4);
	TestFalse(TEXT("Starts out of a party"), Session->IsInParty());
	TestTrue(TEXT("Travel to an unset danger map is refused"), Session->TravelToDangerZone() == false);
	TestTrue(TEXT("Platform invite is refused before a party exists"), Session->OpenPlatformInvite() == false);

	TestTrue(TEXT("HostListenParty can be prepared locally"), Session->HostListenParty());
	TestTrue(TEXT("Host is in a local party"), Session->IsInParty() && Session->IsPartyHost());
	TestEqual(TEXT("Fresh party is at the base stage"), Session->GetTravelStage(), ERuinmatesTravelStage::Base);
	TestFalse(TEXT("Null/local layer does not advertise a platform invite"), Session->CanOpenPlatformInvite());
	TestFalse(TEXT("OpenPlatformInvite does not pretend a platform invite was sent"), Session->OpenPlatformInvite());
	if (Session->IsUsingPlatformOnline())
	{
		AddWarning(TEXT("A non-Null online subsystem is present; platform rooms are still untested and invite is not wired."));
	}

	Session->LeaveParty();
	TestFalse(TEXT("LeaveParty clears host state"), Session->IsInParty() || Session->IsPartyHost());
	Cleanup();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
