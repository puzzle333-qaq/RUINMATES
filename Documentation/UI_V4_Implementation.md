# V4 UI Implementation

## Scope

This change applies the supplied V4 art to the native UMG terminal and field HUD.
The terminal follows the handoff's fixed five-column backpack on the left and
equipment/container/storage/market on the right. It intentionally follows those
coordinates where the concept screenshots show a different composition.

The ordinary backpack and search screen retain their existing data bindings and
server request interfaces. Storage and the new market composition are isolated,
labelled visual previews. They do not transfer items, charge funds, or replace the
existing working merchant screen. No new storage, voice, drag/drop, economy,
inventory, replication, save, or weapon rules were introduced.

## Assets And Provenance

Source: the user-supplied `RUINMATES_UI_AssetPack_V4.zip` in Downloads.
The handoff document and two user reference images supplied the design target.
No new image generation or external art download was used.

- Original PNG count: 159.
- Derived PNG count: 74 (9 rectangular cells, 25 alpha-cropped item icons,
  40 half-resolution Box textures).
- Imported textures: 233, under `/Game/RUINMATES/UI/AssetPackV4/`.
- Real editor-created UI materials: `Materials/M_UI_CharacterPreview` and
  `Materials/M_UI_StaminaRing`.
- Imported textures use UI group, UserInterface2D compression, sRGB, no mips,
  clamp and NeverStream. `Saved/UI_V4_Import.json` records verification.
- Binary assets remain covered by the existing Git LFS attributes.

The original pack remains outside Cook in ignored `Saved/UI_Source/`.
`Scripts/UI/prepare_v4.py` deterministically generates the derivatives and native
brush table. `Scripts/UI/import_v4.py` imports and verifies actual Unreal assets
without replacing existing assets or modifying item data tables.

Slate Box margins use actual texture pixels, regardless of Brush ImageSize.
Half-resolution derivatives preserve normalized source margins while rendering
corners at the intended logical size. The 168x112 slot derivatives preserve
source corner pixels instead of stretching square cut corners.

The five existing gameplay quality values are retained: Common uses white;
Fine and Advanced share green; Rare uses purple; Precious uses gold. Cyan
selection is an independent overlay. This changes no gameplay quality identity.

## Implementation Map

| Component | Source |
|---|---|
| Theme and manifest loading | `RuinmatesUIStyle.*`, `RuinmatesV4Brushes.inl` |
| Interruptible native button glow/press/focus | `RuinmatesTechButton.*` |
| Native slider and animated toggle | `RuinmatesTechControls.*` |
| Fixed grid with external scrollbar | `RuinmatesGridScrollBox.*` |
| Shared terminal and layout | `RuinmatesTerminalScreen.*`, `RuinmatesTerminalV4Layout.cpp` |
| Quality, selection, scan and detail tooltip | `RuinmatesTerminalItemSlot.*` |
| Isolated sample item views | `RuinmatesTerminalTypes.cpp` |
| Cosmetic mesh-only SceneCapture | `RuinmatesCharacterPreview.*` |
| Squad presentation | `RuinmatesSquadRadio.*` |
| Health, stamina, compass and weapons | `RuinmatesFieldHUD.*` |
| Local lifecycle and input mode | `RuinmatesCharacter.*` |
| Editor-only PIE capture support | `RUINMATESEditor/RuinmatesUIPlayPreview.cpp` |

Files above are in `Source/RUINMATES/Public/UI/` and
`Source/RUINMATES/Private/UI/` unless otherwise noted.

The terminal scales one centered 1920x1080 logical canvas after project DPI.
The world dimmer fills the viewport; ultrawide extra space preserves the world.
Left cells remain 168x112 with 12-unit gaps. Page navigation changes only right
content, preserving left selection/filter/scroll. Unknown search cells contain no
real icon, name, count, description or price in their view data.

Character capture copies the real skeletal pose into a non-replicated cosmetic
actor with no inventory, damage or interaction components. It captures at 10Hz
only while visible, and is released on close/context change. No constructor
hardcoded asset finder was introduced.

Health/stamina use existing stat delegates. Weapon snapshots use a 0.1s timer
because the current weapon interface has no ammunition-change delegate.
Radio roster snapshots use a 1s timer. Owner-only teammate health is hidden on
remote clients rather than displaying an incorrect default value.

## Preview And Reproduction

Start PIE in the existing test map, then use the Unreal console:

```text
Ruinmates.PreviewInventory
Ruinmates.PreviewInventory30
Ruinmates.PreviewSearch
Ruinmates.PreviewStorage
Ruinmates.PreviewMarket
Ruinmates.UIShowcase
Ruinmates.PreviewHUD
```

The HUD command toggles labelled sample presentation on the existing HUD.
Normal gameplay uses real local state. TAB opens the ordinary backpack;
TAB/ESC or the close button closes the terminal. Native UIOnly input prevents
gameplay actions while it owns focus. The original console fallback
`Ruinmates.UI.LegacyTerminal 1` retains the earlier layout path, but shares the
updated theme and item cells; it is not a complete visual rollback.

Actual engine screenshots are generated by `Scripts/UI/capture_v4.py` in
`Saved/UI_V4_Captures/`. The script uses a floating PIE viewport and the native
Slate screenshot API, including UMG. These are not HTML renders.
`RUINMATES_V4_QUICK=1` limits capture to 1080p.
`RUINMATES_CAPTURE_KEEP_OPEN=1` leaves the preview open for input checks.
Capture play settings are restored without saving altered editor defaults.

## Remaining Differences

- The test map and mannequin remain the project's actual content. The reference
  wasteland scene, survivor model and teammate portrait art are not in this pack.
- Live loot icons come from DT_Items: every seeded item row binds its own
  `/Game/RUINMATES/UI/Textures/Items/T_Item_<RowName>` artwork. A row with no
  icon falls back to the neutral placeholder cell glyph, so new items without
  art never borrow another item's icon.
- Current weapon data provides no matching weapon portrait: the pack only has
  M4A1, P226 and TacticalKnife art while the project ships a Glock18 data asset.
  `UWeaponData::HUDIcon` is the single opt-in field for a real portrait; an unset
  icon keeps the neutral weapon glyph in the live HUD.
- No actual voice service was verified. Speaking states are demonstrated only
  in labelled previews. There are at most three other teammates, not four.
- The fourth HUD weapon position is a reserved empty position when the existing
  inventory has no corresponding weapon. No new weapon category was created.
- The market view is now bound to the real trade backend (`Documentation/Phase1C_Market.md`);
  the isolated `Ruinmates.PreviewMarket` console command still shows sample goods with no
  transactions. The storage view remains a visual preview with transfer controls disabled.
- Native Slate places/clamps tooltips; a bespoke 180ms per-cell tooltip scheduler,
  closing fade, drag/drop and Alt-based mouse weapon switching are not implemented.
- Host/client manual multiplayer verification and controller-only navigation
  remain separate validation work; screenshots do not establish networking.

## Verification

Editor build, full automation, screenshot dimensions and interaction results are
recorded below after final verification.

## Maintenance log — 2026-11-19

Low-risk maintenance pass. No layout, art or replication change.

Code:

- `URuinmatesBackpackComponent::GetAvailableCapacityForItem` resolved a legacy
  item id but still compared the raw id against stored slots, so the free room
  in matching stacks was under-counted and an all-or-nothing purchase could be
  rejected. The comparison now uses the resolved id. The pure math lives in
  `ComputeAvailableCapacity` for testing.
- `URuinmatesItemDatabaseSubsystem::ResolveItemIdInTable` exposes the table-level
  legacy resolution (numeric id -> `DisplayName`) to tooling and tests.
- `URuinmatesTerminalScreen::RebuildSlots` no longer runs an empty loop over the
  backpack cells.
- `URuinmatesTradeScreen` drops its private `WithAlpha` in favour of the shared
  `RuinmatesUI::WithAlpha`. A file-scope `using RuinmatesUI::WithAlpha;` in
  `RuinmatesTerminalScreen.cpp` leaks into the same unity blob, which made the
  duplicate ambiguous and broke the build whenever the blob layout changed.

Data / presentation:

- DT_Items already bound a matching icon for all six seeded items; the screen
  then discarded four of them and replaced two with generic V4 glyphs. The
  hardcoded override is gone and the table is authoritative. Items without an
  icon keep the neutral placeholder.
- The item tooltip now shows the unit price next to the stack value.

Tests (all Editor Context / Engine Filter):

- `RuinMates.Inventory.BackpackLegacyCapacity` — legacy id resolves by display
  name; capacity counts the room left in the resolved stack.
- `RuinMates.Inventory.BackpackTryAddLegacyId` — a legacy stack normalizes
  through the table and two canonical adds merge into one slot.
- `RuinMates.UI.V4.CategoryFilterRealItems` — filtering the real DT_Items rows
  compacts the grid, keeps each view's true slot index, never emits empty cells,
  the overview still returns every slot, and the "no matches" case uses a
  synthetic key so no real category (e.g. Weapon) is assumed empty. Slot count
  scales with the table instead of a fixed 12.
- `RuinMates.UI.V4.ItemIconReferences` — every bound icon is a loadable project
  UI texture; custom asset naming is accepted and unbound rows remain valid.
- `RuinMates.UI.V4.WeaponHudIcon` — the optional weapon portrait loads as a UI
  texture and an unset icon stays null-safe.

Results:

- `RUINMATESEditor Win64 Development` build: SUCCESS.
- `Automation RunTests RuinMates`: 34/34 Success (see delivery note for the
  exact run).

Known issues recorded, not changed:

- `URuinmatesGridScrollBox` relies on `Super` to release `MyScrollBox`; the
  external scrollbar is owned by its own UMG widget and the UPROPERTY keeps the
  UObject alive. Reviewed 2026-11-19: no leak or dangling Slate ref found, no
  change made.
- Category filter drops empty cells by design, so a filtered page can end with a
  partially filled last row; this matches the compact layout requirement.

