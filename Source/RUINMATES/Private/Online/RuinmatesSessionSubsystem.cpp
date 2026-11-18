#include "Online/RuinmatesSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "OnlineSubsystem.h"
#include "Save/RuinmatesSaveSubsystem.h"

void URuinmatesSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshOnlineSubsystemName();
	StatusText = IsUsingPlatformOnline()
		? FString::Printf(TEXT("在线层是 %s。平台房间要等登录和凭据就绪后才能用。"), *ActiveOnlineSubsystemName.ToString())
		: FString(TEXT("当前在线层是 Null 或未加载，只能做本地 Listen Server。没有 EOS/Steam 房间。"));
}

void URuinmatesSessionSubsystem::RefreshOnlineSubsystemName()
{
	if (const IOnlineSubsystem* OSS = IOnlineSubsystem::Get())
	{
		ActiveOnlineSubsystemName = OSS->GetSubsystemName();
	}
	else
	{
		ActiveOnlineSubsystemName = NAME_None;
	}
}

bool URuinmatesSessionSubsystem::IsUsingPlatformOnline() const
{
	return ActiveOnlineSubsystemName != NAME_None
		&& ActiveOnlineSubsystemName != FName(TEXT("NULL"))
		&& ActiveOnlineSubsystemName != FName(TEXT("Null"));
}

bool URuinmatesSessionSubsystem::HostListenParty()
{
	RefreshOnlineSubsystemName();
	bInParty = true;
	bIsHost = true;
	TravelStage = ERuinmatesTravelStage::Base;
	StatusText = IsUsingPlatformOnline()
		? FString::Printf(TEXT("主机已准备 Listen Server（在线层 %s）。平台邀请尚未在此适配层开通。"), *ActiveOnlineSubsystemName.ToString())
		: FString(TEXT("主机已准备 1–4 人 Listen Server。当前是 Null/本地层，不是平台房间。"));
	return true;
}

void URuinmatesSessionSubsystem::LeaveParty()
{
	bInParty = false;
	bIsHost = false;
	TravelStage = ERuinmatesTravelStage::Base;
	StatusText = TEXT("已离开队伍。");
}

bool URuinmatesSessionSubsystem::OpenPlatformInvite()
{
	RefreshOnlineSubsystemName();
	if (!CanOpenPlatformInvite())
	{
		StatusText = TEXT("当前没有可用的平台邀请（未接 EOS/Steam，或还没有组队）。");
		return false;
	}

	// A real EOS/Steam invite overlay would go here once those plugins and credentials exist.
	StatusText = TEXT("平台在线层已检测到，但邀请 overlay 尚未接线。不会假装已经发出邀请。");
	return false;
}

bool URuinmatesSessionSubsystem::HostServerTravel(FName MapName, ERuinmatesTravelStage NewStage)
{
	if (!bIsHost || !bInParty)
	{
		StatusText = TEXT("只有主机在组队后才能切图。");
		return false;
	}
	if (MapName.IsNone())
	{
		StatusText = TEXT("地图名是空的。请先在会话子系统上设置 BaseMapName / DangerMapName。");
		return false;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		StatusText = TEXT("没有可用的世界，无法 ServerTravel。");
		return false;
	}

	if (URuinmatesSaveSubsystem* Save = GI->GetSubsystem<URuinmatesSaveSubsystem>())
	{
		Save->CapturePartyFromWorld(World);
		Save->SaveCampaign();
	}

	const FString Url = FString::Printf(TEXT("%s?listen"), *MapName.ToString());
	World->ServerTravel(Url, true);
	TravelStage = NewStage;
	StatusText = FString::Printf(TEXT("正在前往 %s（Listen Server）。联机旅行尚未实测。"), *MapName.ToString());
	return true;
}

bool URuinmatesSessionSubsystem::TravelToDangerZone()
{
	return HostServerTravel(DangerMapName, ERuinmatesTravelStage::DangerZone);
}

bool URuinmatesSessionSubsystem::TravelToBase()
{
	return HostServerTravel(BaseMapName, ERuinmatesTravelStage::Base);
}
