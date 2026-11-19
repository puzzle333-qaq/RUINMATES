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
class ARuinmatesMerchant;
class URuinmatesCharacterPreview;
class URuinmatesSquadRadio;

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
	/** Opens an isolated V4 visual page: inventory, search, storage, market, components. */
	void ShowVisualPreview(int32 Mode, int32 PreviewCapacity = 20);

	/** Opens the V4 market bound to a live merchant. Reuses the existing trade backend. */
	void OpenMarket(ARuinmatesCharacter* InCharacter, ARuinmatesMerchant* InMerchant);

	/** True while the market is bound to a live merchant (not the isolated preview). */
	bool IsMarketLive() const { return bMarketLive; }

	/** Server trade result forwarded by the owning character. */
	void ShowTradeResult(bool bSuccess, const FText& Message);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual bool NativeSupportsKeyboardFocus() const override { return true; }
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildChrome();
	void BuildV4Chrome();
	void SetVisualMode(int32 Mode);
	void RefreshVisualPage();
	UFUNCTION() void PreviewInventory();
	UFUNCTION() void PreviewSearch();
	UFUNCTION() void PreviewStorage();
	UFUNCTION() void PreviewMarket();
	UFUNCTION() void PreviewComponents();
	UFUNCTION() void PreviewBuy();
	UFUNCTION() void PreviewSell();
	UFUNCTION() void PreviewQuantityUp();
	UFUNCTION() void PreviewQuantityDown();
	UFUNCTION() void PreviewMarketSelected(int32 Index);

	// ---- Live market ------------------------------------------------------------------------
	/** Builds one display view from a real stack (single implementation shared with the market). */
	void MakeSlotView(const FRuinmatesItemStack& Stack, int32 StableIndex, FRuinmatesTerminalSlotView& OutView) const;

	UFUNCTION() void HandleMarketBuyMode();
	UFUNCTION() void HandleMarketSellMode();
	UFUNCTION() void HandleMarketQuantityUp();
	UFUNCTION() void HandleMarketQuantityDown();
	UFUNCTION() void HandleMarketConfirm();
	UFUNCTION() void HandleMarketSelected(int32 Index);

	void RefreshMarketLive();
	void ClearMarketSelection();
	bool IsMarketSelectionValid() const;
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

	UFUNCTION() void SelectAmmo();
	UFUNCTION() void SelectValuable();
	UFUNCTION() void SelectWeapon();
	UFUNCTION() void SelectEquipment();

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
	/** Local overlays temporarily hidden while this terminal owns the screen. */
	TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> HiddenOverlays;
	/** Preview navigation states share the same independent glow as category tabs. */
	UPROPERTY() TArray<TObjectPtr<class URuinmatesTechButton>> VisualTabs;
	UPROPERTY() TObjectPtr<class URuinmatesTechButton> VisualBuy;
	UPROPERTY() TObjectPtr<class URuinmatesTechButton> VisualSell;

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

	/** V4 keeps one 1920x1080 logical canvas, fitted after the viewport's DPI transform. */
	UPROPERTY() TObjectPtr<UCanvasPanel> DesignCanvas;
	UPROPERTY() TObjectPtr<UWidget> PreviewNavigation;
	UPROPERTY() TObjectPtr<UTextBlock> PreviewBadge;
	UPROPERTY() TObjectPtr<UUniformGridPanel> MarketGrid;
	UPROPERTY() TArray<TObjectPtr<URuinmatesTerminalItemSlot>> MarketCells;
	UPROPERTY() TObjectPtr<UTextBlock> MarketName;
	UPROPERTY() TObjectPtr<UTextBlock> MarketPrice;
	UPROPERTY() TObjectPtr<UTextBlock> MarketQuantity;
	UPROPERTY() TObjectPtr<UTextBlock> MarketTotal;
	UPROPERTY() TObjectPtr<UWidget> MarketGoods;
	UPROPERTY() TObjectPtr<UTextBlock> SellPrompt;
	UPROPERTY() TObjectPtr<UTextBlock> CategoryEmptyHint;
	UPROPERTY() TObjectPtr<URuinmatesCharacterPreview> CharacterPreview;
	UPROPERTY() TObjectPtr<URuinmatesSquadRadio> SquadRadio;

	// ---- Live market (V4 page bound to a merchant) ------------------------------------------
	/** Confirm button and one line status area; the rest of the market page is shared with the preview. */
	UPROPERTY() TObjectPtr<class URuinmatesTechButton> MarketConfirmButton;
	UPROPERTY() TObjectPtr<UTextBlock> MarketStatusText;

	/** Item ids currently shown in the goods grid (empty in sell mode). */
	TArray<FName> MarketGoodsIds;

	TWeakObjectPtr<ARuinmatesMerchant> LiveMerchant;
	bool bMarketLive = false;

	int32 VisualMode = 0;
	int32 VisualQuantity = 1;
	int32 VisualSelected = INDEX_NONE;
	bool bVisualSell = false;
	float PageFadeTime = 1.f;
	float FilterFadeTime = 1.f;
};
