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
5. **[RECOMMENDATION] Implement the minimal native language runtime.** Build the bounded interpreter, fixed-capacity value model, compact program representation, native-function boundary, and resource accounting needed to host higher layers.
6. **[RECOMMENDATION] Implement the UI model and renderer.** Build the standard primitives, layout rules, rendering command format, focus system, and event dispatch, exposing them first through the native language.
7. **[RECOMMENDATION] Add application lifecycle and navigation.** Define how applications initialize, enter, receive events, update, render, leave, suspend, and recover.
8. **[RECOMMENDATION] Add persistent storage.** Implement a transactional, versioned, bounded, and power-loss-safe key-value store.
9. **[RECOMMENDATION] Port SilOS to the four target profiles.** Validate the portable core on the MCU, SBC, x86, and Browser targets defined in the hardware and platform model. Use these ports to test abstraction boundaries, footprint, responsiveness, and input behavior.
10. **[RECOMMENDATION] Add optional services as justified.** Networking, authentication, encryption, background tasks, sound, and dynamic application loading should be introduced only against concrete use cases.
11. **[DECISION] Decide whether a bare-metal kernel adds sufficient value.** Reassess this after the portable environment is working on both Browser and MCU targets.

---

## 2. Proposed Initial Constraints

- **[RECOMMENDATION]** The portable core must not depend on a host operating system.
- **[RECOMMENDATION]** MCU, SBC, x86, and Browser targets must share application and domain-logic code.
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
- **[LOCKED]** The four initial target profiles are MCU, SBC, x86, and Browser, as defined in the hardware and platform model.
- **[RECOMMENDATION]** Use capability profiles so very small boards can run a deliberately reduced SilOS configuration while larger targets provide additional applications and services without changing the core interaction model.
- **[DECISION]** Complete the unresolved details listed with the four target profiles, including storage configurations, the x86 host operating system, the Chrome support window, and whether WebAssembly is required.

---

## 3. Project Purpose and Charter

**[RECOMMENDATION] Proposed charter:**

> SilOS is a tiny, text-first operating environment for embedded control and information systems, sharing a common core, applications where capabilities permit, and the same interaction model across its MCU, SBC, x86, and Browser targets.

**[RECOMMENDATION] Product center of gravity:** SilOS should be an embedded control and information environment that can also be developed, simulated, demonstrated, and used in a browser.

**[LOCKED] Primary use:** The first complete SilOS system will be a compact personal-tools environment intended to demonstrate the platform's potential. Its initial application set will include a to-do list and calendar to exercise persistent storage and general UI, an alarm to exercise system-initiated activity and notifications, and a chat application to exercise networking.

**[OPTION] General-purpose hobby OS:** SilOS could eventually own the full machine and provide kernel facilities directly.

**[OPTION] Embedded UI framework:** SilOS could focus narrowly on displays and controls for appliances and devices.

**[OPTION] Fictional-computer environment:** SilOS could prioritize atmosphere, storytelling, props, games, and interactive installations.

**[OPTION] Portable personal environment:** SilOS could provide a compact collection of tools and user data across devices.

**[DECISION] Scope boundary:** Explicitly list features that SilOS will not support in its first major version.

---

## 4. Definition of “Operating System”

**[RECOMMENDATION] Initial system form:** Build SilOS first as a portable runtime with an OS-like shell. The MCU target may use a thin board-support layer, vendor SDK, or RTOS. The SBC and x86 targets should initially run as lightweight hosted processes. The Browser target should run the same portable core through WebAssembly or an equivalent compiled form.

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
│ target adapters: MCU · SBC · x86 · Browser                │
│ display · input · storage · network · platform services   │
│ capability discovery · normalized events                  │
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

**[RECOMMENDATION] Platform renderers:** The same UI description should be renderable through the MCU, SBC, x86, and Browser platform adapters where their declared capabilities permit.

**[OPTION] Immediate-mode UI:** Rebuild the interface from application state every frame or update cycle.

**[OPTION] Retained UI tree:** Preserve a widget tree and update only changed state.

**[DECISION] Rendering model:** Select immediate, retained, or hybrid rendering after measuring implementation size, memory use, diff complexity, and testability.

**[DECISION] Layout model:** Define sizing units, minimum and maximum dimensions, overflow, clipping, responsive reflow, and behavior on very small displays.

**[DECISION] Raw drawing:** Decide whether applications may request a pixel surface and, if so, how that capability is isolated.

**[DECISION] Remote rendering:** Decide whether UI command streams are stable and serializable enough to support remote displays.

---

## 9. Hardware and Platform Model

**[LOCKED] Target profiles:** SilOS has four initial target profiles. This table is the authoritative definition of their reference hardware, display, and controls; other sections refer to the profiles by name.

| Target | Hardware/runtime | Display | Baseline controls |
|---|---|---|---|
| **MCU** | ESP32; exact board variant and storage configuration remain to be selected | 1.3-inch SH1106 128x64 monochrome OLED over I2C | GPIO- or I2S-connected buttons, optionally including a joystick |
| **SBC** | Raspberry Pi 3 Model B; initially a lightweight hosted process, with host OS and storage configuration still to be specified | ELEGOO 3.5-inch 480x320 HDMI TFT touch display | USB keyboard; touch hardware is present but is not yet a required baseline control |
| **x86** | x86-compatible PC; host OS and storage configuration remain to be specified | 1024x768 monitor over HDMI | USB keyboard |
| **Browser** | Recent Chrome; the support window and whether WebAssembly is mandatory remain to be specified | Browser Canvas with an optional synchronized semantic DOM; viewport size follows the browser environment | Keyboard |

The Browser display is provided by browser APIs rather than WebAssembly itself. All four targets use the portable core and differ through platform adapters and declared capabilities.

**[RECOMMENDATION] Capability tiers:** Define a minimal base profile shared by all viable targets, then optional profiles for color, networking, sound, touch, filesystem access, multitasking, and larger applications. Application manifests should declare their required profile and individual capabilities.

**[DECISION] Minimum viable MCU:** Decide whether hardware smaller than the MCU profile is a first-class execution target, a reduced-profile build target, or outside the supported minimum.

**[DECISION] SBC operating mode:** Decide whether the SBC remains a hosted target, later gains a kiosk image that boots directly into SilOS, or eventually receives a bare-metal port.

**[RECOMMENDATION] Common platform abstraction:** Provide narrow interfaces for displays, input, timers, persistent storage, networking, sound, GPIO, sensors, randomness, power management, and diagnostics.

**[RECOMMENDATION] Input normalization:** The portable core should receive semantic events without needing to know whether they originated from a keyboard, GPIO button, encoder, touch surface, or browser.

**[RECOMMENDATION] Optional capabilities:** Targets should declare available capabilities; absence of networking, sound, touch, or sensors must not prevent the core from running.

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

**[LOCKED] Native interpreted language:** SilOS will include its own deliberately minimal interpreted or bytecode-executed scripting language. The runtime, program representation, and standard environment must be designed for very small flash, RAM, and persistent-storage footprints.

**[LOCKED] Language-first implementation:** As much of SilOS as is practical will be written in the native language, including applications, shell behavior, UI composition, configuration logic, and portable services. C++ should be restricted to the interpreter/runtime, platform adaptation, primitive operations, and code whose timing, memory, bootstrapping, or hardware constraints genuinely require native implementation.

**[RECOMMENDATION] Bounded execution:** Each language invocation should have explicit limits for work, stack depth, live values, message production, and native calls. Exhausting a limit must yield a defined recoverable fault rather than corrupting the system or monopolizing the event loop.

**[RECOMMENDATION] No mandatory garbage-collected heap:** The smallest runtime profile should use fixed-capacity stacks, arenas, pools, interned constants, or immutable program data so tracing garbage collection and unconstrained allocation are not required.

**[OPTION] Dynamic applications:** Larger targets could eventually load application modules at runtime.

**[DECISION] Allocation policy:** Define whether heap allocation is forbidden, allowed only during initialization, or permitted through bounded pools.

**[DECISION] Scheduling semantics:** Specify priorities, fairness, blocking rules, timer resolution, watchdog integration, and maximum permitted handler duration.

**[DECISION] Concurrency model:** Define how interrupt, service, application, and render contexts exchange data safely.

**[DECISION] Language execution model:** Choose source interpretation, compact bytecode, threaded code, or another representation after measuring interpreter size, program density, RAM use, startup cost, and portability. Define whether source compilation occurs on-device, at build time, or both.

---

## 11. Application Model

**[RECOMMENDATION] Lifecycle:** Use a lifecycle resembling `init → enter → event/update → render → leave → suspend`, with explicit recovery and shutdown behavior where the target supports it.

**[RECOMMENDATION] Capability-based access:** Applications should request named capabilities for storage, networking, GPIO, sensors, system settings, and other privileged services.

**[RECOMMENDATION] Standard application contract:** Applications should expose identity, version, required capabilities, resource limits, commands, routes or screens, and lifecycle callbacks.

**[RECOMMENDATION] Native-language application contract:** The standard lifecycle, capability, event, storage, and UI APIs should be native-language interfaces. Statically packaged language programs should remain viable on the smallest targets; runtime loading and editing are optional capabilities rather than baseline requirements.

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

**[LOCKED] First reference application set and order:** Use a small, integrated personal-tools suite rather than forcing one application to prove every subsystem. Implement it in this order: to-do list for persistence and foundational UI; alarm for timers, system-initiated activity, and notifications; calendar for date-based data and more complex structured UI; and chat for networking and asynchronous events. The minimum complete behavior of each application is defined below.

**[LOCKED] Current-user application context:** The personal-tools applications operate within the context of the current user. That user may be authenticated on systems that support login, or may be the implicit local user on a single-user implementation. This application-level ownership model does not yet decide whether SilOS as a whole is single-user or multi-user, nor when authentication is required.

**[LOCKED] To-do data and persistence:** The to-do application provides the current user with a list of items. Every item has a description, an optional target date-time, and a status. The allowed statuses are `to do`, `in progress`, and `done`. The list is persistent and remains available after a system reboot.

**[LOCKED] To-do operations:** The current user can create, edit, and delete to-do items. Changing an item's status is part of editing it rather than a separate application-level operation.

**[LOCKED] Application-controlled to-do ordering:** The to-do application controls the order in which items are presented. Its initial policy may order by target date-time and then status, but the exact precedence, direction, and treatment of undated items are application details rather than platform-level requirements.

**[DECISION] Remaining to-do behavior:** Define filtering, field limits, capacity behavior, and deletion semantics only where these materially constrain the platform design. Timezone, clock-change, input, and display semantics for the optional target date-time remain part of the shared system time model.

**[LOCKED] Alarm data and operations:** The alarm application provides the current user with a list of alarms. Every alarm has a name, a date-time, a Boolean `one-time` flag, and a Boolean `disabled` flag. The user can create, edit, and delete alarms. Alarm records and state persist across system reboots.

**[LOCKED] Alarm triggering:** When the current time reaches the date-time of an alarm that is not disabled, the alarm application triggers a notification showing the alarm name. After a one-time alarm triggers, the application sets its `disabled` flag to true.

**[DECISION] Remaining alarm behavior:** Define missed-alarm and clock-change behavior only where these materially constrain the shared time, scheduling, persistence, or notification services. Notification acknowledgement, history, interruption, and presentation remain part of the shared notification model.

**[LOCKED] Calendar data, operations, and persistence:** The calendar application provides the current user with a list of calendar entries. Every entry has a start date-time, end date-time, title, and description. The user can create, edit, and delete entries. Calendar entries persist across system reboots.

**[DECISION] Remaining calendar behavior:** Define date-time validation, presentation, ordering, filtering, and field or capacity limits only where these materially constrain the platform design. Timezone, clock-change, input, and display semantics remain part of the shared system time model.

**[LOCKED] Chat-user data and operations:** The chat application provides the current user with a list of chat-users. Every chat-user has a name and an address, represented initially as a string. The current user can create, edit, and delete chat-users.

**[LOCKED] Chat message data and operations:** Each chat-user has a list of messages. Every message has a direction (`sent` or `recv`), text, a date-time, and a status (`pending` or `done`). The current user can create a new sent message and delete any message.

**[LOCKED] Chat networking:** Creating a sent message queues an outbound network message addressed to the recipient chat-user's address. When SilOS receives a network message and identifies it as a chat message, the message is received by the chat application.

**[LOCKED] Chat persistence and notification:** Chat-users and messages persist across system reboots. When a chat message is received, the chat application triggers a notification showing the sending chat-user and the message text.

**[DECISION] Remaining chat behavior:** Define address interpretation, network-message identification, message transport, delivery-status transitions, inbound sender matching, failure behavior, ordering, field limits, and capacity behavior only where these materially constrain the networking, persistence, notification, or platform interfaces.

**[DECISION] Notification model:** Define transient notices, acknowledged alarms, persistent faults, severity, history, and interruption rules.

---

## 13. Persistent Storage

**[RECOMMENDATION] Initial abstraction:** Start with a compact hierarchical key-value store rather than a general-purpose filesystem.

**[RECOMMENDATION] Storage properties:** Updates should be transactional, versioned, wear-aware, recoverable after interruption, and subject to strict size limits.

**[RECOMMENDATION] Platform mappings:** Map the store through target adapters: MCU persistent storage, SBC and x86 hosted storage, and Browser durable storage.

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

**[LOCKED] First network use case:** The first concrete networking requirement is asynchronous chat messaging. Creating a sent chat message queues an outbound network message to the recipient address; inbound network messages identified as chat messages are delivered to the chat application. The transport and addressing schemes remain open design decisions.

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

**[DECISION] Update recovery:** Define dual-image, rollback, rescue firmware, or other strategies appropriate to each target profile.

---

## 17. Language, Build System, and Tooling

**[LOCKED] Bootstrap implementation language:** The foundational runtime and platform substrate of SilOS will be implemented in C++.

**[LOCKED] Controlled language profile:** SilOS will use a deliberately controlled subset of C++. The precise subset and enforcement rules will be defined later, with portability, deterministic resource use, small binaries, and suitability for constrained targets as its governing objectives.

**[LOCKED] Native system language:** SilOS will have a small interpreted or bytecode-executed language of its own, and the majority of portable system behavior should be authored in it where resource and timing constraints permit.

**[RECOMMENDATION] Language character:** Favor a tiny, regular, easily parsed language with a small number of value types and control forms, first-class access to SilOS events and capabilities, deterministic error behavior, and no feature that requires a large runtime. Prefer semantic economy and compact stored programs over familiar syntax.

**[RECOMMENDATION] One language across roles:** Use the same core language for applications, shell commands, automation, configuration, and system services. Optional syntactic sugar or tooling must lower to the same compact core rather than introduce multiple runtimes.

**[RECOMMENDATION] Static and interactive use:** Support ahead-of-time packaging into firmware as the baseline. A REPL, source parser, runtime program loading, and on-device editing should be separable capabilities so constrained builds can omit them while still executing packaged programs.

**[RECOMMENDATION] Small native surface:** Expose a narrow, capability-checked set of C++ primitives. Native extensions must declare resource behavior and must not allow language code to bypass scheduling, storage, or capability rules.

**[DECISION] Core language design:** Define syntax, value types, mutability, functions, modules, errors, iteration, event handlers, and the minimum standard library.

**[DECISION] Program representation:** Decide whether deployed programs store source, bytecode, threaded code, AST-like data, or target-selected forms, and define validation and version compatibility.

**[DECISION] Memory semantics:** Define stack and frame limits, object lifetimes, strings and collections, allocation strategy, interning, sharing, and behavior on exhaustion.

**[DECISION] Trust boundary:** Decide whether all packaged programs are trusted or whether the interpreter validates programs and enforces capability and resource isolation strongly enough to run untrusted code on suitable targets.

**[RECOMMENDATION] Freestanding portable core:** Keep the core compatible with freestanding C++ where practical and avoid dependence on operating-system services or a full hosted standard library.

**[RECOMMENDATION] Stable C-compatible boundary:** Expose platform integration through a versioned C-compatible ABI so SilOS can call vendor C SDKs and interoperate with Rust, JavaScript/WebAssembly glue, test harnesses, and other languages without exposing compiler-specific C++ ABI details.

**[DECISION] C++ subset:** Define the permitted language versions and features, including exceptions, RTTI, virtual dispatch, templates, static initialization, allocation, standard-library components, recursion, and thread-local storage.

**[DECISION] Resource rules:** Define ownership conventions, permitted allocation phases, fixed-capacity collection requirements, stack limits, failure handling, and rules for code that may run in interrupts or other constrained contexts.

**[DECISION] Enforcement:** Choose compiler flags, warnings, static analysis, formatting, linting, binary-size reports, map-file checks, sanitizers, and CI gates that enforce the controlled subset.

**[RECOMMENDATION] Cross-target proof:** Compile the same minimal core, event loop, and screen for the MCU, SBC, x86, and Browser targets before expanding the system. Use the exercise to validate the C++ subset, platform ABI, binary size, RAM, startup behavior, and toolchain assumptions.

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

**[LOCKED] Source code licensing:** SilOS source code is licensed under the MIT License. Licenses for fonts, artwork, documentation, examples, and third-party components remain to be selected.

---

## 21. Immediate Decisions to Work Through

The next design discussion should resolve or narrow these questions in order:

1. **[LOCKED] Primary use case:** Deliver a compact personal-tools environment comprising a to-do list, calendar, alarm, and chat application, collectively demonstrating UI, storage, system-initiated activity, notifications, and networking.
2. **[LOCKED] Application scope:** The minimum complete behavior of the to-do list, alarm, calendar, and chat applications is defined in the shell and core applications section. The implementation order is to-do list, alarm, calendar, then chat.
3. **[LOCKED] Target profiles:** Begin with the MCU, SBC, x86, and Browser profiles defined in the hardware and platform model. Unresolved details are recorded in the authoritative profile table.
4. **[DECISION] Minimum display:** What is the smallest supported character or pixel geometry? The MCU display is the smallest selected reference surface and therefore the starting constraint.
5. **[DECISION] Interaction grammar:** Which inputs are universal, and how do focus, back, commands, alerts, and application switching behave?
6. **[DECISION] Rendering model:** Strict cells, grid-aligned pixels, or hybrid; retained, immediate, or hybrid UI state?
7. **[DECISION] Resource budgets:** What flash, RAM, startup, latency, and power targets define “small”?
8. **[DECISION] Controlled C++ profile:** Which C++ language version, features, runtime facilities, library components, and enforcement rules form the approved SilOS subset?
9. **[DECISION] Native language core:** What is the smallest syntax, value model, execution representation, memory discipline, and native API that can express the shell, reference applications, UI composition, and portable services within the target budgets?

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
| Target profiles | **[LOCKED]** The initial targets are MCU, SBC, x86, and Browser, whose authoritative hardware, display, and control definitions are in the hardware and platform model. |
| Bootstrap implementation | **[LOCKED]** The interpreter/runtime, platform adapters, primitives, and constraint-critical substrate will be implemented in a deliberately controlled C++ subset. |
| Native system language | **[LOCKED]** SilOS will include a minimal interpreted or bytecode-executed language, and as much portable system behavior as practical will be written in it. |
| Primary use | **[LOCKED]** The first complete system will be a compact personal-tools environment with to-do, calendar, alarm, and chat applications that collectively exercise UI, storage, notifications, system-initiated activity, and networking. |
| Application order | **[LOCKED]** Implement the reference applications in this order: to-do list, alarm, calendar, then chat. |
| Source code license | **[LOCKED]** SilOS source code is licensed under the MIT License. |
