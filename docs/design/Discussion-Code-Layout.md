# Discussion: SilOS Code Layout

Decision agreed on 23 August 2026.

## Purpose

This note records project-level guidance for separating portable SilOS
behaviour, dependency adapters, platform implementations, and third-party
integration. The Browser to-do prototype prompted the decision, but the
ownership and dependency rules apply to SilOS code generally.

The layout should make it possible to answer three questions from a path
alone: which SilOS capability owns a piece of code, whether it adapts an
external dependency, and whether it is specific to an execution target.

The project subsequently adopted the Browser prototype's FreeRTOS/uLisp
architecture and promoted it to [`src/`](../../src/). Their adapters follow
the same ownership rules as any other dependency adapter. Reconsidering either
dependency requires an explicit project-level decision in the
[SilOS plan](SilOS_PLAN.md); it is no longer an open question in this note.

## Module ownership

SilOS-owned code is organised first by the capability which owns its
behaviour. Representative modules are:

```text
SilOS/
|-- Runtime/
|-- Store/
|-- UI/
|-- Shell/
|-- uLisp/
|-- FreeRTOS/
`-- Platform/
    `-- <Target>/
```

`Store`, `UI`, and `Shell` own SilOS behaviour for those capabilities.
`Runtime` owns portable composition and lifecycle behaviour
which does not naturally belong to one of them.

uLisp supplies the canonical in-memory representation used by applications and
the UI. Allocation-free navigation helpers localise representation knowledge,
but borrowed uLisp objects intentionally flow through UI traversal and the
platform render seam. FreeRTOS maps scheduling, task, queue, mutex, and
completion needs. Neither dependency owns SilOS policy.

The UI pipeline therefore uses ordinary compiled `.h`/`.cpp` modules under
`SilOS/UI`. Only the minimal built-in forwarding, root integration, and private
uLisp object-access implementations remain as fragments under `SilOS/uLisp`.
An `.inc` file is an integration constraint of the monolithic uLisp translation
unit, not the home of a SilOS class or UI policy.

## Platform implementations

`Platform/<Target>` owns code that depends on a particular execution target.
Target entry points, target SDK calls, display projection, persistence
facilities, and target-specific port support belong there. Reusable
composition belongs in `Runtime`; an entry point should not remain in a shared
location merely to make it appear portable.

Do not create empty directories for prospective targets. Add a target
directory when implementation for that target begins. Target names should
describe execution targets consistently. For example, a desktop simulator may
be better named `Native` or `Host` than `x86`, which names a processor
architecture.

## Module-owned platform interfaces

A portable module defines the narrow capabilities it requires a platform to
implement. These interfaces live beside the consuming module. When a header's
primary declaration is an interface class, name the file after that class so
the seam is directly discoverable; capability-only headers may use the
`Platform<Capability>.h` convention. Examples are:

```text
SilOS/UI/IPlatformRenderEngine.h
SilOS/Store/IPlatformStorageEngine.h
SilOS/Store/IPlatformStore.h
SilOS/Shell/PlatformEventTransport.h
```

A target implementation lives under its platform directory and is named
`<Target><Capability>`, for example:

```text
SilOS/Platform/Browser/BrowserSurface.h
SilOS/Platform/Browser/BrowserSurface.cpp
SilOS/Platform/Browser/BrowserStorageEngine.h
SilOS/Platform/Browser/BrowserStorageEngine.cpp
```

An interface is owned by the module whose needs define it, not by the target
which happens to implement it. Use the `Platform` prefix only for a capability
which targets genuinely implement differently; ordinary module interfaces do
not acquire the prefix simply because their implementation ultimately runs on
a platform. Avoid a general `Platform.h` or `PlatformServices` interface which
accumulates unrelated capabilities.

Platform interface declarations must not expose target SDK handles, browser
DOM objects, Emscripten types, device handles, RTOS queue types, or other
platform/dependency details. Target implementations are supplied during
runtime composition; portable modules do not construct or select them.

## Dependency direction

Dependencies point toward portable SilOS behaviour and the narrow interfaces
it owns:

```text
Platform/Browser/BrowserSurface
        implements
             |
             v
       UI/PlatformSurface <--- UI renderer

uLisp object model --> UI traversal / platform render seam
RTOS adapter -------> Store / UI / Shell / Runtime
```

A target implementation may depend on a module-owned platform interface; the
portable module must not depend on a particular target implementation. The
same rule applies to persistence, transport, clocks, input, and other platform
capabilities. Portable modules should remain independently testable without
compiling target-specific code.

An external dependency is not automatically a platform. More than one target
may reuse a language runtime or RTOS adapter. Target-specific port code belongs
under `Platform/<Target>`, while reusable dependency adaptation remains in the
dependency adapter module.

## Third-party integration

Third-party source trees are not assumed to be immutable. When SilOS needs
access to an external dependency's internal extension points, make the
smallest explicit change in that third-party tree and keep the substantive
SilOS implementation in SilOS-owned modules. Possible seams include
registration entries, memory-management roots, and the minimum declarations
or include points required by an adapter. Do not add a third-party hook merely
for symmetry when the dependency's configuration and public interfaces are
already sufficient.

Before adapting a vendored source snapshot, retain an identifiable unmodified
baseline in version control. The baseline is the comparison point which makes
later direct modifications reviewable; it is not a prohibition on modifying
vendor code.

This arrangement does not guarantee effortless upstream updates. An upstream
change may conflict with a hook, just as it may invalidate a source-generation
anchor. A normal three-way reconciliation against the recorded baseline makes
that coupling explicit and reviewable rather than concealing it in build-time
text rewriting.

## Build-system boundary

Build files contain normal build composition: targets, source lists, include
paths, compiler and linker options, generated assets where genuinely required,
platform packaging configuration, and tests. They must not contain C++
function bodies, language built-ins, runtime state, or memory-management logic
as string literals.

If a generated adapter is genuinely necessary for a monolithic dependency,
its SilOS source fragments belong in ordinary source or include files under
the relevant SilOS adapter module; a small build rule may combine them. Prefer
a small, explicit third-party hook when that produces a clearer diff and
dependency boundary.

## Architectural checks

The layout is working as intended when:

- direct uses of a target SDK and target-only startup, display, input, and
  persistence facilities are confined to that target's platform directory;
- Store, UI, Shell, and Runtime policy remains outside language and RTOS
  adapters;
- removing a target directory removes target-specific behaviour without
  removing the portable SilOS modules;
- third-party diffs contain integration seams rather than substantive SilOS
  implementations; and
- build files explain how the pieces are built without being one of the places
  where runtime behaviour is implemented.
