#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesFieldHUD.generated.h"

class UStatsComponent;
class URuinmatesSquadRadio;
class UMaterialInstanceDynamic;
class UTextBlock;
class UCanvasPanel;

/** Lightweight V4 field overlay: real local vitals, compass, squad and weapon presentation. */
UCLASS()
class RUINMATES_API URuinmatesFieldHUD : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Toggles an explicitly labelled visual reference without changing gameplay state. */
	void SetPreview(bool bInPreview);
	/** Whether sample vitals and icons are currently displayed. */
	bool IsPreview() const { return bPreview; }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry&, float) override;
	virtual int32 NativePaint(const FPaintArgs&, const FGeometry&, const FSlateRect&,
		FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
private:
	/** Reads authoritative replicated local health after either health field changes. */
	UFUNCTION() void HealthChanged(UStatsComponent* Stats, float OldValue, float NewValue, AActor* Instigator);
	/** Reads authoritative stamina; updates the radial material with a guarded ratio. */
	UFUNCTION() void StaminaChanged(UStatsComponent* Stats, float OldValue, float NewValue);
	void RefreshWeapons();

	/** Low-frequency interaction prompt: cheap target lookup, no per-frame scanning. */
	void RefreshInteractPrompt();
	TWeakObjectPtr<UStatsComponent> BoundStats;
	UPROPERTY() TMap<FString, FSlateBrush> Brushes;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
	UPROPERTY() FSlateBrush RingBrush;
	UPROPERTY() TObjectPtr<URuinmatesSquadRadio> Radio;
	UPROPERTY() TObjectPtr<UCanvasPanel> HUDDesign;
	UPROPERTY() TObjectPtr<UTextBlock> InteractPrompt;
	UPROPERTY() TArray<FSlateBrush> WeaponIcons;
	TArray<FString> AmmoLabels;
	TArray<bool> WeaponPresent;
	int32 EquippedIndex = INDEX_NONE;
	float Health = 1.f;
	float Stamina = 1.f;
	bool bPreview = false;
	FTimerHandle WeaponRefreshTimer;
	FTimerHandle InteractPromptTimer;
};
