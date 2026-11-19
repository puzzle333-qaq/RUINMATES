# Phase 1B — Astra market integration handoff

Baseline: `e7d415b` (Phase 1B maintenance + test-map work sits on top).
Scope of this note: read-only inventory of what exists, what can be reused, and what a new
market backend must not break. **No new market backend, storage, or split/auto-sort logic
exists in the project.**

---

## 1. V4 market / storage UI (visual only today)

| What | Where |
|---|---|
| Terminal screen, all modes | `Source/RUINMATES/Private/UI/RuinmatesTerminalScreen.cpp`, `Public/UI/RuinmatesTerminalScreen.h` |
| V4 terminal layout helpers | `Private/UI/RuinmatesTerminalV4Layout.cpp` |
| Item cells | `Private/UI/RuinmatesTerminalItemSlot.cpp` |
| Brushes / colours / fonts | `Private/UI/RuinmatesUIStyle.cpp`, `Public/UI/RuinmatesUIStyle.h` |
| Isolated preview entry points | `Ruinmates.PreviewMarket`, `Ruinmates.PreviewStorage`, `Ruinmates.PreviewBuy`, `Ruinmates.PreviewSell` console commands (bottom of `RuinmatesTerminalScreen.cpp`) |

State that already exists on the screen:

- `VisualMode` (`0` inventory, `1` search, `2` storage, `3` market, `4` components) and
  `SetVisualMode(int32)`.
- Market widgets: `MarketGrid`, `MarketCells`, `MarketName`, `MarketPrice`, `MarketQuantity`,
  `MarketTotal`, `MarketGoods`, `SellPrompt`, plus `PreviewBuy/PreviewSell/PreviewQuantityUp/
  PreviewQuantityDown/PreviewMarketSelected`.
- These are **preview-only**: they read sample rows from `RuinmatesTerminalPreview` and every
  confirm/transfer control is explicitly disabled. There is no client→server request behind them.

Reuse for the market UI: the whole shell, tab bar, item cell, category filter
(`RuinmatesTerminalDisplay::FilterBackpackSlots`) and `RuinmatesTerminalV4Layout` helpers. The
preview rows must be replaced with server data; do not add a second item-cell implementation.

## 2. Merchant trade: entry, RPC and pricing

| Layer | Symbol | File |
|---|---|---|
| Actor | `ARuinmatesMerchant` (`MerchantName`, `InteractionDistance`, `MerchantItems` (ItemIds), `AllowsSelling`, `SellsItem`) | `Public/Inventory/RuinmatesMerchant.h`, `Private/Inventory/RuinmatesMerchant.cpp` |
| Interaction | `IInteractable::CanInteract/GetInteractPrompt/Interact`; `ARuinmatesCharacter::TryInteract` (trace + overlap fallback that accepts any interactable) | `Private/Character/RuinmatesCharacter.cpp` |
| Open/close | `ARuinmatesCharacter::BeginTrade/EndTrade`, `Server_EndTrade`, `OnRep_ActiveMerchant`, `OpenTradeScreen/CloseTradeScreen` | same file |
| Buy | `RequestBuyItem(FName ItemId, int32 Count)` → `Server_BuyItem` | same file |
| Sell | `RequestSellItem(int32 SlotIndex, int32 Count)` → `Server_SellItem` | same file |
| Result | `Client_TradeResult(bool, const FText&)` → `URuinmatesTradeScreen::ShowResult` | same files |
| Prices | `URuinmatesItemDatabaseSubsystem::GetItemBuyPrice/GetItemSellPrice/FindItemDefinition` | `Public/Inventory/RuinmatesItemDatabaseSubsystem.h` |
| Trade UI | `URuinmatesTradeScreen` (buy list / sell list / quantity / result line) | `Public/UI/RuinmatesTradeScreen.h`, `Private/UI/RuinmatesTradeScreen.cpp` |

Server rules already enforced (keep them for any new market path):

- Client sends only `ItemId`/slot index + quantity. Price, quality and stack size are read from
  DT_Items on the server.
- `ValidateTradeState` re-checks the active merchant and interaction distance on every request.
- Buy is all-or-nothing: wallet check + `CanAddExactCount` before charging; refund if delivery
  fails. Sell reads the stack from the server backpack, then `RemoveItemAt` + `AddFunds`.
- Item ids are resolved through the database, so legacy ids/names still work
  (`ResolveItemId`, `NormalizeStack`).

## 3. Team funds

`ARuinmatesGameState` (`Public/Game/RuinmatesGameState.h`, `Private/Game/RuinmatesGameState.cpp`):

- `GetSharedFunds()`, `HasEnoughFunds(int32)`, `TrySpendFunds(int32)`, `AddFunds(int32)`,
  `SetSharedFunds(int32)` (server-only guards, clamps at `[0, INT32_MAX]`).
- `SharedFunds` is `ReplicatedUsing=OnRep_SharedFunds`; `OnSharedFundsChanged` is the UI delegate.
- Every mutation mirrors the in-memory campaign via `URuinmatesSaveSubsystem::SetSharedFunds`
  (no disk write per transaction).
- Game mode sets `GameStateClass = ARuinmatesGameState`; `ARuinmatesPlayerState::GetTeamFunds/
  AddTeamFunds` remain as compatibility wrappers only.

The market must spend through `TrySpendFunds` and never keep a second balance.

## 4. Backpack and capacity

`URuinmatesBackpackComponent` (`Public/Inventory/RuinmatesBackpackComponent.h`):

- Read: `GetSlots()`, `GetCapacity()`, `GetUsedSlotCount()`, `GetLootValue()`, `GetRevision()`,
  `GetAvailableCapacityForItem(Stack)`, `CanAddExactCount(Stack, Count)`.
- Write (server only): `TryAdd`, `TryAddExact`, `RemoveItemAt`, `DiscardSlot`, `TryMoveSlot`.
- `Revision` is a replicated counter used to reject moves built from stale UI snapshots
  (`CanMoveSlotSnapshot`); `TryMoveSlot` also validates both slot snapshots.
- `OnChanged` is the single refresh signal for the UI; `OnRep_Slots` re-broadcasts it.
- Stacks are canonicalised through the item database on add (`NormalizeStack`), so callers may pass
  legacy ids/names.

A shared stash should be a **separate storage object**, not a second wallet-like global: reuse
`FRuinmatesItemStack`, the same stacking helpers and the same `Revision` guard, and keep the server
authoritative.

## 5. Save structure

- `URuinmatesSaveGame` (`Public/Save/RuinmatesSaveGame.h`): `SharedFunds`, member array
  (`StableId`, `Level`, `TalentIds`, `EquippedLoadoutId`). **Backpack contents are not saved.**
- `URuinmatesSaveSubsystem` (`Public/Save/RuinmatesSaveSubsystem.h`): `SaveSlotName`
  (`RuinMatesCampaign`), `GetSharedFunds/SetSharedFunds`, `SetMemberProgress/GetMemberProgress`,
  `SaveCampaign/LoadCampaign`, `ResetInMemoryCampaign`, `CapturePartyFromWorld(UWorld*)`,
  `RestoreOrAssignMember(PlayerState*)`, `CanMutateSave()`.
- Flow: GameState restores funds in `BeginPlay`; `CapturePartyFromWorld` mirrors the wallet and every
  member's progress before a travel/save; `PostLogin` restores members.

A market that persists purchases must extend this class (new fields only) and go through the save
subsystem; clients must never mutate the campaign (`CanMutateSave()` is false for them).

## 6. Level travel / extraction

`URuinmatesSessionSubsystem` (`Public/Online/RuinmatesSessionSubsystem.h`):

- `HostListenParty()`, `LeaveParty()`, `TravelToDangerZone()`, `TravelToBase()`,
  `HostServerTravel(FName MapName, ERuinmatesTravelStage)` (protected), `GetStatusText()`,
  `IsInParty()`, `IsPartyHost()`, `IsUsingPlatformOnline()`, `CanOpenPlatformInvite()`.
- Config: `BaseMapName`, `DangerMapName` (empty names refuse travel).
- Travel saves first (`CapturePartyFromWorld` + `SaveCampaign`), then `ServerTravel` to
  `Map?listen`.

There is **no extraction mechanic** beyond this travel pair: no countdown, no extraction zone actor,
no raid result screen. A market at the base can hang off `TravelToBase`/base map only; do not invent
an extraction flow in the market task.

## 7. Directly reusable functions for the new market

- Prices / definitions: `FindItemDefinition`, `GetItemBuyPrice`, `GetItemSellPrice`,
  `CreateItemStack`, `NormalizeStack`, `GetAvailableCategories`.
- Wallet: `HasEnoughFunds`, `TrySpendFunds`, `AddFunds`, `OnSharedFundsChanged`.
- Inventory: `CanAddExactCount`, `TryAddExact`, `GetSlots`, `GetRevision`, `OnChanged`.
- Merchant gating: `ARuinmatesMerchant::SellsItem/AllowsSelling/GetInteractionDistance` and
  `ARuinmatesCharacter::ValidateTradeState` (consider extracting it for a market actor).
- UI: terminal shell, `URuinmatesTerminalItemSlot`, `RuinmatesTerminalDisplay::FilterBackpackSlots`,
  `RuinmatesUI::V4/ImageFor/TextureAt` brushes.
- Networking pattern to copy: `RequestX` client wrapper → `Server_X` reliable RPC → validation →
  state mutation → `Client_TradeResult`. Do not send prices from the client.

## 8. Files a shared stash would likely touch

- New: `Public/Inventory/RuinmatesStashComponent.h` + `.cpp` (or a stash actor with the same
  component), plus a storage panel widget.
- `Public/Save/RuinmatesSaveGame.h` + `Private/Save/RuinmatesSaveSubsystem.cpp`: persist stash slots.
- `Public/Game/RuinmatesGameState.h` (or a game-instance subsystem) if the stash is team-wide; the
  wallet pattern is the closest precedent.
- `Public/UI/RuinmatesTerminalScreen.h/.cpp`: replace the `PreviewStorage` sample page with real
  slots and add the transfer requests.
- `Public/Character/RuinmatesCharacter.h/.cpp`: `Server_*` requests if the stash is reached through
  the character interaction.
- `Public/Inventory/RuinmatesItemDatabaseSubsystem.h`: only if stash-specific rules are needed
  (prefer reusing `GetMaxStack`).

## 9. Constraints to carry into the market task

- Server authority for price, stock, wallet and inventory; the client may only send item ids and
  quantities.
- One item definition source: DT_Items. No per-merchant price copies.
- Do not change `URuinmatesBackpackComponent`'s replication or the existing trade RPC shape; add
  parallel requests instead.
- UI must stay on the V4 shell; no second item-cell or second terminal.
- `ItemPickupSet/` is intentionally untracked; do not commit it.
