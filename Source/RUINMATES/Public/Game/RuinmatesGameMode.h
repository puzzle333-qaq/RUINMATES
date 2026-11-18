#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "RuinmatesGameMode.generated.h"

// Listen-server cooperative mode: 1-4 players. Does not start a dedicated server.
UCLASS()
class RUINMATES_API ARuinmatesGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ARuinmatesGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintPure, Category = "RuinMates|Session")
	int32 GetMaxPartySize() const { return MaxPartySize; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RuinMates|Session", meta = (ClampMin = "1", ClampMax = "4"))
	int32 MaxPartySize = 4;
};
