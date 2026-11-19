#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesTechControls.generated.h"

class USlider;
class UProgressBar;
class UButton;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuinmatesTechSliderEvent, float, Value);

/** Native slider input with independent V4 track, masked fill and thumb. */
UCLASS()
class RUINMATES_API URuinmatesTechSlider : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Updates the native slider and clipped fill together. */
	void SetValue(float Value);

	/** Maps a real value range onto the normalised track (used by the split dialog). */
	void SetValueRange(float Min, float Max, float Value);

	/** Current value in the mapped range, rounded to whole units. */
	float GetValue() const;

	/** Broadcast whenever the mapped value changes. */
	UPROPERTY(BlueprintAssignable, Category = "RuinMates|UI")
	FRuinmatesTechSliderEvent OnValueChanged;
protected:
	virtual void NativeOnInitialized() override;
private:
	UFUNCTION() void ValueChanged(float Value);
	UPROPERTY() TObjectPtr<USlider> Input;
	UPROPERTY() TObjectPtr<UProgressBar> Fill;
	float MinValue = 0.f;
	float MaxValue = 1.f;
};

/** Focusable boolean control with a fixed track and interruptible 150ms thumb motion. */
UCLASS()
class RUINMATES_API URuinmatesTechToggle : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Starts a visual transition from the current thumb position. */
	void SetChecked(bool bInChecked);
	bool IsChecked() const { return bChecked; }
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& Geometry, float Delta) override;
private:
	UFUNCTION() void Clicked();
	UPROPERTY() TObjectPtr<UButton> Input;
	UPROPERTY() TObjectPtr<UImage> OffThumb;
	UPROPERTY() TObjectPtr<UImage> OnThumb;
	bool bChecked = false;
	float Position = 0.f;
	float Start = 0.f;
	float Elapsed = 0.15f;
};
