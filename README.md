# Rusty the Rat

A 16-bit Sega Genesis style platformer that runs in the browser. Open
`index.html` (or serve the repo root with any static server) and press Enter.

```
python3 -m http.server   # then visit http://localhost:8000
```

## The game

Rusty is a sewer rat with somewhere to be. Run, jump, and roll through two
acts, grabbing cheese (it works like rings — get hit and you scatter it,
get hit with none and you're done).

- **Sewer Zone** — line-scrolled shimmering water, a full 360° loop,
  springs, spikes, and patrolling robo-cats.
- **Bonus Stage** — a Sonic-2-style pseudo-3D half-pipe tunnel. Steer left
  and right around the pipe, grab cheese, dodge bombs.
- **Rooftop Run** — dusk skyline parallax, a corkscrew rail, a second loop,
  and the goal sign.

## Controls

| Key | Action |
| --- | --- |
| Arrows / WASD | Move |
| Z / X / Space | Jump (hold for height) |
| Enter | Start / pause |
| M | Mute |

## Tech notes

- 320×224 internal resolution (real Genesis output size), scaled up with
  nearest-neighbor filtering.
- No assets: all sprites, tiles, and backgrounds are generated procedurally
  on canvases at load. Sound and music are synthesized with WebAudio
  (square/triangle channels plus a noise hat, in FM-chip spirit).
- Genesis-style raster tricks: per-scanline line-scroll on the sewer water
  and rooftop clouds, multi-layer parallax, and the pseudo-3D tunnel.

The `src/` and `CMakeLists.txt` files are an unrelated native C++ project
that lives in this repo; the game is just `index.html` + `game.js`.
