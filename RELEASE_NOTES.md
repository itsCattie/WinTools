# WinTools v1.1.0 Release Notes

## What's New

### Core Platform

- **Cross-platform support** — Linux and macOS compatibility with platform abstraction layer for hotkeys, theming, audio backends, and process management.
- **Application settings page** — Centralised configuration for theme selection, hotkey editor, startup behaviour, and update preferences.
- **First-run setup wizard** — Guided configuration of key preferences on initial launch.
- **System tray tooltip** — Dynamic status display showing current track, active module count, and more.
- **Automatic update checks** — Silent background update checks on startup with version display.
- **About dialog** — License information and credits accessible from the main window.
- **Consistent theming** — All window title bars, submenus, and context menus follow the active theme across every module.

### MediaBar

- **Sonos integration reimplemented** — Full UPnP-based Sonos support including volume control, speaker group management, queue listing, and favourites/playlists browsing.
- **Queue viewer** — Source-aware queue panel for both Spotify and Sonos with drag-and-drop reordering and per-track removal.
- **Spotify playlist browsing** — Browse and play Spotify playlists directly from the Library panel.
- **Keyboard shortcuts** — Playback controls when MediaBar is focused: Space (play/pause), Left/Right (prev/next), Up/Down (volume), S (shuffle), R (repeat), M (mini mode).
- **Repeat button** — Added next to skip forward.
- **Volume controls** — Integrated volume adjustment for the active source.
- **Multi-line lyrics** — Lyrics now spread across two lines when space is limited.
- **Album art caching** — Avoids re-fetching the same cover art on every poll.
- **Refreshed icons** — All icons normalised to 24×24 viewBox with `stroke="currentColor"` for dynamic theming.
- **Mini player fixes** — Scrolling text alignment and source selector readability improved.

### AudioMaster

- **New module** — Per-app volume control, mute toggle, VU meter visualisation, and audio device hotswap.
- **Device change events** — Auto-refreshes UI when devices are plugged or unplugged.
- **Virtual devices widget** — Manage virtual audio devices from the UI.
- **Cross-platform backends** — PipeWire/PulseAudio on Linux, CoreAudio on macOS.

### DiskSentinel

- **Performance optimised** — Parallel directory scanning and reduced UI lag during deep scans.
- **Context menu** — Right-click to open in Explorer, copy path, copy size, or navigate into folders.
- **Duplicate file detection** — Hash-based duplicate finder.
- **CSV export** — Export scan results to file.
- **Pie chart** — File type breakdown visualisation alongside the treemap.

### AdvancedTaskManager

- **Profiler overlay** — Customisable always-on-top performance overlay for any application.
- **Services tab** — Enumerate, start, stop, and restart Windows services.
- **Startup Apps tab** — Manage auto-start programs.
- **Network stats** — Per-process bytes sent/received.
- **GPU breakdown** — Per-process GPU usage in the Performance tab.
- **Live theme updates** — Overlay now updates when the theme changes.
- **Cross-platform process enumeration** — `/proc` on Linux, `sysctl`/`libproc` on macOS.

### GameVault

- **Platform logos** — Small icons for each platform for quick visual identification.
- **Platform compatibility** — Shows which OS each game supports (Windows, Mac, Linux).
- **Launch platform clients** — Open Steam, Epic, GOG, and other launchers directly from their menus.
- **Search and filter** — Filter bar for the game library with text search and platform filtering.
- **Sorting** — Sort by name, platform, playtime, last played, or recently added.
- **Tags and favourites** — Categorise games with custom tags and mark favourites.
- **SteamGridDB integration** — Automatic metadata and cover art fetching.
- **Xbox scanner** — Discover Xbox/Game Pass titles.
- **"Other" platform label** — Replaces the previous "Unknown" label.
- **Theme-aware text** — Game title news text colour updates correctly on theme change.
- **Environment-based paths** — Install paths use system environment variables instead of hardcoded locations.

### StreamVault

- **Watchlist tracker** — Local SQLite-based "want to watch" list.
- **Episode progress tracking** — Track progress through TV series.
- **Deeplink support** — Open specific content directly on streaming services.
- **Content notifications** — Polls TMDB for new content on tracked shows and films.
- **Custom services** — Add your own streaming services (home NAS, etc.).
- **Theme fixes** — Search results, film cards, card menus, and settings page all follow the active theme.
- **Search behaviour** — Enter key runs search only; typing is no longer interrupted by early results.

### LogViewer

- **Anti-spam** — Rate limiting on log output.
- **Level filtering** — Toggle info, warning, and error messages.
- **Export** — Save logs to file from the viewer UI.
- **Search** — Text filter for log entries.
