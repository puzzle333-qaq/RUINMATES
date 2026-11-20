#include "Editor.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/RuinmatesGameState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Hub/RuinmatesHubBlackMarket.h"
#include "Hub/RuinmatesHubDefinition.h"
#include "Hub/RuinmatesHubInteraction.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/RuinmatesSaveSubsystem.h"
#include "Stats/StatsComponent.h"

namespace
{
	TWeakObjectPtr<ARuinmatesHubStateActor> PreparedHub;
	int32 SentConstructionRevision=-1;
	/** Explicitly isolated editor playtest. No shipping command, normal slot or source asset is mutated. */
	bool HubStep(UWorld* World, const FString& Step, FString& Detail)
	{
		if (!World || !World->IsPlayInEditor() || World->GetNetMode()==NM_Client) return false;
		ARuinmatesHubStateActor* Hub=nullptr;
		ARuinmatesHubBlackMarket* Market=nullptr;
		for (TActorIterator<ARuinmatesHubStateActor> It(World); It; ++It) { if (Hub) return false; Hub=*It; }
		for (TActorIterator<ARuinmatesHubBlackMarket> It(World); It; ++It) Market=*It;
		auto* PC=World->GetFirstPlayerController();
		auto* Pawn=PC ? Cast<ARuinmatesCharacter>(PC->GetPawn()) : nullptr;
		auto* GI=World->GetGameInstance();
		auto* Save=GI ? GI->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr;
		if (!Hub || !Pawn || !Save || !Hub->Definition) return false;
		if (Step==TEXT("RestartVerify"))
		{
			if (!Save->SaveSlotName.StartsWith(TEXT("HubPlaytest_")) || !Hub->GetSnapshot().bStarterKitClaimed
				|| Hub->GetSnapshot().Generator.Fuel!=0.f || Hub->GetSnapshot().Generator.IsPowered()) return false;
			for (const auto& Room:Hub->GetSnapshot().Rooms)
				if (!Room.Permanent.bUnlocked || Room.Permanent.Level!=Hub->GetRoomMaxLevel(Room.RoomId)
					|| Room.Permanent.bStructureBlocked || !Room.Permanent.bCleared) return false;
			Detail=Save->SaveSlotName;
			return Hub->Warehouse->CountItem(TEXT("Hub_Scrap"))==20 && Hub->Warehouse->CountItem(TEXT("Circuit"))==7;
		}
		if (Step==TEXT("Initial"))
		{
			if (Hub->GetSnapshot().bStarterKitClaimed) return false;
			Save->SaveSlotName=TEXT("HubPlaytest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
			Save->ResetInMemoryCampaign(); PreparedHub=Hub;
			Detail=Save->SaveSlotName;
			return Hub->IsHubInitialized() && !Hub->GetSnapshot().Generator.IsPowered();
		}
		if (PreparedHub!=Hub || !Save->SaveSlotName.StartsWith(TEXT("HubPlaytest_"))) return false;
		if (Step==TEXT("NetworkPrepare"))
		{
			for (FConstPlayerControllerIterator It=World->GetPlayerControllerIterator();It;++It)
				if (auto* Remote=It->Get();Remote && !Remote->IsLocalController() && Remote->GetPawn())
					for (TActorIterator<ARuinmatesHubTerminal> T(World);T;++T)
						if (T->Type==ERuinmatesHubTerminalType::Construction)
						{
							const FVector Location=T->GetActorLocation()+FVector(0,-140,-44);
							Remote->GetPawn()->TeleportTo(Location,FRotator(0,90,0),false,true);
							Remote->ClientSetLocation(Location,FRotator(0,90,0));
							return true;
						}
			return false;
		}
		if (Step==TEXT("NetworkSend") || Step==TEXT("NetworkConstruct") || Step==TEXT("NetworkReplay"))
		{
			for (const FWorldContext& Context:GEngine->GetWorldContexts())
			{
				auto* Client=Context.World();
				if (!Client || !Client->IsPlayInEditor() || Client->GetNetMode()!=NM_Client) continue;
				auto* ClientPC=Client->GetFirstPlayerController();
				auto* ClientPawn=ClientPC ? Cast<ARuinmatesCharacter>(ClientPC->GetPawn()) : nullptr;
				if (!ClientPawn) continue;
				for (TActorIterator<ARuinmatesHubTerminal> T(Client);T;++T)
					if (T->Type==ERuinmatesHubTerminalType::Construction && T->Hub && T->Hub->IsHubInitialized())
					{
						const bool bConstruct=Step!=TEXT("NetworkSend");
						const int32 Revision=Step==TEXT("NetworkReplay") ? SentConstructionRevision : T->Hub->GetSnapshot().Revision;
						if (Step==TEXT("NetworkConstruct")) SentConstructionRevision=Revision;
						ClientPawn->FindComponentByClass<URuinmatesHubInteractionComponent>()->Server_RequestHub(*T,
							bConstruct?ERuinmatesHubCommand::Construct:ERuinmatesHubCommand::StarterKit,
							bConstruct?FName(TEXT("Hub.Exit.Restore")):NAME_None,1,Revision);
						return true;
					}
			}
			return false;
		}
		if (Step==TEXT("NetworkRejected")) return !Hub->GetSnapshot().bStarterKitClaimed && Hub->Warehouse->CountItem(TEXT("Hub_Scrap"))==0;
		if (Step==TEXT("LateJoin")) { GEditor->RequestLateJoin(); return true; }
		if (Step==TEXT("NetworkVerify") || Step==TEXT("LateVerify") || Step==TEXT("NetworkPaidVerify"))
		{
			const bool bPaid=Step==TEXT("NetworkPaidVerify");
			if (!Hub->GetSnapshot().bStarterKitClaimed) { Detail=TEXT("server did not accept starter intent"); return false; }
			if (bPaid)
			{
				FRuinmatesHubRoomState Exit;
				if (!Hub->GetRoomState(RuinmatesHub::ExitId(),Exit) || !Exit.Permanent.bUnlocked
					|| Hub->Warehouse->CountItem(TEXT("Hub_Scrap"))!=10) return false;
			}
			int32 ClientCount=0;
			for (const auto& Context:GEngine->GetWorldContexts())
			{
				auto* Client=Context.World();
				if (!Client || !Client->IsPlayInEditor() || Client->GetNetMode()!=NM_Client) continue;
				++ClientCount;
				bool Found=false;
				for (TActorIterator<ARuinmatesHubStateActor> H(Client);H;++H)
				{
					Found=true;
					if (!H->IsHubInitialized() || !H->GetSnapshot().bStarterKitClaimed || H->GetSnapshot().Rooms!=Hub->GetSnapshot().Rooms
						|| !H->Warehouse || H->Warehouse->CountItem(TEXT("Hub_Scrap"))!=(bPaid?10:12))
					{ Detail=FString::Printf(TEXT("client init=%d claim=%d rooms=%d warehouse=%d"),H->IsHubInitialized(),H->GetSnapshot().bStarterKitClaimed,H->GetSnapshot().Rooms.Num(),H->Warehouse?H->Warehouse->CountItem(TEXT("Hub_Scrap")):-1); return false; }
					FText Rejected;
					if (H->TryClaimStarterKit(Client->GetFirstPlayerController()->GetPawn(),Rejected)) return false;
				}
				if (!Found) return false;
			}
			Detail=FString::Printf(TEXT("replicated_clients=%d"),ClientCount);
			return ClientCount >= (Step==TEXT("LateVerify")?2:1);
		}
		FText Reason;
		auto Action=[&](const TCHAR* Id) { bool Result=Hub->TryExecuteAction(Pawn,Id,Hub->GetSnapshot().Revision,Reason); Detail=Reason.ToString(); return Result; };
		if (Step==TEXT("Starter")) return Hub->TryClaimStarterKit(Pawn,Reason);
		if (Step==TEXT("Exit")) return Action(TEXT("Hub.Exit.Restore"));
		if (Step==TEXT("Materials"))
		{
			// Exploration content is absent. This explicit editor-only grant is NOT a gameplay reward.
			auto* DB=GI->GetSubsystem<URuinmatesItemDatabaseSubsystem>();
			return DB && Hub->Warehouse->TryAddExact(DB->CreateItemStack(TEXT("Hub_Scrap"),1),40)
				&& Hub->Warehouse->TryAddExact(DB->CreateItemStack(TEXT("Circuit"),1),10);
		}
		if (Step==TEXT("Core1")) return Action(TEXT("Hub.Core.Clear")) && Action(TEXT("Hub.Core.Upgrade1"));
		if (Step==TEXT("Power")) return Hub->TryInstallGenerator(Pawn,Hub->GetSnapshot().Revision,Reason)
			&& Hub->TryAddFuel(Pawn,Hub->GetSnapshot().Revision,Reason)
			&& Hub->TrySetGeneratorStarted(Pawn,true,Hub->GetSnapshot().Revision,Reason);
		if (Step==TEXT("Rooms"))
		{
			for (const TCHAR* Id : {TEXT("Hub.Medical"),TEXT("Hub.Workshop"),TEXT("Hub.Elevator"),TEXT("Hub.BlackMarket")})
				if (!Action(*(FString(Id)+TEXT(".Clear"))) || !Action(*(FString(Id)+TEXT(".Unlock")))) return false;
			return true;
		}
		if (Step==TEXT("Core2")) return Action(TEXT("Hub.Core.Upgrade2"));
		if (Step==TEXT("Warehouse"))
		{
			const int32 Before=Hub->Warehouse->CountItem(TEXT("Circuit"));
			return Hub->TryTransferWarehouse(Pawn,TEXT("Circuit"),1,false,Hub->GetSnapshot().Revision,Reason)
				&& Hub->TryTransferWarehouse(Pawn,TEXT("Circuit"),1,true,Hub->GetSnapshot().Revision,Reason)
				&& Hub->Warehouse->CountItem(TEXT("Circuit"))==Before;
		}
		if (Step==TEXT("Medical"))
		{
			if (!Pawn->GetStats()) return false;
			Pawn->GetStats()->ChangeHealthByAmount(-50.f,Pawn);
			const float Before=Pawn->GetStats()->GetHealth();
			for (TActorIterator<ARuinmatesHubTerminal> T(World);T;++T)
				if (T->Type==ERuinmatesHubTerminalType::Medical)
				{
					Pawn->SetActorLocation(T->GetActorLocation()+FVector(140,0,-29));
					IInteractable::Execute_Interact(*T,Pawn);
					return Pawn->GetStats()->GetHealth()>Before;
				}
			return false;
		}
		if (Step==TEXT("OpenUI"))
		{
			for (TActorIterator<ARuinmatesHubTerminal> It(World); It; ++It)
				if (It->Type==ERuinmatesHubTerminalType::Construction)
				{
					Pawn->SetActorLocation(It->GetActorLocation()+FVector(0,-140,-44));
					IInteractable::Execute_Interact(*It,Pawn);
					return It->ValidateAccess(Pawn);
				}
			return false;
		}
		if (Step==TEXT("MarketOpen"))
		{
			Pawn->FindComponentByClass<URuinmatesHubInteractionComponent>()->CloseHub();
			if (!Market) return false;
			Pawn->SetActorLocation(Market->GetActorLocation()+FVector(-100,100,-54));
			IInteractable::Execute_Interact(Market,Pawn);
			return true;
		}
		if (Step==TEXT("MarketTrade"))
		{
			if (!Market) return false;
			IInteractable::Execute_Interact(Market,Pawn);
			return Pawn->GetActiveMerchant()==Market;
		}
		if (Step==TEXT("MarketLeave")) { Pawn->SetActorLocation(FVector(0,-700,95)); return true; }
		if (Step==TEXT("MarketClosed")) return Market && Market->IsWindowClosed() && !Pawn->IsTrading();
		if (Step==TEXT("BuySell"))
		{
			auto* GS=World->GetGameState<ARuinmatesGameState>();
			if (!GS || !Market || !Market->IsTradeAllowed(Pawn)) return false;
			GS->SetSharedFunds(1000);
			auto* Bag=Pawn->GetBackpack();
			const int32 Before=Bag->CountItem(TEXT("Hub_Scrap"));
			Pawn->RequestBuyItem(TEXT("Hub_Scrap"),1);
			if (Bag->CountItem(TEXT("Hub_Scrap"))!=Before+1) return false;
			for (int32 I=0;I<Bag->GetSlots().Num();++I)
			{
				const auto Stack=Bag->GetSlots()[I];
				if (Stack.ItemId==FName(TEXT("Hub_Scrap")))
				{ Pawn->RequestSellItem(I,1,Stack.ItemId,Stack.Quality,Stack.Count,Bag->GetRevision()); break; }
			}
			return Bag->CountItem(TEXT("Hub_Scrap"))==Before && GS->GetSharedFunds()<1000;
		}
		if (Step==TEXT("MarketClose")) { Pawn->EndTrade(); if (Market) Market->CloseSession(); return !Pawn->IsTrading(); }
		if (Step==TEXT("Outage"))
		{
			if (!Hub->TrySetGeneratorStarted(Pawn,false,Hub->GetSnapshot().Revision,Reason)) return false;
			FRuinmatesHubRoomState Medical;
			return Hub->GetRoomState(RuinmatesHub::MedicalId(),Medical) && Medical.Permanent.Level==1
				&& Medical.Permanent.bUnlocked && !Medical.Runtime.bOperational && !Medical.Permanent.bStructureBlocked;
		}
		if (Step==TEXT("Exhaust"))
		{
			Hub->Definition=DuplicateObject<URuinmatesHubDefinition>(Hub->Definition,Hub);
			Hub->Definition->BurnPerSecond=1000.f;
			return Hub->TrySetGeneratorStarted(Pawn,true,Hub->GetSnapshot().Revision,Reason);
		}
		if (Step==TEXT("ExhaustVerify"))
		{
			FRuinmatesHubRoomState Medical;
			return !Hub->GetSnapshot().Generator.IsPowered() && Hub->GetSnapshot().Generator.Fuel==0.f
				&& Hub->GetRoomState(RuinmatesHub::MedicalId(),Medical) && Medical.Permanent.bUnlocked
				&& !Medical.Permanent.bStructureBlocked && !Medical.Runtime.bOperational;
		}
		if (Step==TEXT("Reload"))
		{
			if (!Hub->SaveProgress()) return false;
			const auto Before=Hub->GetSnapshot();
			Save->ResetInMemoryCampaign();
			FRuinmatesHubSnapshot Loaded; TArray<FRuinmatesItemStack> Slots;
			return Save->LoadCampaign() && Save->GetSavedHubState(Loaded,Slots) && Loaded.bStarterKitClaimed
				&& Loaded.Rooms.Num()==Before.Rooms.Num() && Slots.Num()==Hub->Warehouse->GetCapacity();
		}
		return false;
	}
}

/** Writes reviewable PASS/FAIL results; all material grants require an isolated GUID test profile. */
static FAutoConsoleCommand GHubDevelopmentStep(TEXT("Ruinmates.Editor.HubTest"),
	TEXT("Isolated PIE test: Initial Starter Exit Materials Core1 Power Rooms Core2 OpenUI MarketOpen MarketTrade BuySell MarketClose Outage Reload. Writes ONLY HubPlaytest_* saves."),
	FConsoleCommandWithArgsDelegate::CreateStatic([](const TArray<FString>& Args)
	{
		if (!GEditor || Args.Num()!=1) return;
		// Python editor callbacks set this flag, which makes AActor::GetFunctionCallspace force
		// every RPC local. Disable it only inside this PIE harness so real network routing is tested.
		TGuardValue<bool> NetworkRouting(GAllowActorScriptExecutionInEditor,false);
		FString Detail;
		UWorld* Server=GEditor->PlayWorld;
		for (const auto& Context:GEngine->GetWorldContexts())
			if (Context.World() && Context.World()->IsPlayInEditor() && Context.World()->GetNetMode()==NM_ListenServer) Server=Context.World();
		const bool bPass=HubStep(Server,Args[0],Detail);
		const FString Line=FString::Printf(TEXT("%s %s %s\n"),bPass?TEXT("PASS"):TEXT("FAIL"),*Args[0],*Detail);
		FFileHelper::SaveStringToFile(Line,*(FPaths::ProjectSavedDir()/TEXT("HubDevelopment/playtest_steps.txt")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
		UE_LOG(LogTemp,Log,TEXT("HUB_PLAYTEST %s"),*Line);
	}));
