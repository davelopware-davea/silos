# SilOS Plan

This is the authoritative plan for SilOS. It deliberately records only:

- principles and requirements we have agreed;
- the current milestone and the questions blocking it; and
- a short list of areas to explore later.

Ideas are not commitments. Detailed alternatives and earlier recommendations are preserved in [SilOS_DESIGN_BACKLOG.md](SilOS_DESIGN_BACKLOG.md) and should be reconsidered only when relevant work begins.

Project-level code ownership and platform boundaries are recorded in the
[SilOS code-layout discussion](Discussion-Code-Layout.md). Current exploratory
design material is kept separate: [UiRefs and templates](Discussion-VariableBindingAndTemplates.md),
[Shell UI](Discussion-Shell-UI.md), and
[storage and MQTT references](Discussion-QueueMetaphors.md). The completed
[FreeWisp plan](../../experiments/freertos-ulisp-browser/plan.md) supplies
Browser-substrate evidence.

## Contents

1. [Purpose](#1-purpose)
2. [Governing principles](#2-governing-principles)
3. [Committed foundations](#3-committed-foundations)
4. [Current milestone: prove the core idea](#4-current-milestone-prove-the-core-idea)
5. [Questions to answer during this milestone](#5-questions-to-answer-during-this-milestone)
6. [Later milestones](#6-later-milestones)
7. [Topics deliberately deferred](#7-topics-deliberately-deferred)
8. [Planning rule](#8-planning-rule)

## 1. Purpose

SilOS is a tiny, opinionated operating environment for embedded control, information systems, and compact personal tools. It will share a common core and interaction model across constrained hardware, conventional computers, and the browser.

The first complete system is a personal-tools environment containing:

1. a to-do list;
2. an alarm;
3. a calendar; and
4. chat.

Together these applications will exercise UI, persistent storage, time, notifications, system-initiated activity, and networking.

## 2. Governing principles

These principles are locked. Changing one requires an explicit decision.

### Opinionated and focused

SilOS will not try to be all things to all people. It will choose the problems it solves and solve them well rather than accumulate generality for its own sake.

### Conceptual economy

SilOS will use the smallest practical set of distinct concepts. A new primitive, abstraction, mechanism, or state model must earn its place; reuse and composition come first.

### Collapse layers

SilOS will remove or directly connect conventional OS and application-stack layers when their separation adds more indirection, duplicated state, code, or conceptual cost than value. A boundary must provide a concrete benefit such as portability, safety, isolation, or testability.

### Small footprint and broad hardware reach

Small code and memory footprints and support for constrained devices take priority over maximum execution speed. Performance must remain adequate and predictable.

### Live memory-to-display binding

Application code will be able to say, in essence, “show this value here.” The value represents a language-visible memory location. The OS observes the binding and updates the display automatically when the value changes, without the application issuing another draw or refresh command.

This simple mechanism should cover a large proportion of ordinary UI output. Its implementation must preserve that simplicity even if renderers internally use polling, notifications, invalidation, or dirty regions.

## 3. Committed foundations

### Implementation languages

- The foundational runtime and platform substrate will use a deliberately controlled subset of C++.
- SilOS will have a small interpreted or bytecode-executed language of its own.
- As much portable system behaviour as practical—including applications, UI composition, shell behaviour, configuration, and services—will be written in the SilOS language.
- C++ is reserved for the runtime, platform adaptation, primitive operations, and work whose hardware, timing, memory, or bootstrapping constraints require it.

### Initial targets

| Target | Reference environment | Display | Baseline controls |
|---|---|---|---|
| MCU | ESP32; exact board and storage not yet chosen | 1.3-inch SH1106 128×64 monochrome OLED over I2C | GPIO- or I2S-connected buttons, optionally a joystick |
| SBC | Raspberry Pi 3 Model B; initially hosted | ELEGOO 3.5-inch 480×320 HDMI touch display | USB keyboard; touch is present but not initially required |
| x86 | x86-compatible PC; host OS not yet chosen | 1024×768 HDMI monitor | USB keyboard |
| Browser | Recent Chrome | Browser display surface | Keyboard |

The targets share portable application and domain logic. Target-specific facilities sit behind narrow platform interfaces.

### Application order

Applications will be implemented in this order:

1. to-do list, proving foundational UI and persistence;
2. alarm, proving time, system-initiated activity, and notifications;
3. calendar, proving date-based data and more structured UI; and
4. chat, proving networking and asynchronous events.

Applications operate for the current user, who may be an implicit local user or an authenticated user. This does not yet decide the wider user or authentication model.

### Application requirements

#### To-do list

- An item has a description, optional target date-time, and status.
- Status is `to do`, `in progress`, or `done`.
- The current user can create, edit, and delete items. A status change is an edit.
- Items persist across reboot.
- The application controls presentation order; the exact ordering policy is not a platform requirement.

#### Alarm

- An alarm has a name, date-time, `one-time` flag, and `disabled` flag.
- The current user can create, edit, and delete alarms.
- Alarms persist across reboot.
- When an enabled alarm reaches its date-time, it raises a notification containing its name.
- A one-time alarm becomes disabled after triggering.

#### Calendar

- An entry has a start date-time, end date-time, title, and description.
- The current user can create, edit, and delete entries.
- Entries persist across reboot.

#### Chat

- A chat-user has a name and an address, initially represented as a string.
- The current user can create, edit, and delete chat-users.
- A message has a direction (`sent` or `recv`), text, date-time, and status (`pending` or `done`).
- The current user can create sent messages and delete messages.
- Creating a sent message queues an outbound network message to the recipient address.
- Inbound messages identified as chat messages are delivered to the chat application.
- Chat-users and messages persist across reboot.
- An inbound chat message raises a notification containing the sender and message text.

### Licensing

SilOS source code is licensed under the MIT License. Licences for fonts, artwork, documentation, examples, and third-party components remain undecided.

## 4. Current milestone: prove the core idea

Build the smallest end-to-end prototype that can test SilOS’s defining architectural claims.

The prototype is complete when it:

1. runs a minimal SilOS-language program;
2. binds language-visible values directly to screen locations;
3. updates their displayed representations automatically when values change;
4. accepts enough input to create, edit, and delete basic to-do items;
5. persists those items across restart;
6. runs on the Browser and MCU targets from substantially shared code; and
7. reports its flash/code size, peak RAM use, startup time, and input-to-display latency.

The first prototype may omit polish, general-purpose APIs, dynamic application loading, networking, authentication, sound, multitasking, and the later reference applications.

### Current variable-binding hypothesis

The current prototype direction is summarised in [Discussion: Variable Binding and Templates](Discussion-VariableBindingAndTemplates.md). It separates exposing a uLisp variable once from rendering that binding any number of times through reusable, screen-independent flow templates. A Shell UI task owns template interpretation and the framebuffer and initially redraws the complete active template on every refresh. These details remain hypotheses to test rather than settled architecture.

### Immediate next step

Build a Browser-first minimal to-do application in real uLisp. Use one active
application and the smallest proposed Shell boundary: app declaration, loading,
UiRefs/templates, Shell semantic input, and StoreRef-backed to-do rows. The
purpose is to expose where the proposed Shell, source-loading, handler, Ref,
and storage APIs need refinement when expressed as actual uLisp code. This is
an experiment within the current milestone, not adoption of the API sketches
as final interfaces.

### Provisional Browser substrate

The completed [FreeWisp spike](../../experiments/freertos-ulisp-browser/plan.md) demonstrated that the real FreeRTOS kernel and uLisp evaluator can run together in browser WebAssembly using cooperative Emscripten fibers. The runtime operated in a dedicated Web Worker, communicated through FreeRTOS queues and Worker messages, yielded during long evaluations, and drove a 128x64 framebuffer from Lisp without freezing the page.

For the first end-to-end prototype, SilOS will provisionally use FreeWisp as the Browser substrate. This is a prototype choice, not a final commitment to FreeRTOS or uLisp across all targets.

The evidence supporting this choice includes evaluator safe-point gaps no longer than 11.9 ms in the tested workload, garbage collections no longer than 1.0 ms, and final optimised Browser artifacts totalling about 424 KiB. The Browser scheduler preserves monotonic logical time but is not real-time: ticks may be late and advance in catch-up bursts.

FreeWisp's explicit pixel primitives prove the language-to-display path but do not answer the live memory-to-display binding questions. The prototype must still establish ESP32 fit, substantially shared Browser/MCU code, the shared platform interfaces, and whether uLisp and FreeRTOS remain appropriate beyond this provisional Browser role.

FreeWisp is complete and is no longer an active work plan. Its detailed plan, experimental journal, source, and measurements are historical evidence and should be loaded into context only when FreeWisp is specifically requested or referenced. The summary above is sufficient context for ordinary work on the current prototype.

## 5. Questions to answer during this milestone

These are the only active design questions. Prefer small experiments over speculative design.

1. What is the smallest language value and execution model that can express the prototype?
2. How does a live display binding identify a value and remain safe when that value changes type, moves, or expires?
3. Which scalar and collection values can initially be displayed, and how are they formatted and positioned?
4. How are bindings created, updated, and removed within fixed resource limits?
5. Can live bindings replace most of a conventional UI tree or render loop, or is one additional small concept required for input and layout?
6. What minimal display, input, timing, and storage interfaces are shared by Browser and MCU?
7. What persistence representation and interruption guarantees are sufficient for the first to-do list?
8. Which C++ features and runtime facilities are allowed in the prototype?
9. What resource measurements would show that the approach is viable on the reference MCU?

Record answers here only when evidence supports a decision. Record implementation detail in source documentation rather than expanding this plan.

## 6. Later milestones

Plan each milestone only when the preceding prototype supplies enough evidence.

1. Refine the to-do application and interaction model.
2. Add alarm and shared time/notification services.
3. Add calendar and structured date-based UI.
4. Add chat and the minimum networking service it requires.
5. Validate and refine the SBC and x86 targets.
6. Consider additional capabilities only against concrete use cases.

## 7. Topics deliberately deferred

The following areas matter, but do not need detailed recommendations yet:

- exact visual language, fonts, palette, layout, accessibility, touch, and mouse behaviour;
- general application lifecycle, packaging, loading, and capability manifests;
- storage beyond the needs of the current application;
- networking protocols and addressing;
- identity, authentication, permissions, encryption, and secure updates;
- scheduling, multitasking, background work, and isolation;
- filesystems, backup, migration, and export;
- reliability, safe mode, recovery, diagnostics, and logging beyond prototype needs;
- build system, dependency policy, ABI stability, and release governance;
- browser simulation features beyond those required to test the active milestone;
- bare-metal ownership of SBC or x86 hardware; and
- sound, GPIO, sensors, remote rendering, and other optional capabilities.

When one of these becomes necessary, begin with the concrete use case and constraints, consult [SilOS_DESIGN_BACKLOG.md](SilOS_DESIGN_BACKLOG.md) for earlier thinking, and then decide afresh.

## 8. Planning rule

Keep this file short. It should describe what SilOS is, what has genuinely been decided, what is being proved now, and what comes next. Do not add broad recommendations for inactive areas.
