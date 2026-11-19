#include "UI/RuinmatesTechControls.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/Slider.h"
#include "UI/RuinmatesUIStyle.h"

void URuinmatesTechSlider::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
	WidgetTree->RootWidget = Root;
	Fill = WidgetTree->ConstructWidget<UProgressBar>();
	FProgressBarStyle Style;
	Style.BackgroundImage = RuinmatesUI::V4(TEXT("Art/Controls/T_UI_SliderTrack"));
	Style.FillImage = RuinmatesUI::V4(TEXT("Art/Controls/T_UI_SliderFill"));
	Fill->SetWidgetStyle(Style);
	Fill->SetFillColorAndOpacity(FLinearColor::White);
	Fill->SetBarFillStyle(EProgressBarFillStyle::Mask);
	Fill->SetBorderPadding(FVector2D::ZeroVector);
	Fill->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UOverlaySlot* FillSlot = Root->AddChildToOverlay(Fill))
	{
		FillSlot->SetHorizontalAlignment(HAlign_Fill);
		FillSlot->SetVerticalAlignment(VAlign_Fill);
		FillSlot->SetPadding(FMargin(12.f));
	}
	Input = WidgetTree->ConstructWidget<USlider>();
	FSliderStyle InputStyle;
	InputStyle.NormalBarImage.DrawAs = ESlateBrushDrawType::NoDrawType;
	InputStyle.HoveredBarImage = InputStyle.NormalBarImage;
	InputStyle.DisabledBarImage = InputStyle.NormalBarImage;
	InputStyle.NormalThumbImage = RuinmatesUI::V4(TEXT("Art/Controls/T_UI_SliderThumb"));
	InputStyle.HoveredThumbImage = InputStyle.NormalThumbImage;
	InputStyle.DisabledThumbImage = InputStyle.NormalThumbImage;
	Input->SetWidgetStyle(InputStyle);
	Input->OnValueChanged.AddDynamic(this, &URuinmatesTechSlider::ValueChanged);
	if (UOverlaySlot* InputSlot = Root->AddChildToOverlay(Input))
	{
		InputSlot->SetHorizontalAlignment(HAlign_Fill);
		InputSlot->SetVerticalAlignment(VAlign_Fill);
	}
	SetValue(0.5f);
}

void URuinmatesTechSlider::SetValue(float Value)
{
	Value = FMath::Clamp(Value, 0.f, 1.f);
	if (Input) Input->SetValue(Value);
	ValueChanged(Value);
}

void URuinmatesTechSlider::SetValueRange(float Min, float Max, float Value)
{
	MinValue = Min;
	MaxValue = FMath::Max(Min + 1.f, Max);
	const float Range = FMath::Max(KINDA_SMALL_NUMBER, MaxValue - MinValue);
	SetValue(FMath::Clamp((Value - MinValue) / Range, 0.f, 1.f));
}

float URuinmatesTechSlider::GetValue() const
{
	const float Normalized = Input ? Input->GetValue() : 0.f;
	const float Raw = MinValue + FMath::Clamp(Normalized, 0.f, 1.f) * (MaxValue - MinValue);
	return FMath::Clamp(FMath::RoundToFloat(Raw), MinValue, MaxValue);
}

void URuinmatesTechSlider::ValueChanged(float Value)
{
	if (Fill) Fill->SetPercent(FMath::Clamp(Value, 0.f, 1.f));
	OnValueChanged.Broadcast(MinValue + FMath::Clamp(Value, 0.f, 1.f) * (MaxValue - MinValue));
}

void URuinmatesTechToggle::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	Input = WidgetTree->ConstructWidget<UButton>();
	WidgetTree->RootWidget = Input;
	FButtonStyle Style;
	Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Style.Hovered = Style.Normal;
	Style.Pressed = Style.Normal;
	Style.Disabled = Style.Normal;
	Style.NormalPadding = FMargin(0.f);
	Style.PressedPadding = FMargin(0.f);
	Input->SetStyle(Style);
	Input->OnClicked.AddDynamic(this, &URuinmatesTechToggle::Clicked);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	Input->AddChild(Canvas);
	if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Canvas->Slot))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Fill);
		ContentSlot->SetVerticalAlignment(VAlign_Fill);
		ContentSlot->SetPadding(FMargin(0.f));
	}
	auto AddArt = [&](const TCHAR* Path, float X, float Y, float W, float H)
	{
		UImage* Art = WidgetTree->ConstructWidget<UImage>();
		Art->SetBrush(RuinmatesUI::V4(Path));
		UCanvasPanelSlot* ArtSlot = Canvas->AddChildToCanvas(Art);
		ArtSlot->SetPosition(FVector2D(X,Y));
		ArtSlot->SetSize(FVector2D(W,H));
		return Art;
	};
	AddArt(TEXT("Art/Controls/T_UI_Toggle_Track"),0,0,128,52);
	OffThumb = AddArt(TEXT("Art/Controls/T_UI_Toggle_Thumb_Off"),2,2,48,48);
	OnThumb = AddArt(TEXT("Art/Controls/T_UI_Toggle_Thumb_On"),2,2,48,48);
	OnThumb->SetRenderOpacity(0.f);
}

void URuinmatesTechToggle::SetChecked(bool bInChecked)
{
	if (bChecked == bInChecked) return;
	bChecked = bInChecked;
	Start = Position;
	Elapsed = 0.f;
}

void URuinmatesTechToggle::Clicked()
{
	SetChecked(!bChecked);
}

void URuinmatesTechToggle::NativeTick(const FGeometry& Geometry, float Delta)
{
	Super::NativeTick(Geometry,Delta);
	if (Elapsed >= 0.15f || !OffThumb || !OnThumb) return;
	Elapsed = FMath::Min(0.15f,Elapsed+Delta);
	const float Alpha = 1.f-FMath::Square(1.f-Elapsed/0.15f);
	Position = FMath::Lerp(Start,bChecked?1.f:0.f,Alpha);
	OffThumb->SetRenderTranslation(FVector2D(Position*76.f,0));
	OnThumb->SetRenderTranslation(FVector2D(Position*76.f,0));
	OffThumb->SetRenderOpacity(1.f-Position);
	OnThumb->SetRenderOpacity(Position);
}
