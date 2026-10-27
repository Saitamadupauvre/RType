# Accessibility

The game and its documentation must be usable by people with disabilities.
This page lists, for each category, what the game provides **today** and what
is **planned**, with the existing games that inspired each measure.

Status: ✅ available, 🔜 planned for the next releases.

## Physical and motor disabilities

| Measure | Status | Details |
|---|---|---|
| Hold to fire | ✅ | Holding `Space` fires continuously (4 missiles per second). No rapid repeated presses are ever required. |
| Two key sets at once | ✅ | Arrow keys and `ZQSD` both move the ship, so the game can be played with the right or the left hand only. |
| No precision timing on menus | ✅ | There are no menus to navigate: launching the client joins the game, and a new round starts automatically. |
| Generous pace | ✅ | Enemies are spawned at a fixed, moderate rate; the game never speeds up. |
| Remappable controls | 🔜 | Bindings are already data (an action → keys table in `game/scripts/lib/controls.lua`, read by the server from the raw keys clients send) and the input interface exposes `last_key_pressed()` for a remapping screen. Next: per-player bindings from a user file and the screen. Inspired by *Celeste* and *The Last of Us Part II*. |
| Gamepad support | 🔜 | Through the same action table. |
| Adjustable game speed | 🔜 | A server option scaling the fixed step, like *Celeste*'s assist mode. |

## Visual disabilities

| Measure | Status | Details |
|---|---|---|
| Not colour alone | ✅ | Your own ship is marked with a white dot, so you can find yourself without telling colours apart. |
| Distinct colours | ✅ | The four players are blue, red, green and yellow. |
| Scalable display | ✅ | The window is resizable and the full play area is scaled to it with letterboxing, so a large screen gives larger sprites. |
| High-contrast background | ✅ | A plain black background with small grey stars; entities are drawn on top in full colour. |
| Colour-blind palette | 🔜 | Replace red/green with a palette distinguishable under protanopia and deuteranopia (e.g. blue, orange, white, magenta), selectable in `lib/config.lua`. |
| Shape or number per player | 🔜 | A player number drawn next to each ship, as in *Overcooked*. |
| Fullscreen | 🔜 | The renderer already supports `set_fullscreen()`; add a key and a command-line flag. |
| Adjustable text size | 🔜 | For the HUD once it exists. |

## Audio and hearing disabilities

| Measure | Status | Details |
|---|---|---|
| No information conveyed only by sound | ✅ | Every sound has a visual counterpart: a shot spawns a missile, an explosion removes a ship or a Bydo. The game is fully playable muted. |
| Separate music and effects volumes | ✅ | `--music-volume` and `--effects-volume` (`0`–`100`) on the client; the audio interface keeps master, music and effects volumes apart. |
| Mute at any time | ✅ | `M` mutes and unmutes every sound; `--mute` starts muted. |
| Stronger visual cues for important sounds | 🔜 | A flash on hit and an explosion sprite, as in *Fortnite*'s sound visualisation. |
| In-game volume settings | 🔜 | A settings screen instead of command-line options. |

## Mental and cognitive disabilities

| Measure | Status | Details |
|---|---|---|
| Simple rules | ✅ | Move, shoot, avoid the Bydos. One enemy type, one weapon. |
| No setup | ✅ | No account, no lobby, no menu: start the client and play. |
| Automatic restart | ✅ | After a game over, the next round starts by itself after 3 seconds. |
| Clear identity | ✅ | Your ship is always marked. |
| Pause | 🔜 | A pause available at any time (needs a server-side pause in multiplayer). |
| Difficulty setting | 🔜 | Spawn rate and enemy speed are already tunables in `lib/config.lua`; next is a server option exposing presets. |
| On-screen feedback | 🔜 | A HUD showing the game state ("Game over", "Waiting for players") and the players present. |

## Documentation

| Measure | Status |
|---|---|
| Plain Markdown sources, readable in any editor or screen reader | ✅ |
| Website with search, keyboard navigation, light and dark themes, adjustable through the browser zoom | ✅ |
| Diagrams also described in the text next to them | ✅ |
| Tables instead of images for reference data | ✅ |
| Short sentences, one topic per page, a table of contents on every page | ✅ |

## How to contribute a measure

1. Open an issue describing the measure and who it helps.
2. Prefer data over code: a setting in `lib/config.lua` or a client option.
3. Update this page in the same pull request.
