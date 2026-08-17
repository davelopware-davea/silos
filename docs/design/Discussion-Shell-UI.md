# Discussion: Shell UI

Conversation begun on 17 August 2026.

## Purpose

This note records an exploratory discussion of the SilOS Shell UI, including
how it receives, normalises, routes, and applies user input. It is discussion
material, not an agreed design or a replacement for the authoritative
[SilOS plan](SilOS_PLAN.md).

**Related:** [Variable Binding and Templates](Discussion-VariableBindingAndTemplates.md)
defines UiRefs and templates; [Queue Metaphors](Discussion-QueueMetaphors.md)
describes the StoreRefs that may supply a UiRef's value.

## Contents

1. [Starting question](#starting-question)
2. [Scope for the discussion](#scope-for-the-discussion)
3. [Proposed Shell UI model](#proposed-shell-ui-model)
4. [Tiny mode](#tiny-mode)
5. [Layout mode](#layout-mode)
6. [Bottom-line editing](#bottom-line-editing)
7. [External changes during editing](#external-changes-during-editing)

## Starting question

What is the smallest Shell UI model that supports keyboard and directional
controls on every initial target, while allowing applications to create and
edit the first to-do items without taking ownership of platform events or
display details?

## Scope for the discussion

- normalising physical input into a small set of portable semantic events;
- focus, navigation, activation, cancellation, and text entry;
- ownership and queueing between platform, Shell, and uLisp tasks;
- application-facing input and editing primitives; and
- the bounded memory, latency, and failure behaviour needed for the MCU.

Touch, pointer input, shortcuts, accessibility extensions, and sophisticated
text editing may inform the design, but are not initial commitments unless they
are needed to keep the core model coherent.

## Proposed Shell UI model

An app supplies a compact descriptor of its default presentation requirements,
including its name and ideal and minimum width and height. The Shell chooses
which apps are visible and where they appear. A small display may show only one
app, while a larger display may show several. The user controls that Shell
arrangement; being visible does not itself give an app input ownership.

An app's output is described by templates. Template occurrences provide an
ordered traversal of their interactive elements. The Shell navigates that order
and skips static output. A focus target identifies a rendered occurrence, not
only a bound value, because one value may appear in more than one template or
in several rendered list rows. The Shell always renders a clear visible focus
indicator for the current target.

The portable input vocabulary begins with three categories of input:

- four-direction navigation;
- a primary action pair: `Activate`, meaning do or enter the focused thing,
  and `Back`, meaning leave or return; and
- data entry.

The user-facing name for the leave/return action is the **Back button**. It
moves up one interaction level: for example, from a component to its app, then
from an app to app navigation, and finally to the Shell menu. Physical keys,
GPIO buttons, and future pointer input are normalised below this model.

Focus moves through nested interaction levels. At app-level focus, `Activate`
enters the app's component traversal. At component level, it invokes a button,
enters an editable field, or enters a list. A list then exposes its items, and
an item can in turn expose its components. `Back` reverses that path one level
at a time, eventually returning to app-level focus and then to the Shell menu.

## Tiny mode

When a display cannot accommodate a useful multi-app layout, the Shell enters a
distinct **tiny mode**, rather than treating it as a restricted normal layout.
Tiny mode shows exactly one app at a time. At app-level focus, navigation
switches to the next or previous app in the user's app sequence and immediately
shows that app. The Shell should define a consistent mapping of the four
directions to next and previous in this one-dimensional mode.

An app may provide a tiny-screen template as well as its normal template. The
Shell selects the requested presentation mode, while the app supplies the
cut-down template appropriate to it. This is the preferred way to make an app
fit the display, because it allows the app to omit or reorganise secondary
information while retaining the normal template's UiRefs and interaction model.

If an app has no suitable tiny template, or its tiny template is still larger
than the viewport, the Shell offers **Scroll** in place of normal-layout
**Resize**. Scroll mode pans the clipped tiny-mode viewport over the app using
navigation input; it is a fallback for otherwise inaccessible content, not the
desired primary experience. The Shell renders the app at virtual coordinates
and clips it to the viewport, rather than requiring an off-screen framebuffer.

Component-level focus and interaction remain within the one visible app.
Returning with the Back button restores app-level focus, from which navigation
again switches apps. The user's app ordering and scroll position should persist
as Shell layout preferences rather than as application state. The Shell selects
tiny mode from the display profile and available viewport, rather than requiring
each app to infer the physical screen.

Tiny-mode layout controls substitute **Scroll** for **Resize**. Move and
Maximise have no useful multi-app layout to manipulate in this mode and need not
be offered; Close remains subject to the same later lifecycle decision.

## Layout mode

At app-level focus, a Shell-recognised double `Activate` gesture can enter
layout mode for the focused app. This gesture is not delivered to the app. The
Shell overlays its own controls at the top of that app, initially **Resize**,
**Move**, **Maximise** (or **Normalise** when already maximised), and
potentially **Close**.

```text
app-level focus
  -> double Activate -> app layout mode
  -> Activate Resize / Move -> resize or move mode
  -> Back -> app layout mode
  -> Back -> app-level focus on the same app
```

In resize mode, navigation moves the app's bottom-right edge within Shell-set
limits; its top-left edge remains fixed. In move mode, navigation shifts the
whole app within those limits. Neither operation creates overlapping apps.
Instead, it changes the selected app's requested allocation and the Shell
reflows the whole layout, respecting every app's minimum size and balancing
their ideal sizes where possible. The layout algorithm should be deterministic,
with the manipulated app's requested edge or position taking priority, so that
the result remains understandable.

**Maximise** gives the selected app the whole display and retains the prior
layout allocation. It becomes **Normalise** while maximised; selecting it
restores the retained allocation and reflows the other apps. Back accepts the
current placement and moves one level up; it does not restore the pre-operation
placement. Layout commands and placements are Shell-owned and persist per
display profile.

`Close` needs a later lifecycle decision: it might hide an app from the current
layout, stop it, or both. It should not be assumed to discard application data,
and likely needs confirmation.

## Bottom-line editing

The Shell owns a single input line at the bottom of the display. Activating an
editable template field enters an edit session for that bound value. The input
line becomes active and updates the value as the user types; the normal UiRef
and rendering path then updates every visible occurrence of that value.

The edit session is transient Shell state. It records the targeted UiRef and
rendered occurrence, plus ephemeral cursor and scroll state. While it is
active, the app-visible **UiRef** created by `defui` has observable status
`editing`. Templates merely refer to that UiRef and do not own its state.
Application code can watch the UiRef and, when its status changes away from
`editing`, perform post-edit work using the UiRef's then-current value. A
watch must receive the old and new statuses, so an application can distinguish
ordinary completion from a stale conflict. The Back button ends the session and
restores normal navigation with focus on the field that initiated it. It does
not undo already applied edits. A future explicit cancel operation would
require retaining the original value for restoration.

The initial direct model is most natural for string values. Number, date, and
other typed fields need a later rule for incomplete textual input, validation,
and conversion.

## External changes during editing

If the edited UiRef's value changes externally while an edit session is
active, its status changes from `editing` to `stale`. The input line freezes
rather than silently overwriting or replacing the user's entry. Its right-hand
end reports that the data is stale and offers:

- **Overwrite**, applying the user's current entry over the newer value; or
- **Discard**, abandoning the entry and accepting the newer value.

The Shell needs a revision or equivalent change identity for this comparison,
and must distinguish its own per-keystroke writes from an external change. If
a UiRef is backed by a StoreRef or StoreRowRef, its UI status remains
distinct from storage progress such as `saving` or `error`. The storage-write
policy during an edit and the exact control sequence for choosing these two
actions remain open.
