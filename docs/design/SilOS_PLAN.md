# SilOS Plan

This is the authoritative plan for SilOS. It deliberately records only:

- principles and requirements we have agreed;
- the current milestone and the questions blocking it; and
- a short list of areas to explore later.

Ideas are not commitments. Detailed alternatives and earlier recommendations are preserved in [SilOS_DESIGN_BACKLOG.md](SilOS_DESIGN_BACKLOG.md) and should be reconsidered only when relevant work begins.

Project-level code ownership and platform boundaries are recorded in the
[SilOS code-layout discussion](Discussion-Code-Layout.md). Supporting design
material is kept separate: [UiRefs and templates](Discussion-VariableBindingAndTemplates.md),
the adopted [Shell UI specification](Spec-ShellUI.md), supporting
[Shell UI discussion](Discussion-Shell-UI.md), the active
[Shell UI implementation plan](ImplementationPlan-ShellUI.md), and
[storage and MQTT references](Discussion-QueueMetaphors.md). The implemented
[zero-copy UI refactor](RefactorPlan-UI-Rendering-Pipeline.md) records its
supporting implementation plan. The Store class and task refactor is recorded
in [RefactorPlan-UI-OOP-Store.md](RefactorPlan-UI-OOP-Store.md). The completed
[FreeWisp plan](../../experiments/freertos-ulisp-browser/plan.md) supplies
Browser-substrate evidence.

## Contents

1. [Purpose](#1-purpose)
2. [Governing principles](#2-governing-principles)
3. [Committed foundations](#3-committed-foundations)
4. [Current milestone: implement the Browser Canvas Shell UI](#4-current-milestone-implement-the-browser-canvas-shell-ui)
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

### Implementation languages and runtime

- The foundational runtime and platform substrate will use a deliberately controlled subset of C++.
- uLisp is the SilOS application and portable-system language. The promoted implementation starts from uLisp ESP 4.9a; its version may be upgraded without reopening the language choice.
- FreeRTOS is the task, queue, and timing substrate. The promoted implementation starts from FreeRTOS-Kernel V11.3.0, with target-specific ports behind the agreed platform boundaries.
- As much portable system behaviour as practical—including applications, UI composition, shell behaviour, configuration, and services—will be written in the SilOS language.
- C++ is reserved for the runtime, platform adaptation, primitive operations, and work whose hardware, timing, memory, or bootstrapping constraints require it.
- Public SilOS-specific uLisp names must carry a recognisable prefix.
  Project-wide states, kinds, and types use `silos-`; subsystem-specific
  operations and values use their subsystem prefix. Accessors omit `ref` when
  their subsystem and argument already make the Ref role clear.

### Adopted implementation architecture

The Browser to-do prototype has been accepted as the production baseline and
promoted to [`src/`](../../src/). The following general approach is locked:

- one uLisp-owning task evaluates Lisp and owns its workspace, garbage collection, live references, and callbacks;
- FreeRTOS queues carry bounded pointer-free messages between task owners;
- uLisp applications are discovered and loaded from named stores through Shell-managed manifests and lifecycle events;
- the runtime sizes its application catalogue from discovered manifests, with
  app and UI-resource counts limited by available memory rather than global
  compile-time capacities;
- a dedicated UI task samples each app at a platform-configured cadence, locks
  the shared uLisp workspace for one app at a time, and streams canonical uLisp
  declarations and values to a platform-owned renderer;
- Lisp source stores preserve one editable source line per stable-ID Store row. The initial implementation uses dynamically sized linked rows and string values so line count and length are limited by available memory rather than arbitrary per-store constants; that backing remains hidden behind the Store interface for later replacement with measured, target-appropriate block allocation;
- StoreRefs and UiRefs provide stable language-visible live references, while reusable flow templates describe presentation without a conventional application-owned UI tree;
- every named store has shared BoundStore coordination across applications,
  while individual store-bind calls retain independent StoreRefs, windows,
  fields, and watches behind platform-owned storage interfaces;
- platform drawing is separated from Store, UI, Shell, and Runtime policy;
  portable UI deliberately uses allocation-free uLisp navigation helpers
  because uLisp objects are its canonical in-memory representation;
- Shell interaction, focus, screen composition, and Single App/Multi App layout
  follow the adopted [Shell UI specification](Spec-ShellUI.md);
- Input/Output is a top-level capability alongside Shell, UI, Store, and
  Runtime. Its future design will support bidirectional variable binding and
  configurable routing of bounded semantic events to either Shell or apps;
  the active Shell UI milestone defines only the semantic input seam; and
- the Browser target uses Emscripten WebAssembly, cooperative fibers, preloaded startup stores, and a narrow Browser surface adapter.

Public operations, record representations, capacities, rendering strategy, and
platform implementations may be refined as evidence accumulates. Alternative
languages, schedulers, application/UI architectures, or a return to the older
candidate approaches are not active options. Replacing one of these committed
foundations requires an explicit project-level decision and plan update.

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

## 4. Current milestone: implement the Browser Canvas Shell UI

Implement the adopted [Shell UI specification](Spec-ShellUI.md) in `src/`
using the class-based architecture and sequence in the
[Shell UI implementation plan](ImplementationPlan-ShellUI.md). The Browser is
the first target, but its display adapter must draw through a deliberately
small Canvas vocabulary rather than use HTML elements for Shell layout. This
keeps the physical rendering model close to constrained display targets.

This milestone is complete when:

1. portable modules own semantic interaction, hierarchy, focus, relative
   layout, and navigation for the full specification;
2. the Browser Canvas adapter renders the Nested Frame Shell, replacement
   pages, Single App and Multi App layouts, app menus, movement, resizing, and
   data entry without pop-over windows or DOM-based layout;
3. bounded semantic input drives both Shell interaction and app-directed events
   without exposing physical input details to portable modules;
4. the existing zero-copy uLisp template/value path and task-ownership rules
   remain intact; and
5. conformance tests pass and frame time, input-to-display latency, stack use,
   and steady-state heap use are recorded.

ESP32 display and physical I/O integration are deliberately the next
cross-platform validation, not a completion condition for this Browser
milestone. The completed Browser prototype remains behavioural evidence; its
experiment code is not the production implementation.

## 5. Questions to answer during this milestone

These are the only active design questions. They refine the adopted approach;
they do not reopen its foundations.

1. Which bounded semantic input representation covers navigation and editing
   without leaking Browser or future GPIO concepts into portable Shell code?
2. Which relative display capabilities are sufficient for common layout while
   leaving pixel geometry, fonts, clipping, and drawing to each target?
3. Which UI declaration or field metadata additions are required for editable
   values and actions?
4. What measured frame cadence and resource use are acceptable for the Browser
   implementation and plausible for the reference MCU?

Record answers here only when evidence supports a project-level decision.
Record implementation detail in source documentation rather than expanding
this plan.

## 6. Later milestones

Plan each milestone only when the preceding prototype supplies enough evidence.

1. Complete the to-do CRUD and persistence foundation and validate the shared
   Shell UI architecture on ESP32.
2. Refine the to-do application and interaction model.
3. Add alarm and shared time/notification services.
4. Add calendar and structured date-based UI.
5. Add chat and the minimum networking service it requires.
6. Validate and refine the SBC and x86 targets.
7. Consider additional capabilities only against concrete use cases.

## 7. Topics deliberately deferred

The following areas matter, but do not need detailed recommendations yet:

- remaining visual details, exact fonts, accessibility, touch, and mouse behaviour
  beyond the adopted Shell UI specification;
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
- detailed Input/Output design beyond the adopted top-level direction,
  including GPIO discovery, output scheduling, and sensors;
- sound, remote rendering, and other optional capabilities.

When one of these becomes necessary, begin with the concrete use case and constraints, consult [SilOS_DESIGN_BACKLOG.md](SilOS_DESIGN_BACKLOG.md) for earlier thinking, and then decide afresh.

## 8. Planning rule

Keep this file short. It should describe what SilOS is, what has genuinely been decided, what is being proved now, and what comes next. Do not add broad recommendations for inactive areas.
