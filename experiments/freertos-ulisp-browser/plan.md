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
   - **[done]** Verified that uLisp ESP is distributed under the MIT License and may be redistributed with its license notice preserved.
   - **[done]** Pinned the unmodified uLisp ESP 4.9a source at commit `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81` with its upstream licence and revision metadata.
2. Run a standalone uLisp WebAssembly REPL.
   - **[done]** Installed and verified CMake 4.4.2, Ninja 1.13.2, Python 3.14.7, and Emscripten 6.0.6 in the Windows and VS Code development environment.
   - **[done]** Compiled, linked, and executed the minimal C-to-WebAssembly smoke test through CMake, Ninja, Emscripten, and Node.js.
   - **[done]** Found 12 upstream `testescape()` call sites covering the evaluator and several iterative, wait, delay, printing, and lookup paths, without yet claiming a strict pause bound.
   - **[done]** Compiled the real uLisp reader, evaluator, allocator, garbage collector, and printer to WebAssembly and verified arithmetic, lambdas, lists, and state retained across repeated calls.
   - **[done]** Served the included browser REPL and confirmed repeated interactive evaluation with persistent state in Chrome.
3. Prove FreeRTOS tasks, delays, a queue, and a software timer.
4. Run uLisp as a yielding FreeRTOS task.
5. Connect the browser UI through worker messages.
6. Measure Wasm size, memory, startup time, scheduling behaviour, and pauses.

## Done when

A browser page runs uLisp inside a genuine FreeRTOS task, exchanges work with another task through a queue, delays without freezing the page, drives the simulated display, and reports the measurements above.

## Non-goals

Preemptive browser scheduling, ESP32 instruction-level emulation, polished UI, networking, and persistence.
