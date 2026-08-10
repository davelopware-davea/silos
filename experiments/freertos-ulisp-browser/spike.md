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
