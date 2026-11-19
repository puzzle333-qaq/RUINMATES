#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesSquadRadio.generated.h"

/** Display-only teammate view; negative health means the server does not expose that value. */
struct FRuinmatesRadioView
{
	FText Name;
	float Health = -1.f;
	bool bDead = false;
	bool bSpeaking = false;
};

/** Shared HUD/terminal radio. Never invents voice or health for live teammates. */
UCLASS()
class RUINMATES_API URuinmatesSquadRadio : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Simulated people and speaking state are restricted to labelled visual previews. */
	void SetPreview(bool bInPreview);
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
		FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	void RefreshRoster();
	bool bPreview = false;
	TArray<FRuinmatesRadioView> Entries;
	FTimerHandle RosterTimer;
	UPROPERTY() TMap<FString, FSlateBrush> Brushes;
};
