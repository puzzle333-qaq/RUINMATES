#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RuinmatesCrosshairWidget.generated.h"

// Center-screen cross. It does not take mouse hits.
UCLASS()
class RUINMATES_API URuinmatesCrosshairWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Crosshair")
	float LineLength = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Crosshair")
	float Gap = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Crosshair")
	float Thickness = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "RuinMates|Crosshair")
	FLinearColor Color = FLinearColor::White;
};
