#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "RuinmatesTerminalItemSlot.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuinmatesTerminalSlotClicked, int32, StableIndex);

// Uniform item cell. Quality color is only the border and a faint fill.
UCLASS()
class RUINMATES_API URuinmatesTerminalItemSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	FRuinmatesTerminalSlotClicked OnSlotClicked;

	void ApplyView(const FRuinmatesTerminalSlotView& InView);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleClicked();

	void RefreshVisual();

	UPROPERTY()
	TObjectPtr<class UBorder> Frame;

	UPROPERTY()
	TObjectPtr<class UBorder> QualityLine;

	UPROPERTY()
	TObjectPtr<class UTextBlock> IconText;

	UPROPERTY()
	TObjectPtr<class UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<class UTextBlock> CountText;

	UPROPERTY()
	TObjectPtr<class UProgressBar> ScanBar;

	FRuinmatesTerminalSlotView View;
	float ScanTime = 0.f;
};
