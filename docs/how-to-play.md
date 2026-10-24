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

A white dot above a ship marks **your** ship, so you never need to tell the
colours apart to find yourself.

## Screen

The game always shows the full 1920 × 1080 play area, scaled to the window
with black bars when the window has another aspect ratio.
