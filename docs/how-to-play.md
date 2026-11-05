# How to play

## Goal

Survive the waves of Bydos coming from the right of the screen and shoot as
many as you can, together with up to three other players.

## Controls

| Action | Keys |
|---|---|
| Move up / down / left / right | Arrow keys, or `Z` / `S` / `Q` / `D` |
| Fire | `Space`, hold it to keep firing (4 missiles per second) |
| Mute / unmute every sound | `M` |
| Show / hide the metrics overlay | `F3` |
| Quit | Close the window |

Both key sets work at the same time, so you can play with either hand.

## Sound

A looping music plays during the game; each shot and each explosion has its
own sound. Every sound has a visual counterpart (a missile or a Bydo shot
appearing, a ship or a Bydo disappearing), so the game can be played without sound. Volumes are set when
starting the client:

```sh
r-type_client 127.0.0.1 4242 --music-volume 30 --effects-volume 80
r-type_client 127.0.0.1 4242 --mute
```

`M` mutes or unmutes everything in game.

## Rules

- The game starts as soon as the first player connects. A player who joins a
  running game gets a ship immediately.
- Each ship starts on the left side, on its own row.
- Bydos spawn at random heights on the right edge, one every 1.5 seconds,
  and fly to the left. Each Bydo fires an orange shot straight ahead 0.8
  seconds after appearing, then every 2.5 seconds.
- A missile that hits a Bydo destroys both. Missiles and Bydo shots pass
  through each other.
- A Bydo or a Bydo shot that touches a ship destroys the ship. A destroyed player watches
  until the next round.
- When every ship is destroyed, the game is over. A new round starts 3
  seconds later with a ship for every connected player.
- If a player quits or their client crashes, their ship disappears for
  everyone (within 5 seconds for a crash).

## Who is who

| Player slot (join order) | Colour |
|---|---|
| 1st player | Blue |
| 2nd player | Red |
| 3rd player | Green |
| 4th player | Yellow |

A slot freed by a player who leaves goes to the next one who joins.

## Screen

The game always shows the full 1920 × 1080 play area, scaled to the window
with black bars when the window has another aspect ratio.

### Metrics overlay

`F3` shows or hides a panel in the top-left corner, drawn over the game. It
shows:

| Line | What it shows |
|---|---|
| `FPS`, `frame`, `max` | Frames per second, average and worst frame time over the last 2 s |
| `Lua`, `physics`, `draw` | Average time per frame spent in client scripts, local physics and drawing |
| `Server tick` | Only when the server runs in the same process (`host` and `solo` modes): tick, Lua and physics time of the server |
| `Entities` | Entities on screen; in `host` and `solo` modes, also on the server |
| `CPU`, `memory` | CPU use of the process, in % of one core, and its memory |
| `Ping`, `lost`, `snapshots` | Round-trip time to the server, packets lost since connecting, world updates received per second |
| `In`, `out` | Network traffic per second |

Below the text, a graph shows the frame time of the last 2 s, one bar per
frame. A green bar is faster than 60 FPS, a yellow bar faster than 30 FPS and a
red bar slower. The green line marks 60 FPS, and the graph is capped at 50 ms.
