#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RuinmatesSaveGame.generated.h"

USTRUCT(BlueprintType)
struct RUINMATES_API FRuinmatesMemberSave
{
	GENERATED_BODY()

	// Stable id that survives map travel and a process restart (unique net id or a generated guid).
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	FString StableId;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FName> TalentIds;

	// Soft identifier for the member's equipped loadout / weapon data. May be empty.
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	FString EquippedLoadoutId;
};

UCLASS()
class RUINMATES_API URuinmatesSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	int32 SharedFunds = 0;

	UPROPERTY(BlueprintReadWrite, Category = "RuinMates|Save")
	TArray<FRuinmatesMemberSave> Members;

	FRuinmatesMemberSave* FindMember(const FString& StableId);
	FRuinmatesMemberSave& FindOrAddMember(const FString& StableId);
};
