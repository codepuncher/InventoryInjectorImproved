# Inventory Interface Information Injector Improved

**Inventory Interface Information Injector Improved** (I5) is a general fix and performance
addon for **I4 (Inventory Interface Information Injector)**. It is an SKSE plugin that changes
no game data and ships no SWFs, so it has no file conflicts.

## Features

- **Icon cache:** I4 recomputes every item's icon data from scratch on _every_
  item-list refresh, so switching tabs, navigating items, or using/dropping an item can
  drop the frame rate massively in large inventories. A refresh takes about 90 ms for
  200 items, and the cost grows with item count. I5 caches that work per item, cutting the
  per-refresh cost to a few milliseconds.
- **Favorites crash fix:** fixes the crash to desktop when opening the Favorites menu with
  I4 1.1.1 on Skyrim versions before 1.7.
- **Alchemy icon fix:** applies I4's keyword icon rules to alchemy table ingredients.
- **Faster SkyUI list refresh:** when the inventory list refreshes, SkyUI checks every category
  tab against every item to decide which tabs are empty. I5 does that check in one step,
  so large inventories refresh faster.
- **Skip redundant SkyUI refreshes:** when SkyUI's item list would refresh with the same items
  as its last refresh, I5 skips the redundant refresh, so I4 doesn't rebuild the icons again.

---

<!-- nexus:start -->

## Requirements

- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (SE/AE) or [VR Address Library for SKSEVR](https://www.nexusmods.com/skyrimspecialedition/mods/58101) (VR)
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604)
- **[I4 - Inventory Interface Information Injector](https://www.nexusmods.com/skyrimspecialedition/mods/85702)**

## Installation

**Mod manager (recommended):**

1. Install the requirements above.
2. Install I5 via your mod manager.
3. Launch Skyrim via SKSE.

**Manual:**

1. Install the requirements above.
2. Copy `InventoryInjectorImproved.dll` to `Data\SKSE\Plugins\`.
3. Launch Skyrim via SKSE.

## Compatibility

- Skyrim SE 1.5.97, AE 1.6.x, 1.7.x, and VR, through Address Library.
- No ESP/ESL; no game records changed.
- **No SkyUI file conflicts**: it does not replace any SWF and works with SkyUI's stock UI as well as UI patches and reskins.
- Wraps I4's Scaleform hooks at runtime; compatible with I4 icon add-ons.

**Known compatible mods:**

- [Aura's Inventory Tweaks](https://www.nexusmods.com/skyrimspecialedition/mods/68557)
- [B.O.O.B.I.E.S (Immersive Icons)](https://www.nexusmods.com/skyrimspecialedition/mods/89241)
- [Compare Equipment NG](https://www.nexusmods.com/skyrimspecialedition/mods/158874)
- [Constructible Object Custom Keyword System (Crafting Categories for SkyUI)](https://www.nexusmods.com/skyrimspecialedition/mods/81409)
- [Container Distribution Framework](https://www.nexusmods.com/skyrimspecialedition/mods/120152)
- [DynamicInventoryIconInjector](https://www.nexusmods.com/skyrimspecialedition/mods/174136)
- [I4 Weapon Icons Overhaul](https://www.nexusmods.com/skyrimspecialedition/mods/106432)
- [Infinity UI](https://www.nexusmods.com/skyrimspecialedition/mods/74483)
- [NORDIC UI](https://www.nexusmods.com/skyrimspecialedition/mods/49881)
- [QuickLoot IE](https://www.nexusmods.com/skyrimspecialedition/mods/120075)
- [The Handy Icon Collection Collective](https://www.nexusmods.com/skyrimspecialedition/mods/90508)
- [TrueHUD - Inventory Injector Patch](https://www.nexusmods.com/skyrimspecialedition/mods/157139)

<!-- nexus:end -->

---

## How it works

I4 has three icon setters, `InventoryIconSetter`, `CraftingIconSetter`, and `MagicIconSetter`,
each recomputing every entry's icon data (form lookups, keyword scans, ActionScript callbacks)
in its `processList` method on every refresh. I5 wraps all three and caches the result per
item, so each refresh hands I4 only the items it hasn't seen before and applies the cached
result to the rest. Covered menus: inventory, container, barter and gift use
`InventoryIconSetter`; crafting (smithing, alchemy, enchanting) uses `CraftingIconSetter`;
magic uses `MagicIconSetter`.

Soul gems are cached per fill level, since a gem's icon depends on how full it is. Items
created at runtime (form IDs starting `0xFF`) are cached only for the current game session
and not saved between sessions, because the engine can reuse those IDs for a different item
later. The rest of the cache is saved in your save file's SKSE co-save, so the first menu open
after loading a save is fast too, and it's rebuilt automatically if your I4 icon configs or
load order change. A diagnostic log is written to
`Documents/My Games/Skyrim Special Edition/SKSE/InventoryInjectorImproved.log`
(`Skyrim Special Edition GOG` on the GOG version).

## Favorites menu

A fix is included in I5 to prevent the crash to desktop that happens when opening the favorites menu with I4 1.1.1 on game versions before 1.7.

I5 doesn't cache Favorites because it's a small list that's rarely heavy, so caching it would save little.

## Console commands

| Command            | Effect                                                                                                                                               |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| `i5 status`        | Prints cache size, how many entries were restored from the SKSE co-save this session, the last and worst refresh timings, and which features are on. |
| `i5 debug on\|off` | Turns per-refresh timing logging to the log file on or off.                                                                                          |
| `i5 cache purge`   | Clears the icon cache. Use it if icons look wrong after updating I4, then save.                                                                      |

`i5 disable`, `i5 enable`, `i5 verify on|off`, `i5 cache enable|disable` and `i5 skyui inventory-dedupe enable|disable` are developer diagnostics: see [CONTRIBUTING.md](CONTRIBUTING.md#diagnostic-console-commands).

## Reporting bugs

Open a [GitHub issue](https://github.com/codepuncher/InventoryInjectorImproved/issues). If Skyrim crashed to
desktop, install [Crash Logger SSE](https://www.nexusmods.com/skyrimspecialedition/mods/59596), reproduce the
crash, and attach the `crash-*.log` it writes to the same SKSE log folder as I5's own log above.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for building, testing and code style, and
[docs/RELEASING.md](docs/RELEASING.md) for the release process.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). Required by CommonLibSSE-NG.
