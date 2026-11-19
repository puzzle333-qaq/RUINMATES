#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "RuinmatesTerminalItemSlot.generated.h"

class USizeBox;
class UButton;
class UImage;
class UTextBlock;
class UProgressBar;
class UHorizontalBox;
class UWidget;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuinmatesTerminalSlotClicked, int32, StableIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRuinmatesTerminalSlotRightClicked, int32, StableIndex);

/**
 * One terminal cell, shared by the backpack grid (square) and the container cards (tall).
 *
 * Hierarchy (root first):
 *   SizeBox (exact cell size)
 *     Button  (whole cell is the hit target; content slot is Fill/Fill with zero padding)
 *       Overlay
 *         QualityGlow, Hatch, ScanBand, EmptyMark, Icon, UnknownIcon, NameStrip, Name, Count,
 *         RarityPips, StatusText, ScanBar, QualityFrame, Selection     (all HitTestInvisible)
 *
 * The plate itself (empty / filled / hover / pressed / searching / unknown) is the Button's
 * FButtonStyle, so hover and pressed come from Slate with no per-frame work.
 */
UCLASS()
class RUINMATES_API URuinmatesTerminalItemSlot : public UUserWidget
{
	GENERATED_BODY()

public:
	FRuinmatesTerminalSlotClicked OnSlotClicked;

	/** Right click opens the slot action menu on the screen; only revealed slots raise it. */
	FRuinmatesTerminalSlotRightClicked OnSlotRightClicked;

	/** Size and presentation style. Safe to call any time; only touches the size box when unchanged layout. */
	void Configure(const FVector2D& InSize, ERuinmatesTerminalSlotLayout InLayout);

	/** Applies new data. Cheap when nothing changed, so the screen can call it on every refresh. */
	void ApplyView(const FRuinmatesTerminalSlotView& InView);

	const FRuinmatesTerminalSlotView& GetView() const { return View; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	void BuildTree();
	void RefreshVisual();
	void RefreshLayoutMetrics();
	void ApplyPlateStyle();

	UPROPERTY()
	TObjectPtr<USizeBox> SlotSize;

	UPROPERTY()
	TObjectPtr<UButton> HitButton;

	UPROPERTY()
	TObjectPtr<UImage> QualityGlow;

	UPROPERTY()
	TObjectPtr<UImage> Hatch;

	UPROPERTY()
	TObjectPtr<UImage> ScanBand;

	UPROPERTY()
	TObjectPtr<UImage> EmptyMark;

	UPROPERTY()
	TObjectPtr<USizeBox> IconBox;

	UPROPERTY()
	TObjectPtr<UImage> ItemIcon;

	UPROPERTY()
	TObjectPtr<USizeBox> UnknownBox;

	UPROPERTY()
	TObjectPtr<UImage> UnknownIcon;

	UPROPERTY()
	TObjectPtr<UImage> NameStrip;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> CountText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> RarityPips;

	UPROPERTY()
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY()
	TObjectPtr<UProgressBar> ScanBar;

	UPROPERTY()
	TObjectPtr<UImage> QualityFrame;

	UPROPERTY()
	TObjectPtr<UImage> Selection;
	/** Independent four-finish slashes remain visible beneath interaction highlights. */
	UPROPERTY() TObjectPtr<UImage> FinishPips;
	/** Native tooltip content retains the shared V4 panel and readable body font. */
	UPROPERTY() TObjectPtr<USizeBox> DetailTooltip;
	UPROPERTY() TObjectPtr<UTextBlock> DetailText;

	FRuinmatesTerminalSlotView View;
	FVector2D CellSize = FVector2D(168.f, 112.f);
	ERuinmatesTerminalSlotLayout Layout = ERuinmatesTerminalSlotLayout::Grid;
	FSoftObjectPath AppliedIcon;
	int32 AppliedPlate = INDEX_NONE;
	bool bHovered = false;

	/** Visual-only scan loop; the server reveal timer is the only real progress. */
	float ScanTime = 0.f;

	/** Seconds left of the one-shot edge flash played when a Rare+ item is revealed. */
	float RevealFlash = 0.f;
};