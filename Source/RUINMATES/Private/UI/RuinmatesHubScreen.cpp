#include "UI/RuinmatesHubScreen.h"

#include "Character/RuinmatesCharacter.h"
#include "Hub/RuinmatesHubDefinition.h"
#include "Hub/RuinmatesHubStateActor.h"
#include "Inventory/RuinmatesBackpackComponent.h"
#include "Inventory/RuinmatesItemDatabaseSubsystem.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UI/RuinmatesUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	FText RoomName(FName Id)
	{
		if (Id == RuinmatesHub::CoreId()) return FText::FromString(TEXT("中央大厅"));
		if (Id == RuinmatesHub::MedicalId()) return FText::FromString(TEXT("医疗室"));
		if (Id == RuinmatesHub::WorkshopId()) return FText::FromString(TEXT("工作区"));
		if (Id == RuinmatesHub::ElevatorId()) return FText::FromString(TEXT("电梯"));
		if (Id == RuinmatesHub::BlackMarketId()) return FText::FromString(TEXT("无人值守黑市"));
		return FText::FromString(TEXT("任务出口"));
	}
	FText ItemName(const UObject* Context, FName Id)
	{
		const auto* Database = URuinmatesItemDatabaseSubsystem::Get(Context);
		const auto* Row = Database ? Database->FindItemDefinition(Id) : nullptr;
		return Row ? Row->DisplayName : FText::FromName(Id);
	}
}

void URuinmatesHubScreen::SetContext(URuinmatesHubInteractionComponent* InRouter, ARuinmatesHubTerminal* InTerminal)
{
	Router = InRouter; Terminal = InTerminal;
	if (!Terminal || !Terminal->Hub) return;
	auto* Hub = Terminal->Hub.Get();
	HubHandle = Hub->OnRoomStateChanged.AddWeakLambda(this, [this](FName) { Refresh(); });
	if (Hub->Warehouse) WarehouseHandle = Hub->Warehouse->OnChanged.AddUObject(this, &ThisClass::Refresh);
	if (auto* Character = Cast<ARuinmatesCharacter>(Router->GetOwningPawn()); Character && Character->GetBackpack())
		BackpackHandle = Character->GetBackpack()->OnChanged.AddUObject(this, &ThisClass::Refresh);
}

TSharedRef<SWidget> URuinmatesHubScreen::RebuildWidget()
{
	PanelBrush = RuinmatesUI::PanelBrush();
	CommandStyle = RuinmatesUI::ButtonStyle();
	TSharedRef<SWidget> Root = SNew(SBorder).Padding(32).BorderImage(&PanelBrush).BorderBackgroundColor(FLinearColor::White)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)
		[SNew(STextBlock).Text(FText::FromString(TEXT("RUINMATES  /  基地管理"))).Font(RuinmatesUI::FontBold(25)).ColorAndOpacity(RuinmatesUI::Cyan())]
		+ SVerticalBox::Slot().FillHeight(1)
		[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Rows, SVerticalBox)]]
		+ SVerticalBox::Slot().AutoHeight().Padding(0,12)
		[SNew(SButton).ButtonStyle(&CommandStyle).OnClicked_Lambda([this]() { if (Router) Router->CloseHub(); return FReply::Handled(); })
		[SNew(STextBlock).Text(FText::FromString(TEXT("关闭  ·  Esc"))).Font(RuinmatesUI::Font(18))]]
	];
	Refresh();
	return Root;
}

void URuinmatesHubScreen::Refresh()
{
	if (!Rows) return;
	auto Text = [this](const FText& Value, FLinearColor Color = RuinmatesUI::TextPrimary())
	{
		Rows->AddSlot().AutoHeight().Padding(4,6)
		[SNew(STextBlock).Text(Value).Font(RuinmatesUI::Font(17)).ColorAndOpacity(Color).AutoWrapText(true)];
	};
	if (!Terminal || !Terminal->Hub || !Router) { Text(FText::FromString(TEXT("基地终端已离线"))); return; }
	auto* Hub = Terminal->Hub.Get();
	const auto& State = Hub->GetSnapshot();
	const auto* Definition = Hub->Definition.Get();
	if (!State.bInitialized || !Definition) { Text(FText::FromString(TEXT("正在同步基地状态…"))); return; }
	auto* OwnerCharacter = Cast<ARuinmatesCharacter>(Router->GetOwningPawn());
	auto* OwnerBag = OwnerCharacter ? OwnerCharacter->GetBackpack() : nullptr;
	const FText Fuel = FText::FromString(FString::Printf(TEXT("发电机：%s  |  燃油 %.1f / %.1f  |  %s"),
		State.Generator.bInstalled ? TEXT("已安装") : TEXT("未安装"), State.Generator.Fuel,
		Definition->FuelCapacity, State.Generator.IsPowered() ? TEXT("供电中") : TEXT("断电")));
	const uint32 WarehouseRevision = Hub->Warehouse ? Hub->Warehouse->GetRevision() : 0;
	const uint32 BagRevision = OwnerBag ? OwnerBag->GetRevision() : 0;
	if (LastRevision==State.Revision && LastWarehouseRevision==WarehouseRevision && LastBackpackRevision==BagRevision)
	{ if (FuelText) FuelText->SetText(Fuel); return; }
	LastRevision=State.Revision; LastWarehouseRevision=WarehouseRevision; LastBackpackRevision=BagRevision;
	Rows->ClearChildren();
	FuelText.Reset();
	Text(Result, RuinmatesUI::Money());
	if (Terminal->Type!=ERuinmatesHubTerminalType::Construction && Terminal->Type!=ERuinmatesHubTerminalType::Generator)
	{
		Text(FText::FromString(TEXT("设施服务终端  ·  建设操作请前往中央控制台")),RuinmatesUI::Cyan());
		return;
	}
	if (Definition->bTestEconomy) Text(FText::FromString(TEXT("建设资源数量为开发测试数值")), RuinmatesUI::TextWeak());
	Rows->AddSlot().AutoHeight().Padding(4,6)
	[SAssignNew(FuelText,STextBlock).Text(Fuel).Font(RuinmatesUI::Font(17)).ColorAndOpacity(RuinmatesUI::TextPrimary())];
	AddCommand(FText::FromString(TEXT("领取首次建设物资到仓库")), ERuinmatesHubCommand::StarterKit, NAME_None, 1, !State.bStarterKitClaimed);
	AddCommand(FText::FromString(TEXT("安装发电机 ×1")), ERuinmatesHubCommand::Install, NAME_None, 1, !State.Generator.bInstalled);
	AddCommand(FText::FromString(TEXT("添加燃油 ×1")), ERuinmatesHubCommand::Fuel, NAME_None, 1, State.Generator.bInstalled);
	AddCommand(FText::FromString(TEXT("启动发电机")), ERuinmatesHubCommand::Start, NAME_None, 1, State.Generator.bInstalled && !State.Generator.bStarted && State.Generator.Fuel > 0);
	AddCommand(FText::FromString(TEXT("关闭发电机")), ERuinmatesHubCommand::Stop, NAME_None, 1, State.Generator.bStarted);
	for (const auto& Room : State.Rooms)
	{
		const TCHAR* Availability = Room.Permanent.bStructureBlocked ? TEXT("入口阻挡") :
			!Room.Permanent.bUnlocked ? TEXT("等待解锁") : Room.Runtime.bOperational ? TEXT("正常运行") : TEXT("已解锁 · 等待供电");
		Text(FText::FromString(FString::Printf(TEXT("%s  Lv.%d / %d  ·  %s  ·  %s  ·  %s"), *RoomName(Room.RoomId).ToString(),
			Room.Permanent.Level, Hub->GetRoomMaxLevel(Room.RoomId), Room.Permanent.bCleared ? TEXT("已清理") : TEXT("待清理"),
			Room.Permanent.Level==Hub->GetRoomMaxLevel(Room.RoomId) ? TEXT("已达最高等级") : TEXT("仍可建设"), Availability)), RuinmatesUI::Cyan());
		for (const auto& Action : Definition->Actions)
		{
			if (Action.RoomId != Room.RoomId) continue;
			const auto Quote = Hub->GetActionQuote(Router->GetOwningPawn(), Action.ActionId);
			FString Costs;
			for (const auto& Prerequisite : Action.Prerequisites)
			{
				FRuinmatesHubRoomState Current;
				Hub->GetRoomState(Prerequisite.RoomId,Current);
				Costs+=FString::Printf(TEXT("  前置 %s Lv.%d（当前 %d）"),*RoomName(Prerequisite.RoomId).ToString(),Prerequisite.MinimumLevel,Current.Permanent.Level);
			}
			for (const auto& Cost : Quote.Required) Costs += FString::Printf(TEXT("  %s %d/%d（缺 %d）"),
				*ItemName(this, Cost.Key).ToString(), Quote.Available.FindRef(Cost.Key), Cost.Value,
				FMath::Max(0, Cost.Value - Quote.Available.FindRef(Cost.Key)));
			const TCHAR* Verb = Action.Type == ERuinmatesHubActionType::Clear ? TEXT("清理") :
				Action.Type == ERuinmatesHubActionType::Upgrade ? TEXT("升级") : TEXT("恢复 / 解锁");
			AddCommand(FText::FromString(FString::Printf(TEXT("%s%s  ·  %s"), Verb, *Costs, *Quote.Reason.ToString())),
				ERuinmatesHubCommand::Construct, Action.ActionId, 1, Quote.bCanExecute);
		}
	}
	Text(FText::FromString(TEXT("共享仓库  ·  每次精确存取 1 件")), RuinmatesUI::Cyan());
	TSet<FName> Items;
	if (Hub->Warehouse) for (const auto& Stack : Hub->Warehouse->GetSlots()) if (!Stack.IsEmpty()) Items.Add(Stack.ItemId);
	auto* Character = Cast<ARuinmatesCharacter>(Router->GetOwningPawn());
	auto* Backpack = Character ? Character->GetBackpack() : nullptr;
	if (Backpack) for (const auto& Stack : Backpack->GetSlots()) if (!Stack.IsEmpty()) Items.Add(Stack.ItemId);
	for (FName Id : Items)
	{
		const int32 Stored = Hub->Warehouse ? Hub->Warehouse->CountItem(Id) : 0;
		const int32 Held = Backpack ? Backpack->CountItem(Id) : 0;
		Text(FText::FromString(FString::Printf(TEXT("%s  ·  仓库 %d  /  背包 %d"), *ItemName(this, Id).ToString(), Stored, Held)));
		AddCommand(FText::FromString(TEXT("存入 1 件")), ERuinmatesHubCommand::Deposit, Id, 1, Held > 0);
		AddCommand(FText::FromString(TEXT("取出 1 件")), ERuinmatesHubCommand::Withdraw, Id, 1, Stored > 0);
	}
	AddCommand(FText::FromString(TEXT("保存基地进度")), ERuinmatesHubCommand::Save);
}

void URuinmatesHubScreen::AddCommand(const FText& Label, ERuinmatesHubCommand Command, FName Id, int32 Count, bool bEnabled)
{
	Rows->AddSlot().AutoHeight().Padding(4,3)
	[SNew(SButton).ButtonStyle(&CommandStyle).IsEnabled(bEnabled && !bPending).ContentPadding(FMargin(12,8))
		.OnClicked_Lambda([this, Command, Id, Count]() { return Request(Command, Id, Count); })
		[SNew(STextBlock).Text(Label).Font(RuinmatesUI::Font(16)).AutoWrapText(true)]];
}

FReply URuinmatesHubScreen::Request(ERuinmatesHubCommand Command, FName Id, int32 Count)
{
	if (bPending || !Router || !Terminal || !Terminal->Hub) return FReply::Handled();
	bPending = true;
	LastRevision=-1;
	Router->Server_RequestHub(Terminal, Command, Id, Count, Terminal->Hub->GetSnapshot().Revision);
	Refresh();
	return FReply::Handled();
}

void URuinmatesHubScreen::ShowResult(bool bSuccess, const FText& Reason)
{
	bPending = false; Result = Reason; LastRevision=-1; Refresh();
}

void URuinmatesHubScreen::NativeDestruct()
{
	if (Terminal && Terminal->Hub)
	{
		Terminal->Hub->OnRoomStateChanged.Remove(HubHandle);
		if (Terminal->Hub->Warehouse) Terminal->Hub->Warehouse->OnChanged.Remove(WarehouseHandle);
	}
	if (Router) if (auto* Character = Cast<ARuinmatesCharacter>(Router->GetOwningPawn()); Character && Character->GetBackpack())
		Character->GetBackpack()->OnChanged.Remove(BackpackHandle);
	Rows.Reset();
	Super::NativeDestruct();
}

FReply URuinmatesHubScreen::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape && Router) { Router->CloseHub(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}
