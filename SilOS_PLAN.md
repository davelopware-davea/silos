# SilOS Design and Implementation Plan

## Introduction

**[LOCKED] Document purpose:** This is the living design and implementation plan for **SilOS**. Its purpose is to identify the characteristics of the system that must be defined, record the alternatives under consideration, capture recommendations, and preserve decisions once they have been agreed.

This document is intentionally broader than an implementation checklist. It covers the product's purpose, visual language, interaction model, runtime architecture, hardware abstractions, application model, storage, security, networking, reliability, tooling, and delivery sequence. We will refine it together until the important characteristics are explicit enough to implement without relying on unstated assumptions.

### Status annotations

Every substantive proposal is annotated with one of these four values:

| Annotation | Meaning |
|---|---|
| **DECISION** | A question that must be resolved before the relevant design can be considered complete. |
| **OPTION** | One possible answer or approach worth considering. It is not currently preferred or final. |
| **RECOMMENDATION** | The current preferred answer, but it has not yet been finalized. |
| **LOCKED** | An agreed decision. It should not change without deliberately reopening it and recording why. |

**[LOCKED] Project name:** The project is called **SilOS**.

### Governing design principles

**[LOCKED] Conceptual economy:** SilOS should be built from the smallest practical set of distinct concepts. Before introducing a new primitive, abstraction, mechanism, state model, or interaction rule, the design must first try to express the requirement by reusing or composing concepts that already exist. Similar ideas should be unified where doing so preserves clarity and capability. This principle applies across the UI, application model, services, storage, events, hardware interfaces, tooling, and documentation.

**[LOCKED] Size and hardware reach over speed:** Small code and memory footprints, low resource requirements, and the ability to run across a wide range of hardwareâ€”including very constrained devicesâ€”are more important than maximizing execution speed. Performance should remain adequate and predictable, but an optimization that materially increases implementation size, conceptual complexity, or minimum hardware requirements must justify that cost against a concrete need.

---

## 1. Implementation Sequence

The sequence below is ordered to resolve high-impact design questions early, allow rapid visual iteration, and defer complexity until it is justified.

1. **[RECOMMENDATION] Write the system charter and measurable constraints.** Define what SilOS is, who and what it is for, what its smallest supported device looks like, and what it explicitly will not attempt to do.
2. **[RECOMMENDATION] Define the visual language and interaction rules.** Establish color roles, typography, box styles, spacing, focus, navigation, input methods, animation, alerts, and accessibility rules.
3. **[RECOMMENDATION] Define the portable platform interface.** Specify the contracts for display, input, timing, storage, networking, GPIO, sensors, randomness, power state, and diagnostics.
4. **[RECOMMENDATION] Build a browser-based simulator first.** Use it for fast iteration while making embedded constraints—such as screen size, available memory, input devices, CPU speed, and power interruption—visible and testable.
5. **[RECOMMENDATION] Implement the UI model and renderer.** Build the standard primitives, layout rules, rendering command format, focus system, and event dispatch.
6. **[RECOMMENDATION] Add application lifecycle and navigation.** Define how applications initialize, enter, receive events, update, render, leave, suspend, and recover.
7. **[RECOMMENDATION] Add persistent storage.** Implement a transactional, versioned, bounded, and power-loss-safe key-value store.
8. **[RECOMMENDATION] Port SilOS to one reference target in each hardware class.** Begin with a constrained Arduino-compatible MCU or Raspberry Pi Pico-class board, then validate the hosted platform layer on a Raspberry Pi Zero or full Raspberry Pi board. Use these ports to test abstraction boundaries, footprint, responsiveness, and input behavior.
9. **[RECOMMENDATION] Add optional services as justified.** Networking, authentication, encryption, background tasks, sound, and dynamic application loading should be introduced only against concrete use cases.
10. **[DECISION] Decide whether a bare-metal kernel adds sufficient value.** Reassess this after the portable environment is working on both browser and MCU targets.

---

## 2. Proposed Initial Constraints

- **[RECOMMENDATION]** The portable core must not depend on a host operating system.
- **[RECOMMENDATION]** Browser and embedded targets must share application and domain-logic code.
- **[RECOMMENDATION]** The core should be capable of running without heap allocation after initialization.
- **[RECOMMENDATION]** The UI must remain usable in monochrome; color is a semantic enhancement rather than the only carrier of meaning.
- **[RECOMMENDATION]** Every essential function must be accessible using a keyboard or directional controls.
- **[RECOMMENDATION]** Applications must not call platform-specific APIs directly.
- **[RECOMMENDATION]** Persistent configuration updates must survive interruption and unexpected power loss.
- **[RECOMMENDATION]** Networking must be optional rather than a core operating requirement.
- **[RECOMMENDATION]** The system must remain useful offline.
- **[RECOMMENDATION]** The smallest useful configuration should contain only the shell, settings, diagnostics, and one application.
- **[RECOMMENDATION]** Visual effects must never delay input or make system state ambiguous.
- **[RECOMMENDATION]** Resource exhaustion must result in an explicit, bounded, and recoverable state.
- **[DECISION]** Set measurable footprint targets for flash, RAM, startup time, idle power, input latency, and minimum display dimensions.
- **[LOCKED]** The intended target range includes Arduino-compatible boards, ESP32-class boards, Raspberry Pi Pico-class microcontrollers, Raspberry Pi Zero and full Raspberry Pi computers, and web browsers.
- **[RECOMMENDATION]** Use capability profiles so very small boards can run a deliberately reduced SilOS configuration while larger targets provide additional applications and services without changing the core interaction model.
- **[DECISION]** Select one initial reference device, display, and input configuration from each target class.
- **[DECISION]** Define the minimum browser support policy and whether the browser target requires WebAssembly.

---

## 3. Project Purpose and Charter

**[RECOMMENDATION] Proposed charter:**

> SilOS is a tiny, text-first operating environment for embedded control and information systems, sharing a common core, applications where capabilities permit, and the same interaction model across Arduino-compatible boards, ESP32 and Raspberry Pi Pico-class microcontrollers, Raspberry Pi computers, web browsers, and larger computers.

**[RECOMMENDATION] Product center of gravity:** SilOS should be an embedded control and information environment that can also be developed, simulated, demonstrated, and used in a browser.

**[DECISION] Primary use:** Decide whether the first release is primarily for real embedded control, fictional interfaces and installations, personal computing, education, or a deliberately supported combination.

**[OPTION] General-purpose hobby OS:** SilOS could eventually own the full machine and provide kernel facilities directly.

**[OPTION] Embedded UI framework:** SilOS could focus narrowly on displays and controls for appliances and devices.

**[OPTION] Fictional-computer environment:** SilOS could prioritize atmosphere, storytelling, props, games, and interactive installations.

**[OPTION] Portable personal environment:** SilOS could provide a compact collection of tools and user data across devices.

**[DECISION] Scope boundary:** Explicitly list features that SilOS will not support in its first major version.

---

## 4. Definition of “Operating System”

**[RECOMMENDATION] Initial system form:** Build SilOS first as a portable runtime with an OS-like shell. On Arduino-compatible, ESP32, and Raspberry Pi Pico-class MCU hardware it may use a thin board-support layer, Arduino core, vendor SDK, or RTOS. On Raspberry Pi Zero and full Raspberry Pi boards it should initially run as a lightweight hosted process on Linux. In the browser the same portable core should run through WebAssembly or an equivalent compiled target.

**[RECOMMENDATION] Shared-core architecture:** Keep core behavior, applications, data models, and visual semantics portable. Isolate hardware and browser dependencies behind small platform interfaces.

**[OPTION] Bare-metal kernel:** SilOS owns CPU initialization, memory, interrupts, task scheduling, and drivers.

**[OPTION] Firmware over an RTOS:** SilOS owns the product environment while an existing RTOS supplies scheduling and hardware services.

**[OPTION] Host application or shell:** SilOS runs as an application on an existing operating system.

**[OPTION] Portable application runtime:** SilOS provides its own UI, services, applications, and data model without claiming full ownership of every underlying machine.

**[DECISION] Kernel boundary:** Define which facilities are truly part of SilOS and which are delegated to the target platform.

---

## 5. System Architecture

**[RECOMMENDATION] Layered architecture:**

```text
┌───────────────────────────────────────────────────────────┐
│ SilOS text-first interface                                │
│ panels · forms · alerts · commands                        │
├───────────────────────────────────────────────────────────┤
│ applications and system services                          │
├───────────────────────────────────────────────────────────┤
│ portable core                                             │
│ events · tasks · storage · security                       │
├──────────────────┬───────────────────┬────────────────────┤
│ MCU adapters     │ Linux SBC adapter │ browser adapter    │
│ Arduino/ESP/Pico │ Pi Zero/full Pi   │ Canvas/DOM · input │
│ flash · HAL/RTOS │ files · OS I/O    │ IndexedDB · WASM   │
└──────────────────┴───────────────────┴────────────────────┘
```

**[RECOMMENDATION] Dependency direction:** Higher layers may depend only on stable interfaces exposed by lower layers. Portable applications must not import platform implementations.

**[DECISION] Core module boundaries:** Specify which services belong in the portable core, system-service layer, shell, and applications.

**[DECISION] Portability contract:** Decide how platform capabilities are discovered and how an application behaves when a capability is absent.

---

## 6. Visual Identity

**[RECOMMENDATION] Overall aesthetic:** Use an original industrial and institutional text-terminal language inspired by the general atmosphere of *Silo*, without copying its exact screens, graphics, names, or branding.

**[RECOMMENDATION] Default palette:** Use a very dark green-black background, restrained green foreground text, and yellow for selection, attention, active values, and warning emphasis.

**[RECOMMENDATION] Text-first presentation:** Most information and controls should appear textual, arranged into rectangular sections drawn with ASCII or box-drawing lines.

**[RECOMMENDATION] Information density:** Favor clear, compact information over modern card-based spacing and decoration.

**[RECOMMENDATION] Motion:** Use little animation. State changes should be immediate, purposeful, and never obstruct operation.

**[DECISION] Character model:** Choose between a strict character-cell grid, pixel-positioned text aligned to a grid, or a hybrid model.

**[DECISION] Glyph set:** Decide whether baseline layouts must use 7-bit ASCII, extended box-drawing characters, a bundled bitmap font, or target-dependent glyphs.

**[DECISION] Palette specification:** Define exact color values, contrast requirements, monochrome mappings, severity colors, and whether alternate themes are permitted.

**[DECISION] Typography:** Select or design the standard font, supported sizes, fallback rules, and minimum legible dimensions.

**[DECISION] Branding:** Define the boot mark, wordmark, naming conventions, voice, and original visual motifs for SilOS.

---

## 7. Interaction Model

**[RECOMMENDATION] Input priority:** Design first for keyboard and directional controls. Support mouse and touch without changing the underlying navigation model.

**[RECOMMENDATION] Immediate operation:** Startup and navigation should feel appliance-like, with no decorative delay.

**[RECOMMENDATION] Consistent focus:** Every interactive element should expose a visible focus state that remains understandable without color.

**[OPTION] Command-driven interaction:** Make a command prompt or command palette the central means of navigation and operation.

**[OPTION] Panel-driven interaction:** Make structured screens, lists, fields, and function-key actions the primary interface.

**[RECOMMENDATION] Hybrid interaction:** Use panel-driven applications with a universal command facility for expert operation, automation, and recovery.

**[DECISION] Navigation grammar:** Define global keys, back behavior, focus traversal, activation, cancellation, shortcuts, and modal behavior.

**[DECISION] Touch behavior:** Decide minimum target sizes and how touch maps to an intentionally text-dense interface.

**[DECISION] Accessibility model:** Define screen-reader support in browsers, reduced motion, contrast, scalable text, input remapping, and non-color state cues.

---

## 8. UI System

**[RECOMMENDATION] Small primitive set:** Construct standard screens from a compact set of primitives: text, box, divider, list, table, field, action, meter, status line, and modal prompt.

**[RECOMMENDATION] Declarative screens:** Applications should normally describe their UI through a tree or command list rather than draw pixels directly.

**[RECOMMENDATION] Platform renderers:** The same UI description should be renderable to an MCU framebuffer, serial terminal, browser Canvas, browser DOM accessibility representation, and desktop test harness where supported.

**[OPTION] Immediate-mode UI:** Rebuild the interface from application state every frame or update cycle.

**[OPTION] Retained UI tree:** Preserve a widget tree and update only changed state.

**[DECISION] Rendering model:** Select immediate, retained, or hybrid rendering after measuring implementation size, memory use, diff complexity, and testability.

**[DECISION] Layout model:** Define sizing units, minimum and maximum dimensions, overflow, clipping, responsive reflow, and behavior on very small displays.

**[DECISION] Raw drawing:** Decide whether applications may request a pixel surface and, if so, how that capability is isolated.

**[DECISION] Remote rendering:** Decide whether UI command streams are stable and serializable enough to support remote displays.

---

## 9. Hardware and Platform Model

**[LOCKED] Target families:** SilOS is intended to span Arduino-compatible hardware, ESP32-class microcontrollers, Raspberry Pi Pico-class microcontrollers, Raspberry Pi Zero and full Raspberry Pi single-board computers, and web browsers.

**[RECOMMENDATION] Three deployment classes:** Treat the target range as three architectural classes rather than pretending every board has identical resources:

1. **Constrained MCU:** Arduino-compatible, ESP32-class, and Raspberry Pi Pico-class boards using static firmware, bounded memory, direct peripherals, and optional RTOS support.
2. **Hosted SBC:** Raspberry Pi Zero and full Raspberry Pi boards initially running SilOS as a small Linux process with framebuffer, terminal, SDL-like, or direct display/input adapters.
3. **Browser:** A WebAssembly or equivalent portable-core build with Canvas/DOM rendering, browser input, and browser-backed persistence.

**[RECOMMENDATION] Capability tiers:** Define a minimal base profile shared by all viable targets, then optional profiles for color, networking, sound, touch, filesystem access, multitasking, and larger applications. Application manifests should declare their required profile and individual capabilities.

**[DECISION] Minimum viable MCU:** Decide whether very small 8-bit Arduino boards are first-class execution targets, build targets with a reduced feature set, or outside the supported minimum. “Arduino-compatible” spans hardware with radically different memory and processor capabilities.

**[DECISION] Raspberry Pi operating mode:** Decide whether Pi Zero and full Pi boards remain hosted Linux targets, later gain a kiosk image that boots directly into SilOS, or eventually receive a bare-metal port.

**[RECOMMENDATION] Common platform abstraction:** Provide narrow interfaces for displays, input, timers, persistent storage, networking, sound, GPIO, sensors, randomness, power management, and diagnostics.

**[RECOMMENDATION] Input normalization:** The portable core should receive semantic events without needing to know whether they originated from a keyboard, GPIO button, encoder, touch surface, or browser.

**[RECOMMENDATION] Optional capabilities:** Targets should declare available capabilities; absence of networking, sound, touch, or sensors must not prevent the core from running.

**[DECISION] Reference hardware:** Select one initial MCU board and one initial Raspberry Pi board, along with display technology, resolution, color depth, storage configuration, and physical controls. Additional representatives should be added only after the portable core works on the first two physical targets and in a browser.

**[DECISION] Display contract:** Define framebuffer ownership, pixel formats, partial updates, double buffering, orientation, refresh limits, and text acceleration.

**[DECISION] Time model:** Specify monotonic time, wall-clock time, time zones, timers, sleep, and behavior when no real-time clock exists.

**[DECISION] Power model:** Define sleep states, wake sources, state restoration, low-battery behavior, and power-loss notifications.

---

## 10. Execution and Memory Model

**[RECOMMENDATION] Single address space on MCU targets:** Keep the initial implementation small and avoid process isolation that the target hardware cannot efficiently provide.

**[RECOMMENDATION] Event-driven applications:** Applications should respond to input, timer, service, and lifecycle events rather than own uncontrolled execution loops.

**[RECOMMENDATION] Cooperative tasks by default:** Prefer deterministic cooperative scheduling, while allowing a platform RTOS to provide preemption for hardware services where necessary.

**[RECOMMENDATION] Bounded resources:** Use fixed-size queues, explicit limits, bounded buffers, and predictable failure behavior.

**[RECOMMENDATION] Static applications initially:** Link applications into firmware for the first embedded release.

**[OPTION] Dynamic applications:** Larger targets could eventually load application modules at runtime.

**[DECISION] Allocation policy:** Define whether heap allocation is forbidden, allowed only during initialization, or permitted through bounded pools.

**[DECISION] Scheduling semantics:** Specify priorities, fairness, blocking rules, timer resolution, watchdog integration, and maximum permitted handler duration.

**[DECISION] Concurrency model:** Define how interrupt, service, application, and render contexts exchange data safely.

---

## 11. Application Model

**[RECOMMENDATION] Lifecycle:** Use a lifecycle resembling `init → enter → event/update → render → leave → suspend`, with explicit recovery and shutdown behavior where the target supports it.

**[RECOMMENDATION] Capability-based access:** Applications should request named capabilities for storage, networking, GPIO, sensors, system settings, and other privileged services.

**[RECOMMENDATION] Standard application contract:** Applications should expose identity, version, required capabilities, resource limits, commands, routes or screens, and lifecycle callbacks.

**[DECISION] Application packaging:** Define the compiled representation, metadata format, identifiers, version compatibility, and registration mechanism.

**[DECISION] Isolation:** Decide whether applications are always trusted, logically isolated through APIs, or technically sandboxed on capable targets.

**[DECISION] Background work:** Define whether applications may run when not visible and how background resource use is bounded.

**[DECISION] Inter-application communication:** Choose messages, shared services, named data channels, or another deliberately small mechanism.

---

## 12. Shell and Core Applications

**[RECOMMENDATION] Minimum shell:** Provide startup, application navigation, global status, command access, notifications, and recovery controls.

**[RECOMMENDATION] Minimum system applications:** Include settings, diagnostics, event/log viewer, storage inspection, and one representative end-user application.

**[OPTION] Command console:** Provide an interactive textual console for administration, scripting, diagnostics, and recovery.

**[OPTION] Function-key shell:** Organize operation around persistent key legends and context-sensitive commands.

**[DECISION] First reference application:** Choose a concrete application that exercises live values, commands, persistence, alerts, and hardware simulation.

**[DECISION] Notification model:** Define transient notices, acknowledged alarms, persistent faults, severity, history, and interruption rules.

---

## 13. Persistent Storage

**[RECOMMENDATION] Initial abstraction:** Start with a compact hierarchical key-value store rather than a general-purpose filesystem.

**[RECOMMENDATION] Storage properties:** Updates should be transactional, versioned, wear-aware, recoverable after interruption, and subject to strict size limits.

**[RECOMMENDATION] Platform mappings:** Map the store to MCU flash on embedded targets, a file or database-backed implementation on Raspberry Pi/Linux targets, and IndexedDB or another durable browser mechanism in the browser.

**[OPTION] Filesystem facade:** Add a small path-based filesystem API above the key-value layer if application use cases require it.

**[DECISION] Data model:** Define key syntax, value types, maximum sizes, enumeration, namespaces, transactions, migrations, and deletion semantics.

**[DECISION] Durability guarantees:** Specify exactly when a write is considered committed and what can be recovered after power failure.

**[DECISION] Encryption:** Decide whether encryption at rest is required and how device keys are provisioned and recovered.

**[DECISION] Export and backup:** Define portable backup, restore, factory reset, and data migration behavior.

---

## 14. Security and Trust

**[RECOMMENDATION] Initial trust model:** Treat built-in applications as trusted while restricting their access through explicit capability interfaces.

**[RECOMMENDATION] Least authority:** Applications and services should receive only the capabilities necessary for their role.

**[DECISION] User model:** Decide whether SilOS is single-user, role-based, or multi-user.

**[DECISION] Authentication:** Decide when identity verification is required and which input-constrained authentication methods are acceptable.

**[DECISION] Threat model:** Document physical access, hostile networks, malicious applications, firmware replacement, stolen devices, and browser-host threats.

**[DECISION] Secure update:** Define signing, verification, rollback prevention, recovery, and support lifetime for firmware or application updates.

**[DECISION] Secret handling:** Define generation, storage, use, rotation, backup, and erasure of credentials and cryptographic keys.

---

## 15. Networking and Communication

**[RECOMMENDATION] Optional subsystem:** Keep networking outside the mandatory core and expose it through small asynchronous stream or message interfaces.

**[RECOMMENDATION] Protocol separation:** Keep HTTP, WebSockets, discovery, telemetry, and custom protocols in services or applications rather than the kernel.

**[OPTION] Byte streams:** Expose only connected streams and let higher layers implement protocols.

**[OPTION] Message transport:** Expose bounded addressed messages suited to telemetry and commands.

**[OPTION] Standard web protocols:** Provide optional HTTP and WebSocket services for browser integration and remote control.

**[DECISION] First network use case:** Identify the first concrete requirement—remote display, telemetry, configuration, updates, discovery, or device-to-device messaging.

**[DECISION] Connectivity policy:** Define offline behavior, reconnection, timeouts, metered links, network status, and user control.

**[DECISION] Remote command safety:** Define authentication, authorization, confirmation, replay protection, auditing, and safe failure.

---

## 16. Reliability and Recovery

**[RECOMMENDATION] Appliance behavior:** SilOS should start quickly, expose faults clearly, remain bounded under pressure, and recover without hiding failures.

**[RECOMMENDATION] Required mechanisms:** Include watchdog recovery, crash records, atomic configuration, safe-mode boot, a read-only recovery console, explicit degraded states, and size-limited logs.

**[RECOMMENDATION] Deterministic exhaustion:** Memory, queue, storage, and handle exhaustion should each have specified behavior that preserves diagnostic visibility.

**[DECISION] Fault taxonomy:** Define warnings, recoverable faults, application failures, service failures, and system-fatal conditions.

**[DECISION] Safe mode:** Specify entry conditions, available functions, disabled services, data access, and exit conditions.

**[DECISION] Logging:** Define event structure, severity, timestamps, persistence, redaction, retention, and export.

**[DECISION] Update recovery:** Define dual-image, rollback, rescue firmware, or other strategies appropriate to the reference hardware.

---

## 17. Language, Build System, and Tooling

**[LOCKED] Implementation language:** SilOS will be implemented in C++.

**[LOCKED] Controlled language profile:** SilOS will use a deliberately controlled subset of C++. The precise subset and enforcement rules will be defined later, with portability, deterministic resource use, small binaries, and suitability for constrained targets as its governing objectives.

**[RECOMMENDATION] Freestanding portable core:** Keep the core compatible with freestanding C++ where practical and avoid dependence on operating-system services or a full hosted standard library.

**[RECOMMENDATION] Stable C-compatible boundary:** Expose platform integration through a versioned C-compatible ABI so SilOS can call vendor C SDKs and interoperate with Rust, JavaScript/WebAssembly glue, test harnesses, and other languages without exposing compiler-specific C++ ABI details.

**[DECISION] C++ subset:** Define the permitted language versions and features, including exceptions, RTTI, virtual dispatch, templates, static initialization, allocation, standard-library components, recursion, and thread-local storage.

**[DECISION] Resource rules:** Define ownership conventions, permitted allocation phases, fixed-capacity collection requirements, stack limits, failure handling, and rules for code that may run in interrupts or other constrained contexts.

**[DECISION] Enforcement:** Choose compiler flags, warnings, static analysis, formatting, linting, binary-size reports, map-file checks, sanitizers, and CI gates that enforce the controlled subset.

**[RECOMMENDATION] Cross-target proof:** Compile the same minimal core, event loop, and screen for representative Arduino-compatible, ESP32 or Pico-class MCU, Raspberry Pi/Linux, and WebAssembly targets before expanding the system. Use the exercise to validate the C++ subset, platform ABI, binary size, RAM, startup behavior, and toolchain assumptions.

**[DECISION] Build system:** Choose the build, dependency, target configuration, asset-generation, and reproducibility strategy.

**[DECISION] Foreign-function boundary:** Define how board SDKs, C drivers, browser APIs, and optional host services connect to the portable core.

**[DECISION] Dependency policy:** Define acceptable licenses, version pinning, audit expectations, maximum dependency depth, and criteria for implementing functionality internally.

---

## 18. Browser Simulator

**[RECOMMENDATION] Simulator role:** Treat the browser target as both a real SilOS platform and the primary development environment for portable UI and applications.

**[RECOMMENDATION] Constraint simulation:** Allow profiles to limit display size, color depth, memory, storage, CPU budget, input devices, network state, and power stability.

**[RECOMMENDATION] Test controls:** Support simulated reset, power loss during writes, dropped input, time changes, device faults, network loss, and resource exhaustion.

**[OPTION] Canvas renderer:** Use Canvas for deterministic pixel rendering and close visual similarity to framebuffers.

**[OPTION] DOM renderer:** Use DOM elements for accessibility, inspection, and conventional browser integration.

**[RECOMMENDATION] Dual renderer:** Use Canvas as the visual surface with a synchronized semantic DOM representation for accessibility and automated inspection, if the footprint and complexity remain acceptable.

**[DECISION] Browser persistence:** Define the storage mechanism, quota behavior, export/import, and private-browsing degradation.

**[DECISION] Browser security boundary:** Define how the simulator accesses networking, files, clipboard, fullscreen, and connected hardware.

---

## 19. Testing and Quality

**[RECOMMENDATION] Cross-target conformance:** The portable core should run the same behavioral tests on host, browser, and embedded targets wherever practical.

**[RECOMMENDATION] Deterministic UI tests:** Test screens through stable render commands, semantic trees, snapshots, and input sequences rather than relying only on screenshots.

**[RECOMMENDATION] Fault injection:** Make allocation failures, queue saturation, storage corruption, interrupted writes, clock changes, and device disconnection routine test cases.

**[DECISION] Compatibility policy:** Define which serialized data, application APIs, UI descriptions, and platform interfaces remain stable between releases.

**[DECISION] Performance budgets:** Set limits for startup, frame/update time, input latency, application switch time, storage commits, flash, and RAM.

**[DECISION] Release gates:** Define the tests, measurements, supported targets, documentation, and recovery exercises required for a release.

---

## 20. Documentation and Decision Process

**[RECOMMENDATION] Living plan:** Update this document as questions are resolved, promoting items from `DECISION`, `OPTION`, or `RECOMMENDATION` to `LOCKED` only through explicit agreement.

**[RECOMMENDATION] Decision records:** Record significant locked choices in short architectural decision records containing context, alternatives, decision, consequences, and reopening criteria.

**[RECOMMENDATION] Reversible decisions first:** Prefer resolving foundational and expensive-to-reverse choices early while leaving replaceable implementation details open until evidence is available.

**[DECISION] Governance:** Define who may lock or reopen decisions, how disagreements are resolved, and how changes are versioned.

**[DECISION] Licensing:** Select licenses for source code, fonts, artwork, documentation, examples, and third-party components.

---

## 21. Immediate Decisions to Work Through

The next design discussion should resolve or narrow these questions in order:

1. **[DECISION] Primary use case:** What must the first complete SilOS system actually do?
2. **[DECISION] First reference application:** Which application will prove the system design rather than merely demonstrate its appearance?
3. **[DECISION] Reference hardware:** Which MCU board and which Raspberry Pi board, displays, and physical inputs will establish the real resource constraints?
4. **[DECISION] Minimum display:** What is the smallest supported character or pixel geometry?
5. **[DECISION] Interaction grammar:** Which inputs are universal, and how do focus, back, commands, alerts, and application switching behave?
6. **[DECISION] Rendering model:** Strict cells, grid-aligned pixels, or hybrid; retained, immediate, or hybrid UI state?
7. **[DECISION] Resource budgets:** What flash, RAM, startup, latency, and power targets define “small”?
8. **[DECISION] Controlled C++ profile:** Which C++ language version, features, runtime facilities, library components, and enforcement rules form the approved SilOS subset?

---

## 22. Locked Decisions Register

This section provides a compact index of decisions that are currently final.

| Area | Locked decision |
|---|---|
| Project identity | **[LOCKED]** The project is named **SilOS**. |
| Planning | **[LOCKED]** SilOS will be designed through a living plan that distinguishes open decisions, options, recommendations, and locked decisions. |
| Conceptual economy | **[LOCKED]** SilOS will favor reuse and composition, introducing new concepts only when existing ones cannot express the requirement clearly. |
| Resource priority | **[LOCKED]** Small footprint and support for very constrained hardware take priority over maximizing execution speed, while behavior must remain adequate and predictable. |
| Document order | **[LOCKED]** The implementation sequence and proposed initial constraints appear before the detailed design areas. |
| Target range | **[LOCKED]** SilOS is intended to support Arduino-compatible boards, ESP32-class boards, Raspberry Pi Pico-class microcontrollers, Raspberry Pi Zero and full Raspberry Pi boards, and browsers. |
| Implementation language | **[LOCKED]** SilOS will be implemented in C++ using a deliberately controlled subset whose precise rules will be defined later. |
