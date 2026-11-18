#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/RuinmatesTerminalTypes.h"
#include "RuinmatesTerminalScreen.generated.h"

class UTextBlock;
class UProgressBar;
class UUniformGridPanel;
class UWidgetSwitcher;
class UVerticalBox;
class UButton;
class UBorder;
class UCanvasPanel;
class UImage;
class UHorizontalBox;
class UOverlaySlot;
class UScrollBox;
class ARuinmatesCharacter;
class ARuinmatesGameState;
class URuinmatesTerminalItemSlot;
class ARuinmatesLootContainer;

// Shared terminal. The left backpack stays put; the right switcher shows the character or a container.
// Layout: world dim -> metal device shell (9-slice) -> header -> tab bar -> body (backpack | right)
// -> footer key hints. All art comes from RuinmatesUIStyle brushes; see Documentation/UI_Terminal.md.
UCLASS()
class RUINMATES_API URuinmatesTerminalScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenFor(ARuinmatesCharacter* InCharacter, ARuinmatesLootContainer* InContainer);
	void ShowPreview(bool bSearchPage);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildChrome();
	void BuildHeader(UVerticalBox* Column);
	void BuildTabs(UVerticalBox* Column);
	void BuildBody(UVerticalBox* Column);
	void BuildFooter(UVerticalBox* Column);
	void BuildMenuLayer(UCanvasPanel* Canvas);

	void RebuildSlots();

	/** Reuses existing cells; only adds or removes the difference so hover state survives refreshes. */
	void SyncGrid(UUniformGridPanel* Grid, TArray<TObjectPtr<URuinmatesTerminalItemSlot>>& Cells,
		const TArray<FRuinmatesTerminalSlotView>& Views, int32 Columns, bool bLoot);

	/** Derives cell sizes from the real panel width (design units, DPI already applied by Slate). */
	void UpdateCellSizes(bool bForce);

	/** Clamps the shell on ultrawide screens so the terminal keeps a 16:9-ish body. */
	void UpdateShellBounds(const FGeometry& MyGeometry);

	void RebuildFooterHints(bool bSearch);
	void BeginOpenFade();
	void ApplyPageText();
	void RefreshTabStyles();

	UFUNCTION()
	void ClosePreview();

	UFUNCTION()
	void SelectAll();

	UFUNCTION()
	void SelectConsumable();

	UFUNCTION()
	void SelectMaterial();

	UFUNCTION()
	void SelectQuest();

	UFUNCTION()
	void HandleSlotClicked(int32 StableIndex);

	// Right click on a revealed backpack stack opens the small action menu.
	UFUNCTION()
	void HandleSlotRightClicked(int32 StableIndex);

	UFUNCTION()
	void HandleMenuDiscard();

	UFUNCTION()
	void HandleMenuDismiss();

	void OpenSlotMenu(int32 StableIndex);
	void CloseSlotMenu();

	UFUNCTION()
	void HandleLootClicked(int32 StableIndex);

	UFUNCTION()
	void HandleTakeAll();

	void SetFilter(FName InFilter);
	void BindSources();
	void UnbindSources();
	void RefreshFromLive();

	void Refresh();

	FRuinmatesTerminalPreviewPage Page;
	FName ActiveFilter = NAME_None;
	int32 SelectedStableIndex = INDEX_NONE;
	bool bLive = false;
	bool bClosing = false;

	TWeakObjectPtr<ARuinmatesCharacter> LiveCharacter;
	TWeakObjectPtr<ARuinmatesLootContainer> LiveContainer;

	UPROPERTY()
	TObjectPtr<UTextBlock> PageTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> ClockText;

	UPROPERTY()
	TObjectPtr<UTextBlock> PackTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> CapacityText;

	UPROPERTY()
	TObjectPtr<UProgressBar> CapacityBar;

	UPROPERTY()
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BackpackGrid;

	UPROPERTY()
	TObjectPtr<UWidgetSwitcher> RightSwitcher;

	UPROPERTY()
	TObjectPtr<UTextBlock> RightTitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> RightSubtitle;

	UPROPERTY()
	TObjectPtr<UTextBlock> LootTitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> LootCountText;

	UPROPERTY()
	TObjectPtr<UProgressBar> SearchBar;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> LootGrid;

	UPROPERTY()
	TObjectPtr<UButton> TakeAllButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterLeft;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterRight;

	UPROPERTY()
	TArray<TObjectPtr<class URuinmatesTerminalItemSlot>> BackpackSlots;

	UPROPERTY()
	TArray<TObjectPtr<class URuinmatesTerminalItemSlot>> LootSlots;

	UPROPERTY()
	TObjectPtr<UScrollBox> BackpackScroll;

	UPROPERTY()
	TObjectPtr<UScrollBox> LootScroll;

	UPROPERTY()
	TObjectPtr<UTextBlock> FundsText;

	UPROPERTY()
	TObjectPtr<UWidget> FundsChip;

	UPROPERTY()
	TObjectPtr<UTextBlock> SearchStatusText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TakeAllLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> CapacityUsedText;

	UPROPERTY()
	TObjectPtr<UTextBlock> EquipBackpackText;

	UPROPERTY()
	TObjectPtr<UTextBlock> EquipPrimaryText;

	UPROPERTY()
	TObjectPtr<UHorizontalBox> FooterHints;

	/** Root shell; its overlay slot padding is widened on ultrawide displays. */
	UPROPERTY()
	TObjectPtr<UWidget> ShellRoot;

	TWeakObjectPtr<ARuinmatesGameState> LiveGameState;

	float LastBackpackWidth = 0.f;
	float LastLootWidth = 0.f;
	FVector2D LastViewSize = FVector2D::ZeroVector;
	float FadeTime = 1.f;
	int32 FooterMode = INDEX_NONE;

	// Header chrome: close button plus one button / label / underline per category tab.
	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	UPROPERTY()
	TArray<TObjectPtr<UButton>> TabButtons;

	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> TabLabels;

	UPROPERTY()
	TArray<TObjectPtr<UImage>> TabIcons;


	UPROPERTY()
	TArray<FName> TabFilters;

	// Full-screen layer above the shell: a dismiss catcher plus the slot action menu.
	UPROPERTY()
	TObjectPtr<UCanvasPanel> MenuLayer;

	UPROPERTY()
	TObjectPtr<UBorder> ContextMenu;

	UPROPERTY()
	TObjectPtr<UButton> MenuDismissButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> MenuTitle;

	// Stable backpack index the open menu points at; INDEX_NONE means the menu is closed.
	int32 MenuStableIndex = INDEX_NONE;
};
