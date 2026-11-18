#include "UI/RuinmatesCrosshairWidget.h"

#include "Rendering/DrawElements.h"
#include "Styling/WidgetStyle.h"

void URuinmatesCrosshairWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

int32 URuinmatesCrosshairWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	if (Size.X < 2.f || Size.Y < 2.f)
	{
		return LayerId;
	}

	const FVector2D Center = Size * 0.5f;
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();
	const FLinearColor Tint = Color * InWidgetStyle.GetColorAndOpacityTint();

	auto DrawLine = [&](const FVector2D& From, const FVector2D& To)
	{
		TArray<FVector2D> Points;
		Points.Add(From);
		Points.Add(To);
		FSlateDrawElement::MakeLines(OutDrawElements, LayerId, PaintGeometry, Points, ESlateDrawEffect::None, Tint, true, Thickness);
	};

	DrawLine(Center + FVector2D(-Gap - LineLength, 0.f), Center + FVector2D(-Gap, 0.f));
	DrawLine(Center + FVector2D(Gap, 0.f), Center + FVector2D(Gap + LineLength, 0.f));
	DrawLine(Center + FVector2D(0.f, -Gap - LineLength), Center + FVector2D(0.f, -Gap));
	DrawLine(Center + FVector2D(0.f, Gap), Center + FVector2D(0.f, Gap + LineLength));
	return LayerId + 1;
}
