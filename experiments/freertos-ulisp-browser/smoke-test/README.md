# FreeWisp WebAssembly smoke test

**Related:** [FreeWisp plan](../plan.md).

This minimal program verifies that CMake, Ninja, Emscripten, and Node can
configure, compile, link, and execute a WebAssembly program together.

From the repository root in a terminal with the Emscripten SDK activated:

```powershell
emcmake cmake -S experiments/freertos-ulisp-browser/smoke-test -B experiments/freertos-ulisp-browser/smoke-test/build -G Ninja
cmake --build experiments/freertos-ulisp-browser/smoke-test/build
node experiments/freertos-ulisp-browser/smoke-test/build/freewisp-wasm-smoke.js
```

The final command should print:

```text
FreeWisp WebAssembly smoke test passed.
```
