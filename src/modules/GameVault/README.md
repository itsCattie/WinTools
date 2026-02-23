# GameVault

GameVault is a unified game library inside WinTools.

It pulls together every game you have installed across different stores and emulators and puts them in one place to browse, search, and launch.

## What it does

- Scans Steam, Epic Games, and GOG for installed and library games.
- Picks up emulator libraries from RetroArch, RPCS3, Yuzu, Ryujinx, Dolphin, and DeSmuME.
- Lets you add your own folders to catch any games outside a store.
- Shows cover art, playtime, last played date, and achievement progress.
- Lets you filter by platform, search by name, or show only installed games.
- Launches games and emulators directly without leaving WinTools.

## How it feels to use

You open GameVault and everything is already there. Games from every source sit in the same grid with their cover art loaded in. The sidebar lets you jump straight to a specific platform or narrow down to installed-only. Clicking a card opens a detail view with the hero banner, your playtime on record, and your achievement progress if the platform tracks it.

Launching works through each platform's own URI scheme when possible (so Steam games launch through Steam, GOG games through Galaxy) and falls back to the emulator or executable directly when needed.

## Everyday flow

1. Open GameVault from the WinTools launcher.
2. Browse the grid or use the search bar to find a game.
3. Click a game card to open its detail page.
4. Press **Play** to launch it.
5. Use the sidebar to filter by platform or toggle installed-only.
6. Open **Scan Paths** in the sidebar footer to add custom folders or set emulator paths.

## Quick note

Playtime and achievement data depend on what each platform stores locally. Steam, GOG, and some emulators track this automatically. Epic does not store playtime in its local manifests so those entries will show no playtime.
