#include "UI/RuinmatesSquadRadio.h"
#include "Character/RuinmatesCharacter.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Rendering/DrawElements.h"
#include "Stats/StatsComponent.h"
#include "TimerManager.h"
#include "UI/RuinmatesUIStyle.h"

void URuinmatesSquadRadio::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	const TCHAR* Paths[] = {TEXT("Art/HUD/T_UI_Radio_Normal"), TEXT("Art/HUD/T_UI_Radio_Critical"),
		TEXT("Art/HUD/T_UI_Radio_Dead"), TEXT("Art/HUD/T_UI_Radio_Glow"),
		TEXT("Art/HUD/T_UI_RadioPortrait_Normal"), TEXT("Icons/System/T_Icon_SYS_Users"),
		TEXT("Icons/System/T_Icon_SYS_Speaker"), TEXT("Art/HUD/T_UI_VoiceWave")};
	for (const TCHAR* Path : Paths) Brushes.Add(Path, RuinmatesUI::V4(Path));
	RefreshRoster();
	if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RosterTimer, this, &URuinmatesSquadRadio::RefreshRoster, 1.f, true);
}

void URuinmatesSquadRadio::SetPreview(bool bInPreview)
{
	bPreview = bInPreview;
	RefreshRoster();
}

void URuinmatesSquadRadio::RefreshRoster()
{
	Entries.Reset();
	if (bPreview)
	{
		Entries.Add({FText::FromString(TEXT("Alex")), 0.85f, false, true});
		Entries.Add({FText::FromString(TEXT("Mira")), 0.7f, false, false});
		Entries.Add({FText::FromString(TEXT("Rook")), 0.15f, false, false});
		return;
	}
	const AGameStateBase* State = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	const APawn* OwnerPawn = GetOwningPlayerPawn();
	if (!State || !OwnerPawn) return;
	for (APlayerState* Player : State->PlayerArray)
	{
		if (!IsValid(Player) || Player == OwnerPawn->GetPlayerState() || Player->IsOnlyASpectator()) continue;
		FRuinmatesRadioView Entry;
		Entry.Name = FText::FromString(Player->GetPlayerName());
		if (const APawn* Pawn = Player->GetPawn())
		{
			if (const UStatsComponent* Stats = Pawn->FindComponentByClass<UStatsComponent>())
			{
				Entry.bDead = Stats->DeathState != EDeathState::NotDead;
				// Health currently replicates owner-only. A remote client must not display CDO 100%.
				if (Pawn->HasAuthority()) Entry.Health = Stats->MaxHealth > 0.f ? Stats->Health / Stats->MaxHealth : 0.f;
			}
		}
		Entries.Add(Entry);
		if (Entries.Num() == 3) break;
	}
}

int32 URuinmatesSquadRadio::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Culling,
	FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	if (Entries.IsEmpty()) return Layer;
	auto Image = [&](const TCHAR* Path, float X, float Y, float W, float H, FLinearColor Tint = FLinearColor::White)
	{
		if (const FSlateBrush* Brush = Brushes.Find(Path))
			FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(FVector2D(W,H), FSlateLayoutTransform(FVector2D(X,Y))),
				Brush, ESlateDrawEffect::None, Tint);
	};
	FSlateDrawElement::MakeText(Out, Layer + 1, G.ToPaintGeometry(FVector2D(208,24), FSlateLayoutTransform()),
		bPreview ? TEXT("COMMS / DEMO") : TEXT("COMMS"), RuinmatesUI::Font(16), ESlateDrawEffect::None, RuinmatesUI::Accent());
	for (int32 I = 0; I < Entries.Num(); ++I)
	{
		const FRuinmatesRadioView& Entry = Entries[I];
		const float Y = 24.f + I * 72.f;
		Image(Entry.bDead ? TEXT("Art/HUD/T_UI_Radio_Dead") : Entry.Health >= 0 && Entry.Health < 0.25f
			? TEXT("Art/HUD/T_UI_Radio_Critical") : TEXT("Art/HUD/T_UI_Radio_Normal"), 0,Y,208,64);
		if (Entry.bSpeaking) Image(TEXT("Art/HUD/T_UI_Radio_Glow"),0,Y,208,64);
		Image(TEXT("Art/HUD/T_UI_RadioPortrait_Normal"),8,Y+10,44,44);
		Image(TEXT("Icons/System/T_Icon_SYS_Users"),16,Y+18,28,28, RuinmatesUI::TextSecondary());
		const FString Name = Entry.Name.ToString().Left(12);
		FSlateDrawElement::MakeText(Out, Layer + 1, G.ToPaintGeometry(FVector2D(112,24), FSlateLayoutTransform(FVector2D(60,Y+8))),
			Name, RuinmatesUI::Font(16), ESlateDrawEffect::None, RuinmatesUI::TextPrimary());
		if (Entry.Health >= 0.f)
		{
			static const FSlateBrush White = []() { FSlateBrush Brush; Brush.DrawAs=ESlateBrushDrawType::Image; return Brush; }();
			FSlateDrawElement::MakeBox(Out, Layer+1, G.ToPaintGeometry(FVector2D(96,5), FSlateLayoutTransform(FVector2D(60,Y+42))),
				&White, ESlateDrawEffect::None, RuinmatesUI::WithAlpha(RuinmatesUI::TextSecondary(),0.25f));
			FSlateDrawElement::MakeBox(Out, Layer+2, G.ToPaintGeometry(FVector2D(96 * FMath::Clamp(Entry.Health,0.f,1.f),5),
				FSlateLayoutTransform(FVector2D(60,Y+42))), &White, ESlateDrawEffect::None,
				Entry.Health < 0.25f ? RuinmatesUI::Danger() : RuinmatesUI::Accent());
		}
		if (Entry.bSpeaking) Image(TEXT("Icons/System/T_Icon_SYS_Speaker"),180,Y+22,20,20,RuinmatesUI::Accent());
	}
	return Layer+3;
}

void URuinmatesSquadRadio::NativeDestruct()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RosterTimer);
	Super::NativeDestruct();
}
