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

```powershell
emcmake cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Serve the `build` directory over HTTP and open `index.html` to run the proof in
a browser.
