#include "UI/RuinmatesTechButton.h"

#include "UI/RuinmatesUIStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"

/** Slate surface owns its brushes for the full lifetime of the paint references. */
class SRuinmatesTechButtonSurface : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRuinmatesTechButtonSurface) {}
	SLATE_END_ARGS()

	void Construct(const FArguments&, URuinmatesTechButton* InOwner, TSharedRef<SButton> Button, bool bTab, bool bDanger)
	{
		Owner = InOwner;
		Input = Button;
		const FString Group = bTab ? TEXT("Art/Navigation/T_UI_Tab_") : TEXT("Art/Buttons/T_UI_Button_");
		Base = RuinmatesUI::V4(*(Group + (bDanger ? TEXT("Danger") : TEXT("Normal"))));
		Disabled = RuinmatesUI::V4(*(Group + TEXT("Disabled")));
		Glow = RuinmatesUI::V4(*(Group + TEXT("Glow")));
		// Danger keeps its red base; cyan glow never washes out the danger meaning.
		bRed = bDanger;
		SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
		ChildSlot
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SImage).Image_Lambda([this]() { return Owner.IsValid() && Owner->GetIsEnabled() ? &Base : &Disabled; })
				.Visibility(EVisibility::HitTestInvisible)
			]
			+ SOverlay::Slot()
			[
				SAssignNew(GlowImage, SImage).Image(&Glow).Visibility(EVisibility::HitTestInvisible)
			]
			+ SOverlay::Slot()[Button]
		];
		GlowImage->SetRenderOpacity(0.f);
	}

	virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override
	{
		SCompoundWidget::Tick(Geometry, Time, Delta);
		const bool bEnabled = Owner.IsValid() && Owner->GetIsEnabled();
		const bool bPressed = bEnabled && Input->IsPressed();
		const bool bSelected = bEnabled && Owner->IsSelected();
		float Target = bEnabled && (Input->IsHovered() || Input->HasKeyboardFocus()) ? 0.55f : 0.f;
		Target = bSelected ? 1.f : Target;
		Target = bPressed ? FMath::Max(Target, 0.85f) : Target;
		Target = bRed ? 0.f : Target;
		const float TargetScale = bPressed ? 0.98f : 1.f;
		if (Target != GlowTarget || TargetScale != ScaleTarget)
		{
			GlowStart = GlowValue;
			ScaleStart = ScaleValue;
			GlowTarget = Target;
			ScaleTarget = TargetScale;
			Elapsed = 0.f;
			Duration = !bEnabled ? 0.001f : bPressed ? 0.06f : bSelected ? 0.15f : Target > GlowValue ? 0.12f : 0.18f;
		}
		if (Elapsed < Duration)
		{
			Elapsed = FMath::Min(Duration, Elapsed + Delta);
			const float Alpha = 1.f - FMath::Square(1.f - Elapsed / Duration);
			GlowValue = FMath::Lerp(GlowStart, GlowTarget, Alpha);
			ScaleValue = FMath::Lerp(ScaleStart, ScaleTarget, FMath::Min(1.f, Elapsed / (bPressed ? 0.06f : 0.10f)));
			GlowImage->SetRenderOpacity(GlowValue);
			SetRenderTransform(FSlateRenderTransform(FScale2D(ScaleValue)));
		}
	}

private:
	TWeakObjectPtr<URuinmatesTechButton> Owner;
	TSharedPtr<SButton> Input;
	TSharedPtr<SImage> GlowImage;
	FSlateBrush Base, Disabled, Glow;
	bool bRed = false;
	float GlowStart = 0.f, GlowValue = 0.f, GlowTarget = 0.f;
	float ScaleStart = 1.f, ScaleValue = 1.f, ScaleTarget = 1.f;
	float Elapsed = 1.f, Duration = 0.12f;
};

void URuinmatesTechButton::SetPresentation(bool bInTab, bool bInDanger)
{
	bTab = bInTab;
	bDanger = bInDanger;
}

void URuinmatesTechButton::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
}

TSharedRef<SWidget> URuinmatesTechButton::RebuildWidget()
{
	const FString Group = bTab ? TEXT("Art/Navigation/T_UI_Tab_") : TEXT("Art/Buttons/T_UI_Button_");
	RetainedArt = {RuinmatesUI::V4(*(Group + (bDanger ? TEXT("Danger") : TEXT("Normal")))),
		RuinmatesUI::V4(*(Group + TEXT("Disabled"))), RuinmatesUI::V4(*(Group + TEXT("Glow")))};
	FButtonStyle Transparent;
	Transparent.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	Transparent.Hovered = Transparent.Normal;
	Transparent.Pressed = Transparent.Normal;
	Transparent.Disabled = Transparent.Normal;
	Transparent.NormalPadding = FMargin(0.f);
	Transparent.PressedPadding = FMargin(0.f);
	SetStyle(Transparent);
	Super::RebuildWidget();
	return SAssignNew(Surface, SRuinmatesTechButtonSurface, this, MyButton.ToSharedRef(), bTab, bDanger);
}

void URuinmatesTechButton::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Surface.Reset();
}
