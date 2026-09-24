# Nexus Mods Page Content

## Short Description

Improves I4's inventory interface interactivity: I5 indexes item icons initially, ignoring identical items in iterations, including in item-heavy inventories. Importantly, it's installed independently, invisibly integrating into I4's implementation.

---

## Tags

- SKSE
- User Interface
- Bug Fixes

---

## Long Description (BBCode)

<!-- The Requirements, Installation, and Compatibility sections below are generated.
     Run `python3 scripts/generate-nexus-page.py` and copy the output to Nexus Mods.
     Do not copy this file directly: the generated block markers are not valid BBCode. -->

```bbcode
[font=Times New Roman][size=5]OVERVIEW[/size][/font]
I5 aims to fix and improve I4, starting with its performance issues, such as the lag when switching inventory tabs, navigating crafting menu categories, using or dropping items, or taking items from containers.

I4 rebuilds every item's icon data each time a menu's item list refreshes, which takes about 90ms for about 200 items. I5 caches that work per item, so each refresh only processes new items. The cache is saved with your game, so the first menu open after loading a save is fast too.

I5 also fixes the crash to desktop when opening the Favorites menu with I4 1.1.1 on Skyrim versions before 1.7.

[font=Times New Roman][size=4]MENUS COVERED[/size][/font]
[list]
[*]Player inventory
[*]Containers, including follower inventories
[*]Barter
[*]Gifting
[*]Crafting: smithing, smelting, tanning, cooking, alchemy and enchanting
[*]Magic
[/list]

<!-- generated:start -->
[font=Times New Roman][size=5]REQUIREMENTS[/size][/font]
[list]
[*][url=https://skse.silverlock.org/]SKSE64[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/12604]SkyUI[/url]
[*][b][url=https://www.nexusmods.com/skyrimspecialedition/mods/85702]I4 - Inventory Interface Information Injector[/url][/b]: this mod is a performance companion for I4; without I4 installed it does nothing.
[/list]

[font=Times New Roman][size=5]INSTALLATION[/size][/font]
[font=Times New Roman][size=4]MOD MANAGER (RECOMMENDED)[/size][/font]
[list=1]
[*]Install the requirements above.
[*]Install I5 via your mod manager.
[*]Launch Skyrim via SKSE.
[/list]

[font=Times New Roman][size=4]MANUAL[/size][/font]
[list=1]
[*]Install the requirements above.
[*]Copy [font=Courier New]InventoryInjectorImproved.dll[/font] to [font=Courier New]Data\SKSE\Plugins\[/font].
[*]Launch Skyrim via SKSE.
[/list]

[font=Times New Roman][size=5]COMPATIBILITY[/size][/font]
[list]
[*]Skyrim SE 1.5.97 and AE 1.6.x, through Address Library. Tested on AE 1.6.1170.
[*]No ESP/ESL; no game records changed.
[*][b]No SkyUI file conflicts[/b]: it does not replace any SWF and works with SkyUI's stock UI.
[*]Wraps I4's Scaleform hooks at runtime; compatible with I4 icon add-ons.
[/list]
<!-- generated:end -->

[font=Times New Roman][size=5]CONSOLE COMMANDS[/size][/font]
[list]
[*][font=Courier New]i5 status[/font]: prints cache size, how many entries were restored from the co-save this session, and the last and worst refresh timings.
[*][font=Courier New]i5 purge[/font]: clears the icon cache. Use it if icons look wrong after updating I4, then save.
[*][font=Courier New]i5 debug on|off[/font]: turns per-refresh timing logging to the log file on or off.
[/list]

[font=Times New Roman][size=5]FAQ[/size][/font]
[b]Does I5 fix the favorites menu CTD?[/b]
[spoiler]Yes, a patch is included in I5 for this issue and will be removed once it's fixed upstream in I4.[/spoiler]

[b]Why doesn't I5 speed up the Favorites menu?[/b]
[spoiler]The Favorites menu is a small list that's rarely heavy, so caching it would save little.[/spoiler]

[b]Is it safe to add or remove mid-playthrough?[/b]
[spoiler]Yes. I5 has no ESP and changes no game records. Its co-save only holds the icon cache: on a save without one, the cache fills as you open menus, and if you remove I5, SKSE skips its co-save data.[/spoiler]

[b]Icons look wrong after I updated I4. What do I do?[/b]
[spoiler]Run [font=Courier New]i5 purge[/font], then save. The cache resets itself on load when your plugins or I4 configs change, but it can't tell when I4 itself is updated. Saves made before the update keep their old cache until you purge and save over them.[/spoiler]

[b]How do I know it's working?[/b]
[spoiler]Run [font=Courier New]i5 status[/font]. It shows the cache size, how many entries came from the co-save, and the last and worst refresh times.[/spoiler]

[b]Why is the first menu open on a new save slower?[/b]
[spoiler]There is no cache yet, so I4 does its full work once. After that, the cache is kept in your saves.[/spoiler]

[b]Does it change which icons I4 shows?[/b]
[spoiler]No. I5 reuses I4's own results, so the icons are the same.[/spoiler]

[b]Does it work on VR?[/b]
[spoiler]No. I5 supports Skyrim SE and AE only. I don't own a VR headset, so I can't test or support a VR version. If you play in VR and want to help test, post in the comments and I can make a test build.[/spoiler]

[font=Times New Roman][size=5]CREDITS[/size][/font]
[list]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/85702]I4 - Inventory Interface Information Injector[/url] by [url=https://www.nexusmods.com/skyrimspecialedition/users/39501725]Parapets[/url] and [url=https://www.nexusmods.com/skyrimspecialedition/users/4569617]Jelidity[/url]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/12604]SkyUI[/url] by the SkyUI Team and [url=https://github.com/doodlum/SkyUI-Community]community contributors[/url]
[*][url=https://skse.silverlock.org/]SKSE[/url] by the SKSE Team
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url] by [url=https://www.nexusmods.com/profile/meh321]meh321[/url]
[*][url=https://github.com/alandtse/CommonLibVR/tree/ng]CommonLibSSE-NG[/url] by [url=https://github.com/alandtse]alandtse[/url] and contributors
[/list]
```
