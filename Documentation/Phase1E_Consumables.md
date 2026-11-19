# Phase 1E — backpack operation fixes, sell safety and basic consumables

Baseline: `7127899`. Reuses the existing backpack replication, trade RPC shape and stats/weapon
APIs. No GAS, no use animation, no timers, no hunger/buff systems, no storage or cross-map save.

## Task 01 — backpack operation fixes

| Fix | Where |
|---|---|
| Split records the source snapshot | `HandleMenuSplit` stores `ItemId`, quality, original count and revision; `HandleSplitConfirm` re-checks the rendered view before sending. |
| Server validates the full snapshot | `TrySplitStack(Index, Count, ExpectedItemId, ExpectedQuality, ExpectedSourceCount, Revision)` rejects a mismatch and changes nothing. |
| Stale requests | Revision mismatch is rejected on the server; the UI sends the revision it rendered. |
| Sort clears stale selection/menu | `HandleSortBackpack` closes the slot menu and clears `SelectedStableIndex`/cell selection before requesting. |
| Sort button state | Enabled only while a live backpack has items; works on the market page too (`bLive || bMarketLive`). Refreshed by both live refresh paths. |
| Split menu bounds | The menu's Y clamp now reserves the taller split panel, so it stays on screen. |
| Merge test with real separate stacks | `SetSlotsForTest` (test-only) installs two partial Bandage stacks, so the merge is genuine instead of pre-merged by `TryAdd`. |
| Real database category order | `BackpackSortCategories` calls the pure `SortStacks(Table, …)` with DT_Items and asserts Consumable → Ammo → Material plus quality ordering. |

## Task 02 — sell safety

`RequestSellItem(SlotIndex, Count, ExpectedItemId, ExpectedQuality, ExpectedSourceCount, Revision)`
→ `Server_SellItem` validates the revision and the full stack snapshot against the server
backpack before reading the row; the price still comes from DT_Items on the server. A stale
request is rejected with "物品已变化，请重试" and no item moves. Both the V4 market and the
legacy trade window send the snapshot. Buy flow, wallet and economy rules are untouched.

## Task 03 — basic consumables (data driven)

- `FRuinmatesItemRow` gains `UseEffect` (`None`/`Heal`/`Ammo`) and `UseAmount`. The stack carries
  the same two fields so the terminal can enable the action without a lookup.
- DT_Items first-pass values: **Bandage → Heal 25**, **AmmoBox → Ammo 30**. These are placeholders
  owned by design; no other item has an effect.
- Server entry: `TryUseBackpackItem(…)` (called by `Server_UseItem`) validates revision, slot,
  item id, quality and original count, then applies the effect from the **server stack**, not from
  client data:
  - Heal rejects a dead/immutable actor, missing stats and full health, then heals via
    `UStatsComponent::ChangeHealthByAmount`.
  - Ammo rejects a missing weapon, a melee/non-ammo weapon, and a zero refill, then adds reserve
    ammo through `ABaseWeapon::AddCarriedAmmo`.
- One unit is consumed only after the effect applied; every failure path returns before removal.
- UI: the right-click menu has "使用", enabled only when the stack has a use effect. Success
  refreshes through `Backpack::OnChanged`; the HUD ammo readout already polls the weapon.
- `UStatsComponent` gained read-only `GetHealth/GetMaxHealth/IsDead`; `ABaseWeapon` gained
  `UsesAmmo/AddCarriedAmmo`. No stats or weapon architecture was changed.

## Task 04 — tests

| Test | Covers |
|---|---|
| `RuinMates.Inventory.BackpackSort` | two genuinely separate stacks merge, conservation, single revision, stable re-sort, stale revision |
| `RuinMates.Inventory.BackpackSortCategories` | real DT_Items categories and quality ordering inside a category |
| `RuinMates.Inventory.BackpackSplit` | normal split, illegal counts, snapshot mismatch, stale revision, no free slot |
| `RuinMates.Inventory.ItemUse` | heal success consumes exactly one, full health rejected, stale revision, wrong snapshot, illegal slot, ammo without weapon rejected, failures consume nothing |

Result: `RUINMATESEditor Win64 Development` → Succeeded; `Automation RunTests RuinMates` →
**40/40 Success**.

## Unverified (needs PIE)

- Split menu SNAPSHOT flow, menu staying inside the viewport, and the "背包已满" message.
- Sort button visual placement/disabled state, and clearing the selection after a sort.
- Sell rejection message when the stack changed between opening the market and confirming.
- Use action on Bandage at partial health and on AmmoBox with a rifle equipped, including the HUD
  ammo refresh; use feedback text (there is none yet, only the item count/health change).
- Any two-client behaviour.

## Handed to Astra

- Use-item has no client feedback message; failures are silent (item is not consumed).
- Ammo refill has no carried-ammo cap beyond int32; design owns a real cap.
- Item use does not check inventory weight/encumbrance (no such system).
- The legacy trade window now sends the same sell snapshot but has no visual pass.
