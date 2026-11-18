#include "Game/RuinmatesGameMode.h"

#include "Character/RuinmatesCharacter.h"
#include "Game/RuinmatesPlayerState.h"
#include "GameFramework/GameSession.h"
#include "Save/RuinmatesSaveSubsystem.h"

ARuinmatesGameMode::ARuinmatesGameMode()
{
	MaxPartySize = 4;
	PlayerStateClass = ARuinmatesPlayerState::StaticClass();
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
