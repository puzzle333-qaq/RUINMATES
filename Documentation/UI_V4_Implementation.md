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
- Unmatched live loot icons show a neutral glyph plus their real name. Canned
  food, circuit boards, alien glands/cores and model-specific weapon art need
  matching assets. Bandage and AmmoBox reuse corresponding V4 icons.
- Current weapon data does not provide matching weapon portraits. Live HUD uses
  a neutral weapon glyph; realistic sample weapons appear only in labelled demos.
- No actual voice service was verified. Speaking states are demonstrated only
  in labelled previews. There are at most three other teammates, not four.
- The fourth HUD weapon position is a reserved empty position when the existing
  inventory has no corresponding weapon. No new weapon category was created.
- The new market and storage views await actual backend integration. Their
  confirmation/transfer controls explicitly remain disabled.
- Native Slate places/clamps tooltips; a bespoke 180ms per-cell tooltip scheduler,
  closing fade, drag/drop and Alt-based mouse weapon switching are not implemented.
- Host/client manual multiplayer verification and controller-only navigation
  remain separate validation work; screenshots do not establish networking.

## Verification

Editor build, full automation, screenshot dimensions and interaction results are
recorded below after final verification.
