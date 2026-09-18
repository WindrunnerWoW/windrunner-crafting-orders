# Crafting Orders

A Windrunner module that lets players place crafting orders at existing profession trainers.

Talk to a trainer, hand over the materials and a gold fee, and the server makes the item, puts the enchant on, or disenchants for you. The player does not need to know the recipe. Each trainer only fulfills orders up to its own rank, so a trainer capped at 150 will not take an artisan recipe.

This is a port of [WoWGreymane/mod-crafting-Orders](https://github.com/WoWGreymane/mod-crafting-Orders) for Windrunner. It does not patch core sources and does not ship replacement DBC or MPQ files.

## What it supports

- Blacksmithing, Leatherworking, Alchemy, Tailoring, Engineering
- Enchanting (crafted formulas and direct item enchants)
- Jewelcrafting
- Disenchanting
- Recipe and formula hand-ins

Orders are limited by the trainer's highest profession rank (75, 150, 225, or 300). Recipe and formula unlocks are account-wide, but you still have to take a high-rank recipe to a trainer who can actually make it.

Inscription, milling, and prospecting are not in this build.

## Build

Turn the module on in CMake (`MODULES=static` is the supported mode) and rebuild. The native loader picks up `modules/mod-crafting-orders/src`.

If you are compiling on a live box and do not want the worldserver competing with the build, keep it single-threaded:

```sh
nice -n 15 ionice -c 3 cmake --build build -- -j1
```

Domain tests:

```sh
./t/run_tests.sh
```

## Configuration

Copy `conf/mod_crafting_orders.conf.dist` into the server `modules/` config directory.

Leave `CraftingOrders.Enable = 0` until the character migration has been applied on a staging copy.

`CraftingOrders.CraftingTimeMinutes` controls when the crafted item shows up. `0` puts it straight into inventory. `15` (the default) sends it to your mailbox after 15 minutes.

Add the module to `Database.AutoUpdate.AllowedModules` (or use `all`) so the character migration can run through the auto-updater.

## How trainers pick it up

With `CraftingOrders.TrainerGossip.Enable = 1`, the module finds profession trainers from their trainer data and adds gossip options. It does not rewrite `creature_template`, `creature`, `npc_trainer`, or `script_name`.

- Blacksmithing, Leatherworking, Alchemy, Tailoring, Engineering, and Jewelcrafting trainers offer crafting and recipe hand-ins.
- Enchanting trainers offer crafting, direct enchants, formula hand-ins, and disenchanting when `CraftingOrders.TrainerGossip.EnchantingDisenchant = 1`.

Every session is still checked on the server. A creature that is not a recognized trade-skill trainer is left alone. Recipe lists are validated when the player opens them. Turn `CraftingOrders.TrainerGossip.Enable` off if you want stock trainer gossip only.

You do not need to spawn any extra NPCs. After the character migration, any supported in-world trainer will do.

## Database

The only SQL this module ships is:

- `data/sql/character/0001_crafting_orders_character.sql`

That creates the recipe-unlock and cooldown tables. There is no World-DB change: trainers are discovered at runtime, so you do not add creatures, gossip, or `script_name` rows.

## Addon

Drop `AddOns/CraftingOrders/` into the client's `Interface/AddOns/` folder. Interface version is 11200.

The addon is only the UI. The server rechecks profession, spell, item, price, bags, and distance on every request. The list is paginated, with makeable/tier filters, quantity selection, and a confirm step before replacing a permanent enchant. Client requests go out as a Vanilla guild addon packet, which the server consumes before normal guild routing.

## Rollback

1. Set `CraftingOrders.Enable = 0`. Optionally also set `CraftingOrders.TrainerGossip.Enable = 0`.
2. Leave the unlock and cooldown tables in place unless you are sure you want to lose that data.
3. Disable or delete the client addon. No DBC or MPQ rollback is needed.
