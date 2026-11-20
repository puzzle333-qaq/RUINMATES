#include "Game/RuinmatesGameMode.h"

#include "Character/RuinmatesCharacter.h"
#include "Game/RuinmatesGameState.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/GameSession.h"
#include "Save/RuinmatesSaveSubsystem.h"

ARuinmatesGameMode::ARuinmatesGameMode()
{
	MaxPartySize = 4;
	PlayerStateClass = ARuinmatesPlayerState::StaticClass();
	GameStateClass = ARuinmatesGameState::StaticClass();
	DefaultPawnClass = ARuinmatesCharacter::StaticClass();
}

void ARuinmatesGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (GameSession)
	{
		GameSession->MaxPlayers = MaxPartySize;
	}
}

void ARuinmatesGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ARuinmatesPlayerState* PS = NewPlayer ? Cast<ARuinmatesPlayerState>(NewPlayer->PlayerState) : nullptr;
	UGameInstance* GI = GetGameInstance();
	if (URuinmatesSaveSubsystem* Save = GI ? GI->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr)
	{
		Save->RestoreOrAssignMember(PS);
	}
}

void ARuinmatesGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	if (auto* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<URuinmatesSaveSubsystem>() : nullptr)
		Save->RestoreBackpack(NewPlayer ? Cast<ARuinmatesCharacter>(NewPlayer->GetPawn()) : nullptr);
}

bool ARuinmatesGameMode::AreTeammates(const AActor* First, const AActor* Second)
{
	// Co-op has a single side: every player character is a teammate of every other one.
	const ARuinmatesCharacter* FirstCharacter = Cast<ARuinmatesCharacter>(First);
	const ARuinmatesCharacter* SecondCharacter = Cast<ARuinmatesCharacter>(Second);
	return FirstCharacter && SecondCharacter && FirstCharacter != SecondCharacter;
}

float ARuinmatesGameMode::ScaleDamageForTarget(float BaseDamage, const AActor* DamageInstigator, const AActor* DamageVictim) const
{
	if (BaseDamage <= 0.f || !AreTeammates(DamageInstigator, DamageVictim))
	{
		return FMath::Max(0.f, BaseDamage);
	}
	return BaseDamage * GetFriendlyFireMultiplier();
}
