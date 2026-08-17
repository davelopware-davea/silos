# FreeRTOS kernel proof

**Related:** [FreeWisp plan](../plan.md) and the shared
[Emscripten FreeRTOS port](../emscripten-freertos-port/README.md).

This fixture builds the pinned FreeRTOS-Kernel with a cooperative Emscripten
fiber port. Each FreeRTOS task keeps a conventional C stack. Fiber metadata and
the Asyncify continuation stack are separate, port-owned allocations.

The first proof used deterministic yield-count ticks. The current proof uses a
Browser-specific 100 Hz tick configured in `FreeRTOSConfig.h`: every task yield
returns to a central scheduler fiber, which gives the JavaScript event loop an
opportunity to run and advances FreeRTOS from Emscripten's monotonic clock. The
idle hook sleeps until the next tick instead of spinning. The fixture verifies
two application tasks, elapsed task delays, a one-element queue, and an elapsed
one-shot software timer.

Configure and test from an Emscripten-enabled terminal:

```powershell
emcmake cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Serve the `build` directory over HTTP and open `index.html` to run the same
proof in a browser.
