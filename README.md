# MHGU Recomp QoL

A ready-to-install quality-of-life plugin for **Monster Hunter Generations Ultimate Recompiled (MHGU Recomp)**. It speeds up gathering, streamlines quest deliveries and rewards, and adds optional convenience settings—all controlled from the in-game Forge menu.

The goal is to bring some of MHGU's dated, tedious systems closer to how newer games such as Monster Hunter: World and Monster Hunter Rise handle them, while staying careful with save data and multiplayer. Every feature can be turned off on its own.

**Current release:** v1.17.2 · Camp Box Fix

[Download the complete v1.17.2 installer](https://github.com/Blsilvery/MHGU-Recomp-QoL/releases/download/v1.17.2-camp-box-fix/MHGU_Recomp_QoL_v1.17.2_Camp_Box_Fix.zip) · [All releases](https://github.com/Blsilvery/MHGU-Recomp-QoL/releases) · [简体中文](README_zh-CN.md)

> **Requires:** MHGU Recomp, game version 1.4.0.

## Install

1. Download the complete ZIP from the link above.
2. Exit MHGU Recomp, then extract the ZIP.
3. Copy or merge the included `mods` folder into your MHGU Recomp game directory. Allow files with the same name to be replaced.
4. Start the game and press **Insert** to open Forge. Open **MHGU Recomp QoL** to see the settings.

The ZIP is the ready-to-install package: it includes the plugin, the menu resources used by the added in-game options, and the font for the Traditional Chinese Forge menu. Install the whole package rather than copying only the `.nro` file. The source files in this repository are for development; players should use the Release ZIP.

To update, exit the game and repeat the same extract-and-merge steps. No separate Switch mod loader is needed.

## Features

### Gathering and movement

- **Gather without tools:** mine and catch bugs without carrying a pickaxe or bug net.
- **Gather or carve in one action:** process the remaining uses at a gathering point in one interaction. An item already at its carry limit is lost, as when pressing A again in the base game.
- **Adjust gathering speed:** 1.00×–4.00×; default 1.50×. 1.00× is the game's speed, shown as off.
- **Adjust sprint before detection:** sprint speed is adjustable from 1.00×–2.00× (default 1.20×), and stamina cost from 0%–100% of normal (default 0%). 1.00× speed and 100% stamina cost are the game's own values, shown as off. These two settings apply only before a large monster detects the hunter.
- Batch gathering stops when the item pouch is full. Pouch size, item limits, and special gathering rules are currently the same as in the base game.

### Quest and account-item delivery

- **Automatic quest delivery:** each collected quest-goal item is submitted immediately and counts toward the required total, for the main quest and for delivery subquests alike. Extra items beyond the goal stay in the pouch.
- **Settle other account items:** non-goal account items are converted to account points as soon as they are collected. Turn this option off when you want to keep an account item for crafting.
- **Manual item-pouch delivery:** during a quest, press controller Y to deliver all eligible items, or select an item and press A to deliver only that item. This option has its own switch and works independently of automatic delivery.
- Automatic quest delivery follows the game's normal quest-progress flow for solo and multiplayer quests.

If **Put transport items in the item pouch** is enabled, eligible carry items such as eggs or powderstones go into the pouch instead of being carried. Automatic delivery skips these transport items. Disable the option to carry them normally.

### Rewards, storage, and other options

- Open the full item box from the camp courier. Before a quest, interact with the village or hub item box: A opens the full item box, while L + A (hold the left shoulder button, then press A) opens the Palico Board. During a quest, the camp courier always opens the full item box, and the quest HUD is hidden while it is open so it doesn't cover the menus. Turn off **Full item box** under **Items and rewards** to use the game's own boxes and courier again.
- **Send all rewards to the Item Box:** one switch adds a send-all entry to the quest reward and appraisal screens, the farm trade carts (all three at once), and the Meownster Hunters rewards. If the box is full, remaining items stay available; the mod does not sell or discard them.
- Ignore ambient heat and cold, so hot and cool drinks are not required.
- Keep Palico enthusiasm at 99.
- Set item-combination success to 100%.
- Keep the map and large-monster markers visible.
- Optionally equip three Hunter Arts with any hunting style. This changes the game's balance, so it is off by default; turn it on under **Hunting**.
- Optional quest shortcuts: **Start (+) + Down** completes the current quest; **Select (-) + Up** skips the quest-end wait. These shortcuts can be disabled in Forge.

## Languages

The Forge menu and the added game menu entries are in English, French, Spanish, German, Italian, Traditional Chinese, Simplified Chinese, and Korean. **Auto** (the default) follows the game's language. With a Korean translation patch such as [MHGU-KoreanPatch](https://github.com/ZellD-0371/MHGU-KoreanPatch), the game still reports English, so the plugin switches to Korean once the game shows Korean text (in the village or hub menus, or on the reward screen). Pick a language in the **Language** list at the top of the panel to override it.

The camp courier's item box menu uses the game's own text, including a translation patch's. The French, Spanish, German, and Italian menu texts, and part of the Korean, are new translations; corrections are welcome.

## Configure in game

Press **Insert** → **MHGU Recomp QoL**. The **Language** list is at the top, and the options are grouped in four submenus:

- **Gathering** — no tools needed, one-action gathering and carving, and gathering speed.
- **Items and rewards** — the full item box, send all rewards to the Item Box, transport items in the pouch, and item combinations.
- **Quests and delivery** — automatic delivery, settling other account items, manual item-pouch delivery, and the quest shortcuts.
- **Hunting** — hot and cool drinks, map and large-monster markers, Hunter Arts, sprint speed and stamina, and Palico enthusiasm.

Automatic delivery and manual item-pouch delivery are separate options. You can turn off each feature individually. If a feature can't run (for example because another mod changed the game function it needs), its line says it is unavailable and `log.txt` says why.

Your settings are saved automatically. A moment after you change a switch or slider, the plugin writes them to `mhgu_recomp_qol.ini` in the MHGU Recomp game folder, next to `mhgurecomp.ini`. The next time the game starts, it loads them from that file. The last line of the panel shows whether the settings were loaded or saved. To restore the defaults, exit the game and delete `mhgu_recomp_qol.ini`. Your game save data is not touched.

Settings from an earlier version are kept. Options that v1.17.0 merged into one (the message and reward switches, subquest delivery, and the speed switches) use the main option's saved value; their old lines are removed the next time the file is written. If something looks wrong after updating, delete the file to start from the defaults.

## Compatibility

The plugin works alongside other Forge plugins that hook the same game functions, such as Faster Field Hunter, in any load order. When two mods change the same thing, their effects may stack; turn off the matching option in one of them. If a cheat or patch changes a game function this plugin needs, only the feature that uses it is turned off, and `log.txt` names the address.

## Controls

- **Open the Palico Board:** before a quest, hold the controller's left shoulder button L and press A at the village or hub item box (keyboard: Q + A by default). Use A alone to open the full item box. At the camp courier during a quest, A opens the full item box.
- **Deliver items manually:** open the item pouch during a quest. Press controller Y to deliver all eligible items at once, or select one item and press A to deliver it individually. Enable **Deliver from the Item Pouch anywhere** in Forge; it is separate from automatic delivery.
- **Deliver a transport item:** automatic delivery skips transport items. If a transport item is in your item pouch, use the manual delivery controls above when you want to turn it in. The game's normal delivery eligibility rules still apply.

Made for MHGU Recomp with game version 1.4.0, the version MHGU Recomp supports.

The Traditional Chinese menu font is a subset of [Noto Sans CJK](https://github.com/notofonts/noto-cjk) by Adobe and Google, under the SIL Open Font License 1.1 (`OFL.txt` beside the font).

Some gathering code is based on Vuze's [MHGU Field QoL v0.7.0](https://github.com/mhvuze/mhgu-field-qol/tree/v0.7.0). This mod is released under the [MIT License](LICENSE); see it for attribution and license details. Monster Hunter Generations Ultimate and its game files are the property of Capcom; the license covers only this mod's own code and files, not the game's content.
