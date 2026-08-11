# FreeWisp - FreeRTOS + uLisp browser spike journal

**Short name:** FreeWisp

**Experiment plan:** [plan.md](plan.md)

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

## 2026-08-10 - Windows development environment setup

### Purpose

Create a repeatable native-Windows toolchain for building FreeWisp from VS Code,
using PowerShell for Emscripten setup and either PowerShell or Git Bash for normal
build commands.

### Initial audit

- Present: Git 2.39.0, VS Code 1.132.0, Node.js 24.14.0, npm, Chrome, and Edge.
- Not found on `PATH`: CMake, Ninja, Emscripten (`emcc`/`emcmake`), or Python.
- No existing `emsdk` directory was found at `C:\emsdk` or
  `C:\Users\dave\emsdk`.
- Python and a C/C++ compiler are supplied by the Emscripten SDK; a separate
  system Python installation is not currently planned.

### Pinned host tools

These were the current stable releases when setup began. Exact versions are
recorded rather than relying on a moving `latest` alias.

| Tool | Version | Official download/source |
| --- | --- | --- |
| CMake | 4.4.2 | [CMake downloads](https://cmake.org/download/) |
| Ninja | 1.13.2 | [Ninja v1.13.2 release](https://github.com/ninja-build/ninja/releases/tag/v1.13.2) |
| Python bootstrap | 3.14.7 installed | [Python Windows downloads](https://www.python.org/downloads/windows/) |
| Emscripten | 6.0.6 | [emsdk](https://github.com/emscripten-core/emsdk) and [official installation guide](https://emscripten.org/docs/getting_started/downloads.html) |

### Installation procedure and status

- [x] Install the CMake 4.4.2 Windows x64 MSI. In the installer, select the
  option to add CMake to `PATH` for the current user.
- [x] Open a new terminal and verify with `cmake --version`.
- [x] Download `ninja-win.zip` from the Ninja 1.13.2 release, extract
  `ninja.exe` into a stable tools directory, add that directory to the current
  user's `PATH`, then open a new terminal.
- [x] Verify Ninja with `ninja --version`.
- [x] Install a current Python Windows x64 release, selecting `Add python.exe
  to PATH`, then verify in a new terminal with `python --version`.
- [x] In PowerShell, clone `https://github.com/emscripten-core/emsdk.git` into a
  stable tools directory outside the SilOS repository.
- [x] From the `emsdk` directory, run `./emsdk.bat install 6.0.6`.
- [x] Run
  `./emsdk.bat activate --permanent 6.0.6`.
- [x] Open a new PowerShell terminal and verify with `emcc --version` and
  `Get-Command emcc, emcmake`; in Git Bash use `which emcc` and `which emcmake`.
  Restart VS Code and Git Bash before testing them there.
- [x] Compile and run a minimal WebAssembly program to verify the complete
  toolchain before adding uLisp.

### Notes

- Git Bash is suitable for Git and normal CMake/Ninja builds. PowerShell is
  used for the Emscripten install and activation because the supported Windows
  entry points are `.bat` files and Windows path handling is less ambiguous.
- FreeRTOS and uLisp will be pinned as source dependencies in the FreeWisp tree;
  they are not laptop-wide installations.
- uLisp ESP is distributed under the
  [MIT License](https://github.com/technoblogy/ulisp-esp/blob/master/LICENSE),
  so its source can be redistributed with its license notice preserved.

### Result

Setup instructions and initial state recorded. Installation and verification
remain pending.

#### CMake verification

CMake 4.4.2 was installed at `C:\Program Files\CMake\bin\cmake.exe`. Running
that executable reported `cmake version 4.4.2`, and the installer added its
directory to the machine `PATH`. The already-running Codex process retained its
old environment, so newly opened applications/terminals must be used before the
bare `cmake` command becomes visible there.

#### Ninja verification

Ninja was extracted to `C:\Users\dave\Tools\ninja\ninja.exe`. Running that
executable reported version `1.13.2`, and the bare `ninja --version` command was
confirmed working by the user in a newly opened Git Bash terminal. The
already-running Codex process again retained its old `PATH`.

#### Emscripten bootstrap failure and correction

The first Emscripten attempt ran `./emsdk.bat install 6.0.6 --permanent` and
stopped immediately with `Python was not found`. This disproved the initial
assumption above that no system Python was needed: although an installed SDK can
provide Python, the current Windows `emsdk.bat` needs Python to bootstrap the
installation. Install a current Python first. Also use `--permanent` only with
`activate`, not `install`; the corrected commands are:

```powershell
./emsdk.bat install 6.0.6
./emsdk.bat activate --permanent 6.0.6
```

#### Host and bundled Python verification

The user installed Python via the current Windows installer flow; although
3.14.6 was selected manually, `python --version` in a newly opened Git Bash
reported `Python 3.14.7`. This is the host/bootstrap Python selected by `PATH`.

`emsdk.bat install 6.0.6` then installed its own private
`python-3.13.3-64bit`, along with Node 24.19.0 and the Emscripten release tools.
This is expected: the private Python belongs to the pinned SDK and does not
replace the host Python. `emsdk list` confirmed SDK 6.0.6 and its components as
`INSTALLED`; activation remained pending.

#### First permanent-activation attempt

Running `./emsdk.bat activate 6.0.6 --permanent` selected SDK 6.0.6 but did not
write Emscripten entries to the Windows user `PATH`. Consequently a new Git Bash
still reported `emcc: command not found`. `emsdk help` documents the syntax as
`activate [--permanent] <tool/sdk>`, so option ordering matters here. The command
used by the repeatable procedure above was corrected to:

```powershell
./emsdk.bat activate --permanent 6.0.6
```

The corrected command made `emcc` available in newly launched standalone
PowerShell and Git Bash terminals. VS Code integrated terminals initially still
used the old environment because VS Code can keep background `Code.exe`
processes alive after a window is closed. Fully exit every VS Code process, then
launch VS Code from a standalone terminal in which `emcc --version` already
works. A window reload alone does not refresh the parent process environment.

After all `Code.exe` processes were stopped and VS Code was launched from a
working standalone terminal, Emscripten was confirmed available in both
PowerShell and Git Bash integrated terminals. The pinned host tool installation
and activation are complete; the minimal WebAssembly compilation test remains.

## 2026-08-10 - Minimal WebAssembly compilation smoke test

### Question

Can the installed CMake, Ninja, and pinned Emscripten toolchain compile and link
a minimal C program to WebAssembly, and can the generated module execute?

### Method

- Added a small checked-in CMake fixture under `smoke-test/` that prints a
  single success message.
- Configured it through `emcmake` with CMake 4.4.2 and Ninja 1.13.2.
- Built it with Emscripten 6.0.6, producing the normal JavaScript loader and a
  WebAssembly module.
- Executed the loader with Node.js 24.14.0 and inspected the module header.

### Observed result

- Configuration, C compilation, and WebAssembly linking all completed without
  compiler or linker errors.
- The build produced `freewisp-wasm-smoke.js` (56,190 bytes) and
  `freewisp-wasm-smoke.wasm` (5,512 bytes).
- Node printed `FreeWisp WebAssembly smoke test passed.` and exited with status
  zero.
- The module began with `00 61 73 6D 01 00 00 00`, the WebAssembly magic bytes
  and version 1 header.
- Codex's restricted execution sandbox initially denied access to SDK binaries
  and generated OneDrive files. Re-running those checks with the required local
  execution permission succeeded; this was not a toolchain failure.

### Result

The complete minimal WebAssembly build-and-run path is verified. The next spike
step is to select and pin the uLisp source version, inspect its `testescape()`
safe-point coverage, and begin the standalone uLisp WebAssembly REPL.

## 2026-08-10 - Pinned uLisp and standalone WebAssembly evaluator

### Question

Can the current uLisp ESP source be pinned without local edits, adapted through
a narrow platform layer, compiled to WebAssembly, and called repeatedly while
retaining Lisp state?

### Method

- Selected uLisp ESP 4.9a at upstream commit
  `aa9b24ca3323159dacadca60ea0e9ffdf00b1a81` (2 March 2026); upstream does not
  attach a tag to this commit.
- Vendored the unmodified `ulisp-esp.ino`, README, and MIT licence under
  `third-party/ulisp-esp/`, with explicit revision metadata.
- Counted and inspected `testescape()` calls in the pinned source.
- Added a minimal Arduino compatibility layer outside the upstream snapshot.
  Hardware, filesystem, Wi-Fi, and sleep operations are inert at this stage.
- Reproduced Arduino's generated forward declarations during CMake configure so
  the `.ino` compiles as ordinary C++ without modifying it.
- Exported `freewisp_eval()` for JavaScript and added a small persistent browser
  REPL page, plus Node-based CTest coverage.

### Observed result

- The pinned source has 12 `testescape()` call sites. They include the main
  evaluator dispatch and iterative printing, lookup, wait, delay, and traversal
  paths. This is promising safe-point coverage but does not establish a strict
  upper bound for every primitive or garbage collection.
- Emscripten 6.0.6 compiled and linked the upstream reader, evaluator, allocator,
  garbage collector, and printer successfully. The only compiler diagnostic was
  an upstream integer-to-float conversion warning in the random-number path.
- The build produced `freewisp-ulisp.js` (70,026 bytes) and
  `freewisp-ulisp.wasm` (183,772 bytes).
- Arithmetic and list CTests both passed in 0.59 seconds total. Direct checks
  returned `42` for arithmetic and `(1 4 9 16)` for a lambda/mapcar expression.
- Two evaluations in one module instance—`(defvar answer 40)` followed by
  `(+ answer 2)`—returned `answer` and then `42`, proving that state persists
  across the same exported call path used by the browser page.
- A temporary HTTP server returned the REPL page and WebAssembly module with
  status 200, and served the module as `application/wasm` with the expected
  183,772-byte length.
- Automated in-app-browser startup was blocked by a Windows sandbox permission
  error while accessing Codex's `AppData`, before the page could load. This is a
  remaining visual/browser interaction check rather than a Wasm build failure.

### Result

The current uLisp source is pinned and the standalone WebAssembly evaluator is
working with persistent state. Confirm the included REPL interactively in a
recent Chrome instance, then pin FreeRTOS and start the cooperative kernel
task/delay/queue/timer proof.

## 2026-08-10 - Manual Chrome REPL confirmation

### Observed result

The user served `standalone-ulisp/build/`, opened the REPL in Chrome, and
confirmed that the arithmetic, list/lambda, and persistent-state checks all
worked as expected.

### Result

Sequence step 2, the standalone uLisp WebAssembly REPL, is complete. Next, pin
FreeRTOS and begin the cooperative task, delay, queue, and software-timer proof.

## 2026-08-11 - Pinned FreeRTOS kernel source

### Question

Which upstream FreeRTOS kernel baseline and vendoring boundary should the
cooperative WebAssembly port use?

### Method

- Selected the current FreeRTOS-Kernel V11.3.0 release at commit
  `9b777ae5c5b8e9e456065a00294d1e5f5f9facf5`.
- Vendored an unmodified minimal snapshot: generic kernel sources, public
  headers, `heap_4`, and the MIT licence.
- Kept FreeWisp configuration and the future Emscripten port outside the
  upstream directory so they can evolve without patching the dependency.
- Compared SHA-256 hashes for all 29 copied upstream files against a clean,
  shallow checkout before removing the temporary checkout.

### Result

The kernel dependency is pinned and verified. The next decision is the first
cooperative scheduling mechanism to prove before implementing the port.

## 2026-08-11 - Deterministic cooperative kernel proof

### Question

Can the real FreeRTOS kernel schedule Emscripten fibers cooperatively while
preserving conventional C task stacks and supporting delays, queues, and
software timers?

### Method

- Added a FreeWisp Emscripten port outside the pinned kernel snapshot.
- Kept each FreeRTOS task allocation as its conventional C stack. Allocated
  the Emscripten fiber context and its 4096-byte Asyncify continuation stack as
  port-owned auxiliary storage, indexed by the task's stable stack marker.
- Advanced one deterministic kernel tick at every cooperative yield.
- Ran producer and consumer tasks at equal priority through a one-element
  queue, delayed the producer between messages, and started a one-shot
  software timer serviced by the real FreeRTOS timer daemon task.

### Observed result

- The CMake/Emscripten build completed with warnings treated as errors, except
  for one explicitly suppressed unused upstream trace counter.
- CTest passed in 0.73 seconds and printed
  `trace=PCTPCPC ticks=9 received=3 timer=1` followed by
  `FREEWISP_KERNEL_PROOF_PASS`.
- The trace proves both application tasks ran, all three queue values arrived
  in order, and the timer callback ran between task operations.
- The unoptimised Asyncify build produced 80,079 bytes of JavaScript and a
  104,133-byte WebAssembly module.

### Result

The deterministic proof satisfies the first half of sequence step 3. Next,
replace yield-count ticks with an event-loop-driven monotonic clock and verify
delays and software-timer timing against elapsed time.

## 2026-08-11 - Event-loop-driven monotonic clock

### Question

Can the cooperative port keep FreeRTOS time from the host event loop without
starving browser work, while preserving task delays and software-timer timing?

### Method

- Refactored all task yields through a central scheduler fiber rather than
  switching directly between task fibers.
- At each scheduler pass, yielded through Asyncify to the JavaScript event loop,
  measured `emscripten_get_now()`, and advanced every elapsed FreeRTOS tick.
- Used a Browser-specific 100 Hz tick rate. This is a platform configuration,
  not a project-wide or ESP32 tick-rate decision.
- Made the idle hook sleep until the next logical tick instead of spinning.
- Added elapsed-time assertions for two-tick producer delays and a five-tick
  one-shot software timer.
- Ran the proof repeatedly under Node and served the same generated module to
  the in-app Chromium browser.

### Observed result

- Ten consecutive CTest runs passed.
- A representative Node run reported
  `trace=PTCPCPC ticks=10 received=3 timer=1 timer_ms=56.04`
  with producer gaps of 13.93 ms and 48.15 ms. The gaps exceed the minimum
  elapsed duration; host event-loop jitter explains why they need not equal the
  nominal two-tick delay.
- Chromium reported
  `trace=PCPCTPC ticks=6 received=3 timer=1 timer_ms=52.90`
  with producer gaps of 18.70 ms and 26.30 ms, followed by
  `FREEWISP_KERNEL_PROOF_PASS`; its console had no warnings or errors.
- The resulting unoptimised Asyncify build is 80,470 bytes of JavaScript and
  105,497 bytes of WebAssembly.

### Result

FreeRTOS tasks, delays, a queue, and a software timer now work with a real
event-loop-driven clock in both Node and Chromium. Sequence step 3 is complete.
The next spike step is to run uLisp as a yielding FreeRTOS task.

### Follow-up refactor

Moved the proven Emscripten port into a shared `emscripten-freertos-port`
directory so the kernel proof and the combined uLisp runtime compile the same
port implementation while retaining independent `FreeRTOSConfig.h` files.
Made the task-table capacity and per-task Asyncify stack size target-configurable.
The kernel proof rebuilt successfully and passed three consecutive regression
runs after the move.

## 2026-08-11 - uLisp in a yielding FreeRTOS task

### Question

Can the pinned uLisp evaluator run inside a genuine FreeRTOS task, retain Lisp
state across queued requests, and yield often enough for another task to run
during a long evaluation?

### Method

- Added a combined FreeRTOS/uLisp target using the shared cooperative port.
- Kept the vendored uLisp source unchanged. During CMake generation, inserted
  one call to `freewisp_ulisp_safe_point()` at the start of the upstream
  `testescape()` function.
- Applied a 5 ms monotonic wall-clock budget at that hook. An expired budget
  calls `taskYIELD()` and begins a new budget after the task resumes.
- Kept uLisp garbage collection stop-the-world and non-yielding.
- Sent fixed-size expression requests and printed results through separate
  one-element FreeRTOS queues.
- Evaluated a global definition, a later reference to that definition, and a
  100,000-iteration expression. Kept an equal-priority observer task ready to
  prove that safe-point yields transferred execution to another task.

### Observed result

- Five consecutive initial CTest runs passed, followed by three final runs
  after explicitly hydrating OneDrive's generated JavaScript and Wasm files.
- A representative Node run returned `answer`, then `42`, proving retained
  Lisp state, and returned `100000` for the long evaluation.
- The representative long evaluation yielded 47 times; the observer ran 49
  times during it. Counts vary with host timing, as expected from a wall-clock
  budget.
- Chromium returned the same three results. The long evaluation yielded 100
  times and the observer ran 102 times during it; the page reported
  `FREEWISP_ULISP_TASK_PASS` with no console warnings or errors.
- The generated source contained exactly one inserted safe-point call.
- The unoptimised Asyncify build is 91,174 bytes of JavaScript and 501,229
  bytes of WebAssembly.
- Node intermittently reported the generated JavaScript as missing while
  OneDrive represented it as an unhydrated reparse point. The file was present,
  Chromium had already executed it successfully, and explicitly hydrating the
  generated JavaScript and Wasm restored repeatable Node execution. This was a
  host-filesystem artifact rather than a runtime failure.

### Result

Sequence step 4 is complete. The real uLisp evaluator now runs as a yielding
FreeRTOS task and communicates through FreeRTOS queues. The next step is to run
the runtime in a Web Worker and connect the browser UI through worker messages.
