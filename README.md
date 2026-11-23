# MHGU Recomp QoL

A ready-to-install quality-of-life plugin for **Monster Hunter Generations Ultimate Recompiled (MHGU Recomp)**. It speeds up gathering, streamlines quest deliveries and rewards, and adds optional convenience settings—all controlled from the in-game Forge menu.

**Current release:** v1.16.2 · Mod Compatibility

[Download the complete v1.16.2 installer](https://github.com/Blsilvery/MHGU-Recomp-QoL/releases/download/v1.16.2-mod-compatibility/MHGU_Recomp_QoL_v1.16.2_Mod_Compatibility.zip) · [All releases](https://github.com/Blsilvery/MHGU-Recomp-QoL/releases) · [简体中文](README_zh-CN.md)

> **Requires:** MHGU Recomp, game version 1.4.0.

## Install

1. Download the complete ZIP from the link above.
2. Exit MHGU Recomp, then extract the ZIP.
3. Copy or merge the included `mods` folder into your MHGU Recomp game directory. Allow files with the same name to be replaced.
4. Start the game and press **Insert** to open Forge. Open **MHGU Recomp QoL** to see the settings.

The ZIP is the ready-to-install package: it includes the plugin and the menu resources used by the added in-game options. Install the whole package rather than copying only the `.nro` file. The source files in this repository are for development; players should use the Release ZIP.

To update, exit the game and repeat the same extract-and-merge steps. No separate Switch mod loader is needed.

## Features

### Gathering and movement

- **Gather without tools:** mine and catch bugs without carrying a pickaxe or bug net.
- **Gather or carve in one action:** process the remaining uses at a gathering point in one interaction. To gather or carve only once, hold the controller's right shoulder button R and press A.
- **Adjust gathering speed:** 1.00×–4.00×; default 1.50×.
- **Adjust sprint before detection:** sprint speed is adjustable from 1.00×–2.00× (default 1.20×), and stamina cost from 0%–100% of normal (default 0%). These two settings apply only before a large monster detects the hunter.
- Batch gathering stops when the item pouch is full. It does not add pouch slots or bypass the game's normal item limits and special gathering rules.

### Quest and account-item delivery

- **Automatic quest delivery:** each collected quest-goal item is submitted immediately and counts toward the required total. Extra items beyond the goal stay in the pouch.
- **Automatic subquest delivery:** enable delivery-goal subquests to count the required items as they are collected.
- **Settle other account items:** non-goal account items are converted to account points as soon as they are collected. Turn this option off when you want to keep an account item for crafting.
- **Manual item-pouch delivery:** during a quest, press controller Y to deliver all eligible items, or select an item and press A to deliver only that item. This option has its own switch and works independently of automatic delivery.
- Automatic quest delivery follows the game's normal quest-progress flow for solo and multiplayer quests.

If **Put transport items in the item pouch** is enabled, eligible carry items such as eggs or powderstones go into the pouch instead of being carried. Automatic delivery skips these transport items. Disable the option to carry them normally.

### Rewards, storage, and other options

- Open the full item box from the camp courier. Before a quest, interact with the village or hub item box: A opens the full item box, while R + A opens the Palico Board. During a quest, both A and R + A at the camp courier open the full item box.
- Send all quest rewards and appraisal results to the item box, collect all three farm trade carts, or send all Meownster Hunters rewards to the box. If the box is full, remaining items stay available; the mod does not sell or discard them.
- Ignore ambient heat and cold, so hot and cool drinks are not required.
- Keep Palico enthusiasm at 99.
- Set item-combination success to 100%.
- Keep the map and large-monster markers visible, and equip three Hunter Arts with any hunting style.
- Optional quest shortcuts: **Start (+) + Down** completes the current quest; **Select (-) + Up** skips the quest-end wait. These shortcuts can be disabled in Forge.

Added reward, farm, and Meownster Hunters menu entries have text for the game's seven supported resource languages: English, French, Spanish, German, Italian, Traditional Chinese, and Simplified Chinese. Forge setting labels are in English.

## Configure in game

Press **Insert** → **MHGU Recomp QoL**. The main panel contains the individual switches; related settings are grouped in submenus:

- **Movement and gathering speed** — gathering speed, sprint speed, and sprint stamina.
- **Automatic quest delivery** — quest goals, subquest goals, and other account items.
- **Map, Hunter Arts and quest shortcuts** — map and marker display, Hunter Arts, and quest shortcuts.

Automatic delivery and manual item-pouch delivery are separate options. You can turn off each feature individually.

Your settings are saved automatically. A moment after you change a switch or slider, the plugin writes them to `mhgu_recomp_qol.ini` in the MHGU Recomp game folder, next to `mhgurecomp.ini`. The next time the game starts, it loads them from that file. The last line of the panel shows whether the settings were loaded or saved. To restore the defaults, exit the game and delete `mhgu_recomp_qol.ini`. Your game save data is not touched.

## Compatibility

The plugin works alongside other Forge plugins that hook the same game functions, such as Faster Field Hunter, in any load order. When two mods change the same thing, their effects may stack; turn off the matching option in one of them. If a cheat or patch changes a game function this plugin needs, only the feature that uses it is turned off, and `log.txt` names the address.

## Controls

- **Gather one at a time:** hold the controller's right shoulder button R and press A when interacting with a gathering point. This temporarily performs one gather or carve instead of the full batch. R means the controller button, not the keyboard letter.
- **Open the Palico Board:** before a quest, use R + A at the village or hub item box. Use A there to open the full item box. At the camp courier during a quest, A or R + A opens the full item box.
- **Deliver items manually:** open the item pouch during a quest. Press controller Y to deliver all eligible items at once, or select one item and press A to deliver it individually. Enable **Deliver from the Item Pouch anywhere** in Forge; it is separate from automatic delivery.
- **Deliver a transport item:** automatic delivery skips transport items. If a transport item is in your item pouch, use the manual delivery controls above when you want to turn it in. The game's normal delivery eligibility rules still apply.

Made for MHGU Recomp with game version 1.4.0, the version MHGU Recomp supports.

Some gathering code is based on Vuze's [MHGU Field QoL v0.7.0](https://github.com/mhvuze/mhgu-field-qol/tree/v0.7.0). This mod is released under the [MIT License](LICENSE); see it for attribution and license details. Monster Hunter Generations Ultimate and its game files are the property of Capcom; the license covers only this mod's own code and files, not the game's content.
