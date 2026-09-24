# Inventory Interface Information Injector Improved

**Inventory Interface Information Injector Improved** (I5) fixes and improves
**I4 (Inventory Interface Information Injector)**, starting with the menu lag it causes in SkyUI.

I4 recomputes every item's icon data from scratch on *every* item-list refresh, so switching
tabs or using/dropping an item lags, badly in large inventories (~90 ms per refresh for
~200 items, and it scales up from there). I5 is an SKSE plugin that caches I4's
per-item work keyed by form, cutting the per-refresh cost to a few milliseconds. It changes
no game data and ships no SWFs, so it has no file conflicts.

---

<!-- nexus:start -->
## Requirements

- [SKSE64](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)
- [SkyUI](https://www.nexusmods.com/skyrimspecialedition/mods/12604)
- **[I4 - Inventory Interface Information Injector](https://www.nexusmods.com/skyrimspecialedition/mods/85702)**: this mod is a performance companion for I4; without I4 installed it does nothing.

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

- Skyrim SE 1.5.97 and AE 1.6.x, through Address Library. Tested on AE 1.6.1170.
- No ESP/ESL; no game records changed.
- **No SkyUI file conflicts**: it does not replace any SWF and works with SkyUI's stock UI.
- Wraps I4's Scaleform hooks at runtime; compatible with I4 icon add-ons.
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
later. The rest of the cache is saved in your save file's co-save, so the first menu open
after loading a save is fast too, and it's rebuilt automatically if your I4 icon configs or
load order change. A diagnostic log is written to
`Documents/My Games/Skyrim Special Edition/SKSE/InventoryInjectorImproved.log`
(`Skyrim Special Edition GOG` on the GOG version).

## Why not the Favorites menu?

The Favorites menu is a small list that's rarely heavy, so caching it would save little.

## Console commands

| Command | Effect |
|---|---|
| `i5 status` | Prints cache size, how many entries were restored from the co-save this session, and the last and worst refresh timings. |
| `i5 purge` | Clears the icon cache. Use it if icons look wrong after updating I4, then save. |
| `i5 debug on\|off` | Turns per-refresh timing logging to the log file on or off. |

`i5 bypass`, `i5 verify` and `i5 memo` are developer diagnostics: see [CONTRIBUTING.md](CONTRIBUTING.md#diagnostic-console-commands).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for building, testing and code style, and
[docs/RELEASING.md](docs/RELEASING.md) for the release process.

## License

MIT, see [LICENSE](LICENSE).
