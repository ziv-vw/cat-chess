# Cat Chess

A chess game with cat pieces! Play locally with a friend in pass-and-play mode or take on a simple computer opponent.

The game rules are handled by a C++ chess engine compiled to WebAssembly. The browser handles the board, cat sprites, and game interface.

## Files

| File | Description |
|---|---|
| `chess.cpp` | The chess engine, based on the original C++ project, with bug fixes and additional rules. |
| `chess.wasm` | The compiled WebAssembly version of the engine, included in the repository. |
| `index.html` | The game interface, including the board, piece rendering, and interaction with the engine. |
| `pieces/` | The chess piece sprites: `him_*` for cream-colored cats and `tux_*` for tuxedo cats. |
| `build.sh` | Builds `chess.wasm` from `chess.cpp`. |

## About the Engine

Rather than rewriting the project from scratch, I kept the original class structure and movement logic where possible, then fixed bugs and added support for the full set of standard chess rules.

- **Kept:** The original `Piece` class hierarchy (`Pawn`, `Knight`, and the other piece types), along with `Coordinate`, `Board`, `Game`, and `Player`. Each piece still implements its own `canMove()` function.
- **Fixed:** Several movement bugs, including black pawns capturing in the wrong direction, pawns jumping over pieces on their two-square move, and bishops and queens overlooking blocking pieces on diagonals.
- **Reworked:** Check, checkmate, and stalemate detection. The original `checking()` and `endGame()` logic had issues with simulated moves and an uninitialized flag. The new implementation checks attacked squares and uses make/undo to evaluate whether a move leaves the king in check.
- **Added:** Castling, en passant, and pawn promotion, which were not supported in the original project.
- **Adapted for WebAssembly:** Replaced `shared_ptr` with raw pointers managed by a small arena, changed the board from a vector to a fixed-size array, and removed `iostream` to work with the freestanding WebAssembly target.

## Testing

The engine was tested using *perft*, which counts the number of legal move sequences from the standard starting position at different depths.

```text
perft(1) = 20
perft(2) = 400
perft(3) = 8902
perft(4) = 197281
perft(5) = 4865609
```

All counts match the expected results for these positions, providing a useful check of move generation and legality, including special moves.

## Build

The engine is compiled with `clang++` targeting `wasm32`. Emscripten isn't needed because the engine uses no C++ standard library or C library functions, so the resulting WebAssembly module has no imports.

Run:

```sh
./build.sh
```

## Run Locally

The browser needs to load the WebAssembly file and piece images over HTTP, so opening `index.html` directly with a `file://` URL won't work. Start a local server from the project directory:

```sh
python3 -m http.server 8000
```

Then open [http://localhost:8000](http://localhost:8000) in your browser.

The project can also be hosted on GitHub Pages since it's a static site.

## Notes

- **Computer opponent:** The AI uses a simple one-ply strategy. It prioritizes delivering checkmate, then looks for good captures, and otherwise makes a somewhat random move. A minimax search with alpha-beta pruning would make it a stronger opponent.
- **Custom cat sprites:** To change the look of the pieces, replace the PNG files in `pieces/` with your own sprites using the same filenames.
