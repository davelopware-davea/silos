# Standalone uLisp WebAssembly REPL

This target compiles the pinned, unmodified uLisp ESP 4.9a source with a small
Arduino compatibility layer. Hardware, filesystem, sleep, and Wi-Fi operations
are currently inert; the real uLisp reader, evaluator, allocator, garbage
collector, and printer are used.

Configure and build from the repository root:

```powershell
emcmake cmake -S experiments/freertos-ulisp-browser/standalone-ulisp -B experiments/freertos-ulisp-browser/standalone-ulisp/build -G Ninja
cmake --build experiments/freertos-ulisp-browser/standalone-ulisp/build
```

Run one expression under Node.js:

```powershell
node experiments/freertos-ulisp-browser/standalone-ulisp/build/freewisp-ulisp.js "(mapcar (lambda (x) (* x x)) '(1 2 3 4))"
```

For the browser REPL, serve the build directory over HTTP and open `index.html`.
For example:

```powershell
python -m http.server 8000 --directory experiments/freertos-ulisp-browser/standalone-ulisp/build
```
