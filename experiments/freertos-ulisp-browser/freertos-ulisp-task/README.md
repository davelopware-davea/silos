# FreeRTOS uLisp task proof

**Related:** [FreeWisp plan](../plan.md), the shared
[Emscripten FreeRTOS port](../emscripten-freertos-port/README.md), and the
[standalone uLisp evaluator](../standalone-ulisp/README.md).

This target runs the pinned uLisp evaluator inside a genuine FreeRTOS task on
the shared cooperative Emscripten fiber port. A client task sends expressions
and receives printed results through FreeRTOS queues.

The build generates an adapter from the unmodified uLisp source and inserts a
single safe-point hook at the start of `testescape()`. The hook yields after a
5 ms wall-clock budget. Garbage collection remains non-yielding and
stop-the-world.

The proof checks persistent Lisp state and uses a long evaluation to verify
that safe-point yields allow another equal-priority task to run.

The same source also builds `freewisp-freertos-ulisp-worker.js`. The included
page starts that runtime in a dedicated Web Worker. Structured messages carry
expressions, results, controls, and statistics across the worker boundary; the
page never calls WebAssembly directly. Pause holds evaluation dispatch, Single
tick admits one queued expression, and Reset replaces the worker so Lisp and
kernel state start fresh.

```powershell
emcmake cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Serve the `build` directory over HTTP and open `index.html` to use the worker
UI. The original command-line proof remains the CTest target.

The worker runtime exposes `(display-clear [state])` and
`(display-pixel x y state)`. They update a runtime-owned packed 128x64
framebuffer; after evaluation, the Worker transfers changed pixels to the page
and the page renders them without calling WebAssembly directly.

Open `benchmark.html` from the same server to run the repeatable Chromium
measurement workload. It starts a fresh Worker, records startup and artifact
sizes, then runs fixed short, long, allocation-pressure, and explicit-GC cases.
The raw JSON result can be downloaded from the page for later comparison. The
first automatic run is labelled `fresh-page-worker`; button-triggered reruns are
labelled `warm-worker`, and page visibility is recorded. The build also emits an
optimised, assertions-disabled worker solely for release-size comparison.
Add `?runtime=release` to the benchmark URL to smoke-test that release Worker;
the default benchmark runtime remains the assertion-enabled development build.
