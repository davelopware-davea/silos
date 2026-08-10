# FreeWisp - FreeRTOS + uLisp browser spike

**Short name:** FreeWisp

**Experimental journal:** [spike.md](spike.md)

## Goal

Run the real FreeRTOS kernel with uLisp in a browser, learning what can be shared with the ESP32 target.

## Approach

- Compile FreeRTOS and uLisp to WebAssembly with Emscripten.
- Implement a cooperative FreeRTOS port using Emscripten fibers and Asyncify; do not use pthreads.
- Run the runtime in a dedicated Web Worker and keep the UI on the browser main thread.
- Run uLisp in one task; other tasks communicate with it through FreeRTOS queues.
- Add budgeted yields through uLisp's `testescape()` path and an optional explicit Lisp yield/sleep primitive.
- Keep garbage collection stop-the-world initially. Measure pauses, log those over 100 ms, and investigate repeated pauses over 250 ms.
- Use vanilla HTML, CSS, and TypeScript for a terminal, start/pause/reset/single-tick controls, a 128x64 monochrome display, and minimal scheduler statistics.
- Use VS Code with CMake, Ninja, and the Emscripten SDK.

## Sequence

1. Verify uLisp redistribution terms and pin dependency versions.
2. Run a standalone uLisp WebAssembly REPL.
3. Prove FreeRTOS tasks, delays, a queue, and a software timer.
4. Run uLisp as a yielding FreeRTOS task.
5. Connect the browser UI through worker messages.
6. Measure Wasm size, memory, startup time, scheduling behaviour, and pauses.

## Done when

A browser page runs uLisp inside a genuine FreeRTOS task, exchanges work with another task through a queue, delays without freezing the page, drives the simulated display, and reports the measurements above.

## Non-goals

Preemptive browser scheduling, ESP32 instruction-level emulation, polished UI, networking, and persistence.
