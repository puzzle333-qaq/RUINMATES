# Phase 1C — V4 market integration

Baseline: `8c342d9`. Scope: bind the existing V4 terminal market page to the real trade
backend, and fix the functional test map so it runs the project's configured character.
No new trade backend, wallet, item database, extraction or storage was added.

## What changed

### Task 01 — functional test map

`RuinmatesBuildTestMap` now resolves the project's configured game mode instead of
hardcoding the C++ one:

- `/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode` (parent `ARuinmatesGameMode`,
  pawn `BP_ThirdPersonCharacter`, mesh present) becomes the map's `DefaultGameMode`.
- The commandlet logs and falls back to `ARuinmatesGameMode` if that blueprint ever stops
  being a compatible subclass with a meshed pawn.
- The test merchant is stocked through `ARuinmatesMerchant::SetMerchantItems` with
  `Bandage`, `Cans`, `AmmoBox`, `Circuit` — the four DT_Items rows that already have
  `bCanBuy`. No price, stack size or balance value was touched.
- Shipping maps were not modified.

### Task 02 — V4 market

`URuinmatesTerminalScreen` gained a live market mode on the existing V4 page
(`RuinmatesTerminalV4Layout.cpp`):

| Requirement | Implementation |
|---|---|
| Real goods from the current merchant | `ARuinmatesMerchant::GetMerchantItems()` on every refresh |
| Icon / name / quality / description / price from DT_Items | `FRuinmatesItemRow` + `GetItemBuyPrice` / `GetItemSellPrice` |
| Real backpack on the left | `Backpack->GetSlots()` rendered through the shared `MakeSlotView` |
| Merchant goods on the right | existing `MarketGrid` cells, goods list in buy mode, backpack stacks in sell mode |
| Buy / sell switch | existing 购买 / 出售 buttons; `PreviewBuy/PreviewSell` route to the live handlers when the market is live |
| Item and backpack selection | cell `OnSlotClicked` → `HandleMarketSelected`; selection is validated on every refresh and cleared when it no longer exists |
| Quantity +/- | existing `+` / `-` buttons, clamped to the selected stack count (sell) or 99 (buy) |
| Unit price, quantity, total | `MarketPrice`, `MarketQuantity`, `MarketTotal` updated every refresh |
| Team funds | `Page.Funds` from `ARuinmatesGameState::GetSharedFunds`, refreshed on `OnSharedFundsChanged` |
| Confirm buy / sell | `MarketConfirmButton` → `RequestBuyItem` / `RequestSellItem` (client sends id/slot + quantity only) |
| Server result | `Client_TradeResult` → `ShowTradeResult`, shown in `MarketStatusText` in success/error colour |
| Refresh after trade | backpack `OnChanged` and wallet `OnSharedFundsChanged` both call `Refresh` → `RefreshMarketLive` |
| ESC / close | `ClosePreview` ends the trade through `EndTrade`, which closes the terminal and restores game input |

The isolated preview (`Ruinmates.PreviewMarket`) still works and still has no transactions;
it now shares the same cell/selection code.

### Task 03 — backend reuse

No duplicate backend. The market calls exactly:

- `ARuinmatesCharacter::RequestBuyItem` / `RequestSellItem`
- `ARuinmatesCharacter::Client_TradeResult`
- `ARuinmatesMerchant` (`GetMerchantItems`, `SellsItem`, `GetInteractionDistance`)
- `ARuinmatesGameState::GetSharedFunds` / `OnSharedFundsChanged`
- `URuinmatesItemDatabaseSubsystem` (`FindItemDefinition`, `GetItemBuyPrice`, `GetItemSellPrice`)
- `URuinmatesBackpackComponent` (`GetSlots`, `GetCapacity`, `GetUsedSlotCount`, `OnChanged`)

Local changes limited to the UI entry/close/result forwarding in `ARuinmatesCharacter`:
`OpenInventoryScreen` takes an optional merchant and opens the market, `BeginTrade` and
`OnRep_ActiveMerchant` open the V4 market, `EndTrade` / `Server_EndTrade` close it through
`CloseMarketScreens`, and `Client_TradeResult` forwards to the terminal first.
`URuinmatesTradeScreen` remains in the project as a fallback and is no longer opened by the
normal trade path.

## Acceptance (Task 04)

**All interactive rows are UNVERIFIED.** The agent cannot drive PIE input or two-client
sessions. Rows marked "logic only" are covered by code paths and, where applicable, by the
automation suite; they are still not a substitute for a PIE pass.

| # | Check | Expected | Status |
|---|---|---|---|
| 1 | Normal purchase | Funds drop by unit price × quantity, goods appear in the backpack | UNVERIFIED |
| 2 | Normal sale | Funds rise by sell price × quantity, stack shrinks or disappears | UNVERIFIED |
| 3 | Quantity selection | `+` / `-` clamp between 1 and the stack count / 99 | UNVERIFIED |
| 4 | Insufficient funds | Server rejects, error text shown, wallet unchanged | UNVERIFIED (server path covered by `RuinMates.Economy.BackpackExactTrade`) |
| 5 | Full backpack | Server rejects the buy, error text shown, no charge | UNVERIFIED |
| 6 | Unsellable item | Sell list only contains backpack stacks; merchant `bCanSell` is enforced server side | UNVERIFIED |
| 7 | Trade result display | Success/error text in the status line, colours differ | UNVERIFIED |
| 8 | Live refresh after trade | Backpack cells, funds and selection update without reopening | UNVERIFIED |
| 9 | Input restored after close | Cursor hidden, game-only input, movement/look/fire work | UNVERIFIED |
| 10 | Backpack/search unaffected | TAB backpack and container search still open and behave as before | UNVERIFIED |
| 11 | Development Editor build | `RUINMATESEditor Win64 Development` builds | PASS (`Docs/CompileEditorTarget.ps1`, 0 errors) |
| 12 | Automation suite | All RuinMates tests pass | PASS (34/34, Exit Code 0) |

Build and test evidence for this round: `docs/CompileEditorTarget.ps1` → `Result: Succeeded`;
`Automation RunTests RuinMates` → 34/34 Success.

## Known limitations / handed to Astra

- The merchant has no stock counters; the list is a fixed id set from the actor. Buying does
  not deplete it.
- The status line is one text block; there is no per-row error highlight.
- The market refresh is event driven only; no polling, so an externally mutated backpack
  (e.g. another UI path) must emit `OnChanged` to appear.
- The V4 market page position for the confirm button and status line was authored without a
  visual pass in this round; verify spacing at 1080p and 1440p.
- `URuinmatesTradeScreen` is retained but unreachable from normal interaction; deleting it is
  a separate decision.
