# Cat Chess

Chess with cat pieces. Two-player pass-and-play or against a simple computer.
The rules run on a C++ chess engine compiled to WebAssembly; the page is just
the board and the cats.

## Files

| File | What it is |
|------|------------|
| `chess.cpp` | The engine. Keeps the object-oriented design — a `Piece` base class with one subclass per type (`Pawn`, `Knight`, …), each generating its own moves — over a 0x88 board. Full rules (castling, en passant, promotion, check/checkmate/stalemate), a small API for the UI, a one-ply "greedy" computer move, and `perft` for testing. |
| `chess.wasm` | `chess.cpp` compiled to WebAssembly (checked in). |
| `index.html` | The UI. Builds each piece in `pieceEl(type, color)` and calls the engine for everything about the rules. |
| `pieces/` | The piece sprites (`him_*` cream cats, `tux_*` tuxedos) as transparent PNGs. |
| `build.sh` | Rebuilds `chess.wasm` from `chess.cpp`. |

## The engine is verified

The move generator is checked with **perft** (counting legal move sequences
from the start position against known-correct totals):

```
perft(1) = 20     perft(2) = 400      perft(3) = 8902
perft(4) = 197281 perft(5) = 4865609
```

All match, so castling, en passant, promotion, and check legality are correct.

## Build

Compiling C++ to WebAssembly here uses plain `clang++` with the wasm32 target —
**no Emscripten needed**, because the engine is written freestanding (no STL,
no libc), so the `.wasm` has no imports and drops straight into the browser:

```sh
./build.sh
```

If you'd rather keep the original STL design (`shared_ptr`, `vector`), that
also compiles — with Emscripten (`emcc chess.cpp -o chess.js -sEXPORTED_...`) —
but then the engine pulls in libc++ and the page loads Emscripten's glue
instead of instantiating the bare module. The freestanding build above avoids
all of that.

## Run

Browsers won't `fetch()` the `.wasm` (or the piece PNGs) from a `file://`
path, so serve the folder:

```sh
python3 -m http.server 8000
# open http://localhost:8000
```

Or push the folder to GitHub Pages — it's a static site; the engine runs
client-side as WebAssembly.

## Notes

- The computer is intentionally simple (one ply: take a mate, grab the best
  capture, otherwise a little random). A stronger opponent would add a few
  plies of minimax with alpha-beta — a natural next step.
- Swapping in a different piece set is just replacing the PNGs in `pieces/`.
