# Twenty-Four (24 points)

Two versions of the 24 game:

- **web/** — the browser game. The solver is written in C (`solve.c`) and
  compiled to WebAssembly (`solve.wasm`); `index.html` is the UI. See
  `web/README.md` for how to build and run (serve over HTTP, or host on
  GitHub Pages).
- **terminal/** — the original-style C program (`main.c`): deals four cards
  and can reveal a solution. Build with `gcc main.c -o 24points -lm`.

Both play the same game: make 24 from four cards using + - * /.
