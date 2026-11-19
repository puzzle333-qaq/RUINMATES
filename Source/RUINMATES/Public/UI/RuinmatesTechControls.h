#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesTechControls.generated.h"

class USlider;
class UProgressBar;
class UButton;
class UImage;

/** Native slider input with independent V4 track, masked fill and thumb. */
UCLASS()
class RUINMATES_API URuinmatesTechSlider : public UUserWidget
{
	GENERATED_BODY()
public:
	/** Updates the native slider and clipped fill together. */
	void SetValue(float Value);
protected:
	virtual void NativeOnInitialized() override;
private:
	UFUNCTION() void ValueChanged(float Value);
	UPROPERTY() TObjectPtr<USlider> Input;
	UPROPERTY() TObjectPtr<UProgressBar> Fill;
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
