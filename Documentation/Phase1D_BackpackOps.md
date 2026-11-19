# Phase 1D — market fixes, backpack sort and stack split

Baseline: `b345371`. Additions reuse the existing V4 market page, the existing backpack
replication and the existing trade RPC shape. No new backend, wallet, item database,
storage or save path.

## Task 01 — market fixes

| Fix | Where |
|---|---|
| Host result was overwritten by the pending label | `HandleMarketConfirm` now marks the request pending and paints "正在处理…" **before** sending. A listen-server host receives `Client_TradeResult` inside the request call, so the later write no longer clobbers the result. |
| Slot click in live sell mode called `RefreshVisualPage` | `HandleSlotClicked` routes to `HandleMarketSelected` while `bMarketLive`, and only falls back to the preview path outside the live market. |
| Live market must use the real refresh path | Every market mutation (mode, selection, quantity, server result, `OnChanged`, `OnSharedFundsChanged`) passes through `RefreshMarketLive`. |
| Sell selection identity | `FRuinmatesMarketTradeState` remembers item id, quality and slot; `RefreshMarketLive` compares them with the real backpack and clears the selection when the stack changed or vanished. |
| Duplicate submits | `FRuinmatesMarketTradeState::BeginRequest` refuses a second request while one is in flight; the confirm button is disabled while pending; `CompleteRequest` runs on the server answer, `Reset` on reopening the market. |
| Reopen resets state | `OpenMarket` calls `TradeState.Reset()` and clears the status line. |
| Total price overflow | Unit price × quantity is computed as `int64` before formatting. |
| Legacy window closing stole input | `CloseTradeScreen` no longer forces `FInputModeGameOnly` when the terminal/market widget is still in the viewport. |

Server-side stale sell validation is intentionally out of scope (recorded for a later task).

## Task 02 — one-click sort (server authoritative)

`URuinmatesBackpackComponent::TrySort(uint32 ExpectedRevision)`:

1. Merges stacks with the same ItemId, quality and MaxStack.
2. Re-splits each merged total into legal stacks (never above MaxStack).
3. Orders by item category (DT_Items when the database is loaded, otherwise quality/material fallback).
4. Quality from high to low inside a category.
5. ItemId lexical order for equal quality.
6. Writes one final slot array: gaps at the end, capacity untouched, one `MarkChanged` (revision + `OnChanged`).
7. Conservation guards reject the pass if the unit total changed or the slot array would overflow.

Client entry: `ARuinmatesCharacter::RequestSortBackpack` → `Server_SortBackpack` → the component.
A stale revision is rejected on the server; the UI sends the revision it rendered.

## Task 03 — split stack (server authoritative)

- Item context menu gains `拆分`; it opens an inline panel with the existing V4 slider
  (`URuinmatesTechSlider` range support added) and a live "拆分 N" label.
- Range is `1 .. Count-1`; a single-unit stack shows "数量为 1 的物品无法拆分"; cancel changes nothing.
- Confirm checks for a free slot locally and shows "背包已满，无法拆分" instead of sending.
- Server: `TrySplitStack(SourceIndex, Count, ExpectedRevision)` validates revision, slot, count
  range and a free slot, copies every stack field into the first empty slot, keeps the total,
  then emits one `MarkChanged`.
- Failure (revision, count, no slot) never touches the backpack.

## Tests (Task 04)

Added to `RuinMates` automation:

| Test | Covers |
|---|---|
| `RuinMates.Inventory.BackpackSort` | conservation, merge, quality separation, stable re-sort, stale revision |
| `RuinMates.Inventory.BackpackSortEdges` | empty backpack, full backpack, revision bump |
| `RuinMates.Inventory.BackpackSplit` | normal split, illegal counts (0 / equal / larger), stale revision, no free slot |
| `RuinMates.UI.V4.MarketTradeState` | pending guard, complete keeps selection, reset clears pending + identity |

Result: `RUINMATESEditor Win64 Development` builds; `Automation RunTests RuinMates` → **38/38 Success**.

## Unverified (needs PIE)

Every interactive row below is UNVERIFIED; the agent cannot drive PIE input. Component logic for
sort/split is covered by the tests above, but the following are not:

| Check | Status |
|---|---|
| Market confirm shows the host result instead of "正在处理…" | UNVERIFIED |
| Sell selection is cleared when the stack changes | UNVERIFIED |
| Double-click on confirm sends a single request | UNVERIFIED |
| Sort button visually placed in the backpack header | UNVERIFIED (positioned in the gap before the value number) |
| Sort via menu + button, revision rejected after a concurrent change | UNVERIFIED (server logic tested) |
| Split slider range/labels, cancel, full-backpack message | UNVERIFIED |
| Split result appears in the first free slot and selection clears | UNVERIFIED |
| Closing the market returns game input and does not disturb another open UI | UNVERIFIED |

## Handed to Astra

- Sorting uses the database category only when the item database subsystem is available; in a bare
  test world it falls back to quality/material, so the two orderings can differ.
- Split/sort have no per-slot animation; the UI refreshes through `OnChanged` only.
- Server `RequestSellItem` still trusts the slot index for stack freshness; the pending/identity
  guard is client-side only (server-side stale request validation is a follow-up task).
