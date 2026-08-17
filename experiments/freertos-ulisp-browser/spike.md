# FreeWisp - FreeRTOS + uLisp browser spike journal

**Short name:** FreeWisp

**Experiment plan:** [plan.md](plan.md)

**Project context:** [SilOS plan](../../docs/design/SilOS_PLAN.md). This is a
chronological journal; the plan above is its concise navigable summary.

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

## 2026-08-11 - Browser UI and Web Worker boundary

### Question

Can the combined runtime live entirely in a dedicated Web Worker while a
responsive browser page submits expressions, controls dispatch, and receives
results and runtime statistics through messages?

### Method

- Built a second worker-mode executable from the same uLisp/FreeRTOS runtime
  source and shared Emscripten port used by the command-line proof.
- Kept JavaScript-to-Wasm calls out of the page. A classic worker owns the
  generated loader and exchanges structured `evaluate`, `control`, `ready`,
  `status`, `result`, and `log` messages with the UI.
- Added a FreeRTOS client task that polls the worker inbox, sends expressions
  through the existing request queue, receives evaluator responses through the
  response queue, and posts timing, tick, yield, and heap values.
- Made Pause hold evaluation dispatch rather than freeze the kernel clock, so
  the cooperative runtime can still receive Resume. Single tick admits one
  queued expression while paused. Reset terminates and replaces the worker.
- Added a terminal, controls, a 128x64 monochrome activity canvas, and minimal
  live statistics.

### Observed result

- The original Node/CTest proof still passed after the shared-source change.
- Chromium evaluated `(defvar browser-state 40)` and later returned `42` from
  `(+ browser-state 2)`, proving state persisted across worker messages.
- While paused, `(+ browser-state 3)` remained queued; Single tick released it
  and returned `43` while the runtime remained paused.
- Reset created fresh state: `(boundp 'browser-state)` returned `nil`.
- A 100,000-iteration evaluation returned `100000` in 996.1 ms with 101
  budgeted safe-point yields. The UI reported kernel tick 1039 and 365 KiB of
  free FreeRTOS heap afterward. These are integration observations, not the
  controlled measurements required by sequence step 6.
- Chromium reported no console warnings or errors.
- The unoptimised worker build is 92,504 bytes of JavaScript and 497,575 bytes
  of WebAssembly.

### Result

Sequence step 5 is complete. The browser page remains outside the runtime and
communicates with it through a working Worker message boundary. Next, measure
size, memory, startup, scheduling behaviour, and pauses under a repeatable
workload; the activity canvas is diagnostic and does not yet prove Lisp-driven
display output.

## 2026-08-11 - Local browser launcher

Added `freertos-ulisp-task/serve.cmd` so the built worker UI can be served
again without reconstructing the HTTP-server command. From the repository
root, run:

```powershell
experiments\freertos-ulisp-browser\freertos-ulisp-task\serve.cmd
```

The script verifies that `build/index.html` exists and that Python is on
`PATH`, then serves only the target's `build` directory at
`http://127.0.0.1:8765/`. It binds to the loopback interface, runs in the
foreground, and stops with `Ctrl+C`. If the build is absent, configure and
build the target using the commands in `freertos-ulisp-task/README.md` first.

## 2026-08-12 - Repeatable measurement harness

### Purpose

Prepare sequence step 6 so measurements come from a fixed workload through the
real Chromium Worker boundary rather than from isolated interactive runs.

### Method

- Added a self-running `benchmark.html` page that starts a fresh Worker and
  records cold worker-ready time and JavaScript/Wasm artifact sizes.
- Added fixed warm-up, short-arithmetic, long-evaluation,
  allocation-pressure, and explicit-GC cases with per-sample JSON output.
- Reported Worker round-trip and evaluator time, kernel tick, safe-point
  yields, current and minimum FreeRTOS heap, and current Wasm memory size.
- Instrumented the evaluator's existing `testescape()` hook to record its
  longest uninterrupted interval and counts over 100 ms and 250 ms. The final
  interval through evaluation completion is included, so non-yielding garbage
  collection can be observed.

### Observed result

- Both JavaScript files passed `node --check`.
- The rebuilt command-line proof passed CTest in 2.25 seconds.
- The loopback server returned the benchmark page and script with HTTP 200.
- The unoptimised worker build is 93,287 bytes of JavaScript and 499,929 bytes
  of WebAssembly after adding measurement fields.
- After Codex restarted with the refreshed Browser plugin, three complete
  Chromium runs produced 66 samples with no browser warnings or errors. Raw
  results are preserved in
  `freertos-ulisp-task/measurements/2026-08-12-chromium.json`.
- Fresh Worker ready times were 136.6 ms, 85.5 ms, and 67.6 ms. Wasm memory
  stayed at 17,563,648 bytes; current and minimum-ever FreeRTOS heap both
  remained at 373,776 bytes throughout the workload.
- Across 30 short-arithmetic samples, evaluator time averaged 17.0 ms
  (13.2-26.7 ms). Across nine 100,000-iteration samples, it averaged 3,851.0
  ms (2,799.7-5,025.4 ms) with 255.4 budget yields on average.
- Across nine allocation-pressure samples, evaluator time averaged 879.5 ms
  (597.6-1,257.4 ms). Fifteen explicit-GC samples averaged 16.2 ms total
  evaluator time; the longest measured uninterrupted interval was 1.1 ms.
- The longest uninterrupted interval anywhere in the three runs was 10.6 ms.
  No interval exceeded 100 ms or 250 ms.

### Result

The repeatable harness is implemented, regression-tested, and verified through
the Chromium Worker boundary. The next spike action is to interpret these
measurements and decide whether the workload or instrumentation needs refinement
before judging scheduling, memory, and pause behaviour.

## 2026-08-12 - Measurement interpretation

### Question

Do the first three controlled Chromium runs support a step-6 viability judgment,
or does the harness need refinement first?

### Supported conclusions

- The tested evaluator and allocation paths met the spike's provisional pause
  policy: the largest gap between `testescape()` observations was 10.6 ms, with
  no gaps over 100 ms or 250 ms.
- The allocation-pressure expression creates far more temporary list cells than
  the configured uLisp workspace can hold, so its 1.5 ms maximum observed gap
  covers repeated automatic garbage collection as well as ordinary evaluation.
- The runtime remained stable through 66 requests. FreeRTOS heap and allocated
  Wasm linear memory did not grow during the workload.
- Worker-message and cooperative scheduling overhead is material for tiny work:
  short requests averaged 17.0 ms inside the worker and 26.2 ms round-trip. The
  client's 10 ms polling delay and event-loop/fiber transitions are included;
  this is not a pure evaluator benchmark.

### Limits of the evidence

- The long workload varied from 2,799.7 ms to 5,025.4 ms and was much slower
  than the earlier 996.1 ms interactive observation. The runs were performed in
  an automation-controlled background browser, so host load, browser warm-up,
  and background scheduling are uncontrolled confounders.
- The 67.6-136.6 ms Worker-ready values are fresh-Worker measurements on one
  already-loaded page, not clean-cache page-startup measurements.
- `HEAPU8.byteLength` reports allocated Wasm linear memory (16.75 MiB), not live
  or peak used memory. The unchanged 365 KiB FreeRTOS heap measures only the
  kernel allocator, not uLisp's static workspace, Emscripten stacks, or other
  Wasm memory.
- Final kernel ticks prove time advanced, but the harness does not measure tick
  lateness, catch-up bursts, or scheduler jitter directly.
- The pause instrument measures gaps between evaluator safe-point observations.
  It does not report garbage-collection count/duration directly or observe
  scheduler/event-loop stalls outside an evaluation.
- Artifact sizes are from an unoptimised Asyncify build with assertions enabled;
  they are a reproducible development baseline, not a release-size estimate.

### Decision

Refine the measurement harness before completing sequence step 6. Keep the
current workload for comparison, but add:

1. direct scheduler tick-lateness and catch-up statistics;
2. direct GC count and maximum-duration statistics;
3. clearer memory accounting for FreeRTOS, uLisp workspace, configured stacks,
   and Wasm linear-memory allocation;
4. an explicit visible-tab timing protocol with warm runs reported separately
   from a fresh page/Worker run; and
5. an optimised, assertions-disabled size build alongside the development build.

The existing pause result is encouraging but provisional. No SilOS-level
decision follows from these measurements yet.

## 2026-08-12 - Refined scheduling, GC, memory, and release measurements

### Question

Can direct instrumentation close the evidence gaps in sequence step 6 without
changing the proven runtime architecture?

### Method

- Added per-evaluation port counters for maximum tick lateness, scheduler passes,
  multi-tick catch-up events, and maximum ticks advanced in one pass.
- Inserted one GC start hook and one GC finish hook into the generated uLisp
  adapter, leaving the pinned upstream source unchanged.
- Reported FreeRTOS heap capacity/current/minimum, uLisp workspace/free bytes,
  allocated Wasm linear memory, and the Wasm dynamic-memory top.
- Distinguished a visible fresh-page Worker run from visible warm Worker reruns
  and fixed navigation-to-ready timing at the actual Worker `ready` message.
- Added an `-O2`, assertions-disabled Worker target for release-size comparison
  and ran the full workload through it once as a browser smoke test.
- Preserved three final development runs and one release smoke run in
  `freertos-ulisp-task/measurements/2026-08-12-chromium-refined.json`.

### Verification

- Development Worker, release Worker, and command-line proof all built.
- CTest passed after the instrumentation changes.
- Generated source contained exactly one safe-point hook and one GC start/finish
  pair. JavaScript syntax checks and `git diff --check` passed.
- One release relink was temporarily denied access to its existing Wasm file
  while the loopback server held it open. Stopping the server and retrying linked
  successfully; this was a Windows/OneDrive file-handle artifact.
- All 88 final Chromium samples completed with visible-page timing and no browser
  warnings or errors.

### Observed result

- Development artifacts total 597,419 bytes: 93,916 bytes of JavaScript and
  503,503 bytes of Wasm. Release artifacts total 422,640 bytes: 23,115 bytes of
  JavaScript and 399,525 bytes of Wasm, 29.3% smaller in total.
- The fresh development page reached Worker ready in 327.0 ms, of which Worker
  creation and runtime initialisation took 91.8 ms. Two warm Workers took 196.3
  ms and 117.2 ms. The release page/Worker figures were 269.3 ms and 153.7 ms.
- Allocated Wasm linear memory stayed at 17,563,648 bytes while the dynamic-memory
  top stayed at 749,568 bytes. The 524,288-byte FreeRTOS heap retained 373,728
  bytes current and minimum-ever free, so its measured peak use was 150,560
  bytes. uLisp's workspace was 73,728 bytes with 6,480-73,680 bytes free across
  the workload.
- Thirty short development requests averaged 17.7 ms inside the Worker and 28.8
  ms round-trip. Nine long requests averaged 5,921.9 ms and nine
  allocation-pressure requests averaged 1,411.7 ms; these host timings remain
  workload/environment observations rather than portability requirements.
- The nine long requests triggered 627 collections. Direct GC duration never
  exceeded 1.0 ms; the largest evaluator safe-point gap was 11.9 ms. No tested
  interval crossed 100 ms or 250 ms.
- Under the long workload, a tick was at most 68.2 ms late and the scheduler
  advanced at most seven ticks in one pass. Allocation pressure reached 23.6 ms
  and three ticks. Catch-up preserved logical kernel time but confirms that the
  Browser port cannot promise a regular physical 100 Hz cadence.
- The release Worker completed the full workload. Its long requests averaged
  4,497.3 ms, but one run is only a functional smoke test, not a stable
  development-versus-release performance comparison.

### Result

Sequence step 6 is complete: size, startup, memory, scheduling, GC, and pause
behaviour are now directly reported. The tested pause behaviour is comfortably
inside the spike's thresholds. Scheduler catch-up is acceptable for this
cooperative browser spike because the logical clock remains monotonic and the UI
runs in another thread, but tick cadence is not real-time and must not be treated
as such.

The remaining FreeWisp completion gap is the simulated display: it still shows
diagnostic page activity rather than Lisp/runtime-driven pixels. No SilOS-level
decision follows automatically, and the main SilOS plan remains unchanged.

## 2026-08-12 - Lisp-driven simulated display

### Question

Can Lisp running in the FreeRTOS task drive the 128x64 browser display through
the existing Worker boundary, including pause, single-step, and reset controls?

### Method

- Added `(display-clear [state])` and `(display-pixel x y state)` to the
  generated uLisp adapter without changing the pinned upstream source.
- Appended the generated builtin entries after every upstream entry so existing
  uLisp builtin indices remain unchanged.
- Stored pixels in a runtime-owned 1,024-byte packed monochrome framebuffer.
  Pixel operations change a revision counter; the Worker transfers the complete
  framebuffer only after an evaluation that changed the revision.
- Replaced the page's diagnostic activity graph with framebuffer rendering. The
  page reports lit-pixel count and revision and still never calls Wasm directly.
- Extended the command-line proof to clear the framebuffer, set pixel `(7,9)`
  from Lisp, and assert the corresponding packed bit.

### Observed result

- Development Worker, release Worker, and command-line proof rebuilt. CTest
  passed, and the direct proof printed `display_pixel_set=yes revision=2` and
  `FREEWISP_ULISP_TASK_PASS`.
- Generated source contained exactly one definition and one table entry for
  each display primitive.
- In Chromium, a three-pixel Lisp expression queued while paused left the canvas
  at `0 pixels / rev 0`. Single tick evaluated it, returned `t`, rendered
  `3 pixels / rev 4`, and left the runtime paused.
- The canvas recorded revision 4 and three lit pixels. Reset replaced the Worker
  and restored `0 pixels / rev 0`. Chromium reported no warnings or errors.
- The final development artifacts are 94,170 bytes of JavaScript and 505,342
  bytes of Wasm. The final release artifacts are 23,327 bytes of JavaScript and
  400,276 bytes of Wasm.

### Result

Sequence step 7 and the FreeWisp completion criteria are satisfied. The browser
now runs uLisp in a genuine FreeRTOS task, exchanges requests through queues,
delays without freezing the page, drives the simulated display from Lisp, and
reports the required measurements.

This completes the FreeWisp spike as an experiment. It supplies evidence for a
possible Browser substrate but does not itself promote any architectural choice
into the main SilOS plan; that requires explicit user approval.

## 2026-08-12 - Project-level conclusion promoted

With explicit user approval, the completed spike's conclusion was promoted to
the project plan: FreeWisp will be used provisionally as the Browser substrate
for the first end-to-end SilOS prototype.

This is deliberately narrower than adopting FreeRTOS or uLisp as the final
cross-target architecture. The prototype must still test ESP32 fit, shared
Browser/MCU code, live memory-to-display binding, and the continuing suitability
of the selected kernel and language.
