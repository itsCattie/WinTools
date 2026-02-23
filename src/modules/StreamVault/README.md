# StreamVault

A multi-service streaming hub for WinTools that lets you search for movies and TV shows and jump straight to the streaming service that has them.

## Features

| Feature | Details |
| --- | --- |
| **Universal search** | Searches the TMDB catalogue for movies and TV shows and shows poster art, ratings, and synopses. |
| **Real availability** | On the detail page, TMDB's JustWatch-powered watch-provider data is fetched to show **only the services that actually carry the title**, filtered by country. |
| **Country selector** | A dropdown on the detail page lists every country where the title is available for subscription streaming. Defaults to your system locale. |
| **12 known services** | Netflix, Disney+, Max, Hulu, Prime Video, Apple TV+, Peacock, Paramount+, Crunchyroll, Funimation, Plex, and Tubi — shown with branded accent-colour buttons when available. |
| **Unknown providers** | If TMDB reports a provider that isn't in the known list, it's still shown as a greyed-out label so you know it exists. |
| **One-click redirect** | Clicking a service button searches that service's website for the title in your browser. |
| **Quick login** | Clicking a service name in the sidebar opens its login page. |
| **Sidebar open button** | Click `→` next to a service name to go straight to its homepage. |
| **Filter bar** | Filter results by All / Movies / TV Shows. |
| **Poster & backdrop art** | Images are downloaded asynchronously and cached for the session. |
| **Availability cache** | Provider data for each title is cached so revisiting the detail page doesn't re-fetch. |

## Setup

1. **Get a free TMDB API key**  
   Register at [themoviedb.org/settings/api](https://www.themoviedb.org/settings/api) — the free tier covers all use-cases needed here.

2. **Enter the key in StreamVault**  
   Open StreamVault → click **⚙ Settings** → paste your key and click **OK**.

3. **Start searching**  
   Type a title in the search bar and press Enter (or wait 400 ms for auto-search). Double-click any result card to open the detail page and choose a service.

## Supported Services

| Service | Accent Colour |
| --- | --- |
| Netflix | `#E50914` |
| Disney+ | `#0063E5` |
| Max | `#002BE7` |
| Hulu | `#1CE783` |
| Prime Video | `#00A8E6` |
| Apple TV+ | `#A2AAAD` |
| Peacock | `#F4C519` |
| Paramount+ | `#0064FF` |
| Crunchyroll | `#F47521` |
| Funimation | `#5B0BB5` |
| Plex | `#E5A00D` |
| Tubi | `#FA4E00` |

## Notes

- This module **does not** handle DRM, streaming playback, or account credentials. All logins and playback happen in the user's default web browser.
- Availability data comes from TMDB's watch-provider endpoint (sourced from JustWatch). It reflects subscription/flatrate services only — rent and purchase options are not shown.
- Provider availability is **per-country**. The country dropdown is populated only with countries where the title can be streamed via subscription. If no subscription options exist for any country, a "No streaming availability data found" message is shown instead.
- The context menu (right-click on a result card) provides quick *Search on …* entries for all 12 known services regardless of availability, as a manual fallback.
- TMDB provider IDs are mapped to known services; any provider not in the map is shown as a disabled label with its name.
