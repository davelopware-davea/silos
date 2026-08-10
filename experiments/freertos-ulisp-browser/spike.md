# FreeWisp - FreeRTOS + uLisp browser spike journal

**Short name:** FreeWisp

Short chronological notes recording what we tried, why, and what happened. Keep conclusions provisional until supported by the experiment.

## 2026-08-10 - Initial direction

### Question

Can the real FreeRTOS kernel and uLisp run together in a browser while retaining useful code and design similarities with an ESP32 target?

### Decisions

- Use **FreeWisp** as the short name for this spike.
- Target a cooperative FreeRTOS WebAssembly port using Emscripten fibers and Asyncify.
- Run the runtime in a dedicated Web Worker so occasional scheduler or garbage-collection pauses do not freeze the UI.
- Use uLisp's `testescape()` path for budgeted safe-point yields; consider an explicit Lisp yield/sleep primitive as well.
- Keep uLisp single-threaded inside one FreeRTOS task and communicate with it through queues.
- Accept modest lag and jitter. Log pauses over 100 ms and investigate repeated pauses over 250 ms.
- Build with CMake, Ninja, and Emscripten from VS Code.

### Known prior art

- FreeRTOS supplies a POSIX simulator, but its pthread and signal model is a poor first fit for browser execution.
- Emscripten fibers provide cooperative contexts with separate stacks.
- An earlier uLisp WebAssembly experiment inserted yield hooks in the evaluator and loop implementation.

### Open checks

- Confirm current uLisp redistribution terms before vendoring source.
- Select and pin FreeRTOS, uLisp, and Emscripten versions.
- Verify the current uLisp source reaches `testescape()` often enough for bounded pauses.

### Result

Planning only. No source has been compiled or executed yet.
