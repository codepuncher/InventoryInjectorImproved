# Nexus Mods Page Content

## Short Description

Fixes and improves I4: faster item menus (inventories, containers, barter, crafting, magic), a Favorites menu crash fix, and alchemy table keyword icons.

---

## Tags

- SKSE
- User Interface
- Bug Fixes
- Performance Optimization
- SkyUI

---

## Long Description (BBCode)

<!-- The Features, Requirements, Installation, Compatibility and Console commands sections are generated
     from README.md. Run `python3 scripts/generate-nexus-page.py` and copy the output to Nexus Mods.
     Do not copy this file directly: the generated block markers are not valid BBCode. -->

```bbcode
[font=Times New Roman][size=5]OVERVIEW[/size][/font]
[quote]Improves I4's inventory interface interactivity: I5 indexes item icons, ignoring identical items in iterations, including in immense inventories. It includes ingredient icon improvements. Importantly, it's installed independently, invisibly integrating into I4's implementation.[/quote]

I5 is a general fix and performance addon for I4.

<!-- generated:start -->
<!-- generated:end -->

[font=Times New Roman][size=5]FAQ[/size][/font]
[b]Does I5 fix the favorites menu CTD?[/b]
[spoiler]Yes, a patch is included in I5 for this issue and will be removed once it's fixed upstream in I4.[/spoiler]

[b]Why doesn't I5 speed up the Favorites menu?[/b]
[spoiler]The Favorites menu is a small list that's rarely heavy, so caching it would save little.
If you need it then just ask and I'll add support.[/spoiler]

[b]Is it safe to add or remove mid-playthrough?[/b]
[spoiler]Yes. I5 has no ESP and changes no game records. Its SKSE co-save only holds the icon cache: on a save without one, the cache fills as you open menus, and if you remove I5, SKSE skips its co-save data.[/spoiler]

[b]Icons look wrong after I updated I4. What do I do?[/b]
[spoiler]Run [font=Courier New]i5 cache purge[/font], then save. The cache resets itself on load when your plugins or I4 configs change, but it can't tell when I4 itself is updated. Saves made before the update keep their old cache until you purge and save over them.[/spoiler]

[b]My game crashed. What do I attach to a bug report?[/b]
[spoiler]Install [url=https://www.nexusmods.com/skyrimspecialedition/mods/59596]Crash Logger SSE[/url], reproduce the crash, then post a bug report in the Bugs tab with the newest crash-*.log from your SKSE log folder attached.[/spoiler]

[b]How do I know it's working?[/b]
[spoiler]Run [font=Courier New]i5 status[/font]. It shows the cache size, how many entries came from the SKSE co-save, the last and worst refresh times, and which features are on.[/spoiler]

[b]Why is the first menu open on a new save slower?[/b]
[spoiler]There is no cache yet, so I4 does its full work once. After that, the cache is kept in your saves.[/spoiler]

[b]Does it change which icons I4 shows?[/b]
[spoiler]No. I5 reuses I4's own results, so the icons are the same.[/spoiler]

[b]Does it work on VR?[/b]
[spoiler]Yes, confirmed working by VR testers (see Credits). You'll need the [url=https://www.nexusmods.com/skyrimspecialedition/mods/58101]VR Address Library for SKSEVR[/url] instead of the SE/AE one.[/spoiler]

[font=Times New Roman][size=5]CREDITS[/size][/font]
[list]
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/85702]I4 - Inventory Interface Information Injector[/url] by [url=https://www.nexusmods.com/skyrimspecialedition/users/39501725]Parapets[/url] and [url=https://www.nexusmods.com/skyrimspecialedition/users/4569617]Jelidity[/url]
[*]Alchemy table keyword fix based on the [url=https://github.com/GroundAura/InventoryInjector/tree/alchemy-fix]I4 Alchemy Fix[/url] by [url=https://github.com/GroundAura]GroundAura[/url], used with permission
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/12604]SkyUI[/url] by the SkyUI Team and [url=https://github.com/doodlum/SkyUI-Community]community contributors[/url]
[*][url=https://skse.silverlock.org/]SKSE[/url] by the SKSE Team
[*][url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url] by [url=https://www.nexusmods.com/profile/meh321]meh321[/url]
[*][url=https://github.com/alandtse/CommonLibSSE-NG/tree/ng]CommonLibSSE-NG[/url] by [url=https://github.com/alandtse]alandtse[/url] and contributors
[*]VR testing by [url=https://www.nexusmods.com/profile/Patka250]Patka250[/url] and [url=https://www.nexusmods.com/profile/ITSCOMING]ITSCOMING[/url]
[/list]
```
