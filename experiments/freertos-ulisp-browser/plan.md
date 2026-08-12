# FreeWisp - FreeRTOS + uLisp browser spike

**Short name:** FreeWisp

**Experimental journal:** [spike.md](spike.md)

**Status:** Complete

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
   - **[done]** Pinned an unmodified minimal FreeRTOS-Kernel V11.3.0 snapshot at commit `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5`, with its MIT licence and revision metadata.
   - **[done]** Proved cooperative scheduling, delays, queue transfer, and a software timer with deterministic manual ticks.
   - **[done]** Replaced manual ticks with a Browser-specific 100 Hz event-loop-driven monotonic clock and verified delayed tasks and software timers against elapsed time.
4. Run uLisp as a yielding FreeRTOS task.
   - **[done]** Ran the pinned uLisp evaluator in a FreeRTOS task, exchanged expressions and results with another task through queues, and yielded from generated `testescape()` safe points on a 5 ms wall-clock budget.
5. Connect the browser UI through worker messages.
   - **[done]** Moved the combined runtime behind a dedicated Web Worker, connected a terminal and controls with structured messages, and reported live kernel/evaluation statistics without calling Wasm from the page.
6. Measure Wasm size, memory, startup time, scheduling behaviour, and pauses.
   - **[done]** Ran repeatable visible-Chromium development and release workloads with direct GC, scheduler, memory, startup, and artifact-size reporting.
   - **[done]** Observed a maximum 11.9 ms evaluator safe-point gap and 1.0 ms GC duration; no tested pause crossed 100 ms or 250 ms.
   - **[done]** Characterised the 100 Hz cooperative clock as logically monotonic with catch-up bursts of up to seven ticks after a maximum observed 68.2 ms lateness under load.
7. Drive the simulated display from Lisp/runtime output rather than diagnostic page activity.
   - **[done]** Added Lisp-visible `display-clear` and `display-pixel` primitives backed by a runtime-owned packed 128x64 framebuffer.
   - **[done]** Transferred changed framebuffers from the Worker to the page and verified paused queueing, single-step rendering, and reset in Chromium.

All sequence steps and the FreeWisp completion criteria are now satisfied.

## Results

- The real FreeRTOS kernel and pinned uLisp evaluator run together in browser WebAssembly without pthreads. Tasks, queues, delays, software timers, persistent Lisp state, and cooperative safe-point yields all worked.
- A dedicated Web Worker isolates the runtime from the page. Long evaluations do not freeze the UI, and structured messages carry requests, results, controls, statistics, and display framebuffers.
- The tested pause behaviour is suitable for this prototype: the largest evaluator safe-point gap was 11.9 ms and the longest measured garbage collection was 1.0 ms.
- The event-loop-driven 100 Hz clock preserves monotonic logical time but is not real-time. Under load, a tick was up to 68.2 ms late and the scheduler advanced up to seven ticks in one catch-up pass.
- The final optimised Browser artifacts total 423,603 bytes. The measured Wasm dynamic-memory top was about 750 KiB; this does not establish ESP32 memory use.
- Lisp successfully drove a runtime-owned 128x64 framebuffer through the Worker to the browser canvas. This proves the language-to-display path, not SilOS's live memory-to-display binding.

## Conclusion

FreeWisp is a viable Browser substrate for the first SilOS prototype and may be used provisionally for that purpose. The spike does not establish that FreeRTOS or uLisp is the final cross-target architecture, that the same configuration fits the reference ESP32, or that enough Browser and MCU code can be shared. Those questions belong to the end-to-end prototype.

## Done when

A browser page runs uLisp inside a genuine FreeRTOS task, exchanges work with another task through a queue, delays without freezing the page, drives the simulated display, and reports the measurements above.

## Non-goals

Preemptive browser scheduling, ESP32 instruction-level emulation, polished UI, networking, and persistence.
