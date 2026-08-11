# FreeRTOS uLisp task proof

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
