# Phase 1F-A — game-test fixes and friendly fire

Baseline: `7dfe107`. Scope limited to the requested fixes; no storage, save, AI, progression or
base systems were added.

## Task 01 — weapon operation check (inspection)

Code path checked, no weapon/animation refactor:

- **Aim**: `AimAction` is bound to `Started/Completed/Canceled` on the runtime mapping context and
  drives `StartAim/StopAim`. `OpenInventoryScreen` calls `StopAim`, so opening the backpack cancels
  aim by design; after closing, the first aim press re-enters it.
- **Fire / reload / switch**: `FireAction` → `TryFireWeapon` → server-authoritative `TryFire`;
  reload and weapon switching use the existing inventory APIs. No change made.
- **Test map default weapon — test condition gap, fixed on the map side**: the map had no weapon
  source at all. Two `BP_Glock18` actors are now placed in the shooting range. Whether the player
  character spawns already armed belongs to `BP_ThirdPersonCharacter` defaults and is *not* a code
  bug in the weapon system; it stays a blueprint/test-condition item.
- **Input**: Enhanced Input actions are created and bound in C++ on the runtime mapping context;
  no asset-level IMC is required for them.
- **After closing the backpack**: `CloseInventoryScreen` removes the widget, hides the cursor and
  restores `FInputModeGameOnly`; no input action is consumed by the terminal. Verified by code
  inspection only (PIE unverified).

## Task 02 — friendly fire

- `ARuinmatesGameMode::FriendlyFireMultiplier` (default **0.25**, `Config`/blueprint overridable,
  clamped to `[0,1]`).
- `AreTeammates(A, B)`: true only for two different player characters (co-op has one side), so
  enemies and self-damage are unaffected.
- `ScaleDamageForTarget(BaseDamage, Instigator, Victim)`: teammates take `Base * multiplier`,
  everyone else takes the full amount.
- Applied at the single server damage site (`ABaseWeapon::ApplyServerHit`) before
  `UStatsComponent::ChangeHealthByAmount`, so the existing weapon damage and stats systems are
  reused; no second damage system. Health HUD refresh already flows through `OnHealthChanged`.
- Test: `RuinMates.Combat.FriendlyFire` (0.25 default, teammate/enemy/null cases, scaling, zero).

## Task 03 — removed backpack mouse drag

- Deleted `URuinmatesBackpackDragOperation`, the drag/drop overrides, drag-enable and dragging
  state from `URuinmatesTerminalItemSlot`, the drop-target/allowed fields from the slot view, and
  the screen call that enabled drag.
- Removed the drag highlight branch; hover and selection highlight remain.
- Kept: click selection, hover tooltip, right-click menu (use / split / discard / sort), automatic
  stacking, one-click sort, split, discard, use.
- **Kept server-side move API for future reuse** (`TryMoveSlot`, `CanMoveSlotSnapshot`,
  `RequestMoveBackpackSlot`): the task explicitly asks to retain reusable item operations and
  snapshot validation for the future shared stash.
- **Test note**: no test was deleted. `RuinMates.Inventory.BackpackMoveSnapshots` still tests the
  retained component API (not the removed UI drag path); `BackpackSort`/`BackpackSplit` cover the
  remaining operations.

## Task 04 — empty category

- `CategoryEmptyHint` is centered and only shown when a real filter has no matches.
- Filtered mode still omits empty cells (compact); overview still lists every real slot; the
  backpack panel size is unchanged.

## Task 05 — interaction prompt

- New `ARuinmatesCharacter::FindInteractTarget()` holds the existing trace-then-overlap target
  search; `TryInteract` now uses it, so the prompt and the F key resolve the **same** target.
- `URuinmatesFieldHUD` shows one centered line above the weapon bar with the target's
  `GetInteractPrompt` text. It is hidden when no target is in range, when the prompt is empty and
  while the terminal is open (`IsTerminalOpen`).
- Cost control: a 0.15 s timer does the lookup; no per-frame trace.

## Task 06 — consumable feedback

- New `ERuinmatesItemUseResult` (`Success, FullHealth, NoWeapon, NotUsable, ItemChanged`) is sent
  from the server via `Client_ItemUseResult`; the terminal shows it on the backpack page.
- Values unchanged: Bandage heals 25, AmmoBox adds 30 reserve ammo. Consumption still happens only
  after the effect applies; every failure returns before removal.
- The previous failures were caused by having no damage source in the test setup, which is now
  possible with friendly fire.

## Task 07 — legacy UI fixes

- Sort button: the market refresh sets its enabled state as well, so a disabled button recovers
  once the backpack has items (previously only the backpack page refreshed it).
- Split menu: the menu is re-clamped after the split panel expands, so it no longer overflows the
  viewport bottom.
- Selection: sorting clears the remembered slot selection and closes the context menu.

## Verification

- `RUINMATESEditor Win64 Development` → Succeeded (`Docs/CompileEditorTarget.ps1`).
- `Automation RunTests RuinMates` → **41/41 Success**.

## Unverified (needs PIE)

- Aim/fire/reload/switch in the map, and whether the character spawns armed (blueprint default).
- Friendly fire with two real clients (host and client both dealing/receiving 25 % damage).
- Interaction prompt visibility/position for containers and the merchant, and hiding on terminal
  open.
- Use result lines for each failure reason, and healing with a real damage source.
- Drag removal: no leftover drag cursor behaviour, right-click menu still reachable.
- Category empty hint centering at 1080p/1440p.

## Handed to Astra

- Friendly fire has no penalty/score system; the multiplier is a single global value.
- Default weapon provisioning is blueprint-side; the test map only supplies world pickups.
- No prompt for the merchant or container when the player is behind a wall (same trace as F).
