# Specification: SilOS Shell UI

**Status:** adopted interaction and layout specification, derived from the
Shell UI prototype.

This document defines the Shell UI behavior to implement across SilOS targets.
It is the source of truth for Shell hierarchy, screen regions, focus,
navigation, app layout, menus, and data entry. Project scope and milestone
priority remain governed by [SilOS_PLAN.md](SilOS_PLAN.md). Public uLisp UI
interfaces remain governed by the applicable API specifications. Class
ownership, platform seams, and delivery order are defined by the active
[Shell UI implementation plan](ImplementationPlan-ShellUI.md).

Normative terms **must**, **should**, and **may** describe required, preferred,
and optional behavior respectively.

## 1. Goals

The Shell UI must provide one interaction model across the 128×64 MCU display,
larger embedded displays, desktop, and Browser targets. It must:

- remain usable with four directional controls, `Enter`, `Back`, and text input;
- expose Shell, app, template, row, field, and editing state as one hierarchy;
- support both one-app and tiled multi-app presentation;
- keep focus and ownership visually unambiguous; and
- retain the character-led, monochrome or two-colour visual character validated
  by the Nested Frame prototype.

Touch, pointer input, shortcuts, exact fonts, animation, and a broader colour
system may be added later. They must preserve the semantic controls and state
transitions in this specification.

## 2. Terms and hierarchy

The interaction hierarchy is:

```text
SilOS
└── App
    └── Mounted App Template
        ├── Row (only when the template is a list)
        │   └── Field
        └── Field (for a non-list template)
            └── Data Entry (when editable)
```

- **SilOS level** owns system actions, global settings, app lifecycle, and power.
- **App level** selects one visible running app as the current app.
- **Mounted App Template level** selects a rendered template occurrence.
- **Row level** exists only for list templates.
- **Field level** selects one interactive or editable field occurrence. Static
  fields are rendered but skipped by focus navigation.
- **Data Entry level** edits the value bound to the selected field.
- **App menu** is a Shell-owned contextual level entered from App level.
- **Layout adjustment** is the Move or Resize sublevel of the App menu.

A focus identifier must identify a rendered occurrence, rather than only its
bound value, because a value may occur in multiple templates or rows.

## 3. Semantic input

Platform input must normalize to:

- `Up`, `Down`, `Left`, and `Right`;
- `Enter`;
- `Back`; and
- text-editing input while Data Entry is active.

Outside Data Entry, keyboard targets map `Enter` to Enter and both `Escape` and
`Backspace` to Back. Inside Data Entry, `Backspace` retains its normal text-edit
meaning and `Escape` maps to Back. Repeated keydown events must not count as
additional presses for the double-Enter gesture.

Except where a level defines a specific directional rule, arrows move focus to
a peer at the same hierarchy level in the requested screen direction. The
candidate nearest in that direction wins, with distance on the requested axis
taking priority over cross-axis distance. Navigation stays within the current
app/template/row context and does nothing when no candidate exists.

Whenever focus changes, clipped or scrollable content must scroll only enough
to reveal the new focus target.

## 4. Screen composition

The normal screen consists of three semantic regions:

```text
┌──────────────┬─────────────────────────────┐
│ SilOS menu   │ app workspace or page       │
├──────────────┴─────────────────────────────┤
│ bottom line                                │
└────────────────────────────────────────────┘
```

The diagram shows semantic regions, not a required outer border.

### 4.1 SilOS menu

The SilOS menu must:

- occupy the left side whenever visible;
- scroll within its own region when its contents overflow;
- end above the bottom line and never overlap it; and
- contain `Mode`, `Run`, `Settings`, and `Power` actions.

There is no top-level `Apps` action and no top-level layout action.

The `Auto-hide SilOS Menu` setting controls menu visibility on non-tiny
displays:

- off: the menu remains visible during app interaction;
- on: the menu hides during every app-owned or app-context interaction level;
- either value: the menu remains visible at SilOS level and on SilOS pages.

On a tiny display, the menu must be visible only at exact SilOS level. It must
hide at App level and every deeper or contextual app level regardless of the
setting.

### 4.2 Workspace and pages

The workspace renders visible apps in Single App or Multi App mode. A SilOS
page or app settings page replaces the workspace for the duration of that page.
Returning from the page restores the app workspace and its retained state.

SilOS uses no pop-over or modal windows. Run, Settings, Power, confirmations,
and app settings all use workspace replacement.

### 4.3 Bottom line

The bottom line is a dedicated final screen row and must never be covered by
the menu, apps, or pages. It displays exactly one of:

- the current hierarchy path plus concise key help;
- the app-menu selector;
- the active Move or Resize operation and its key help; or
- the Data Entry control.

Long paths and notices may truncate. The primary context and active operation
must remain identifiable at the target's supported size.

## 5. Visual language and focus

The UI must be character-led and work in monochrome. A second colour may be
used, but meaning must not depend on colour alone.

Borders demarcate semantic components: the SilOS menu, apps, mounted templates,
rows where useful, fields, pages, and the bottom line. The physical screen and
the workspace itself must not receive decorative outer boxes.

Focus has the following visual states:

- the exact focused component receives the normal focus border;
- ancestors on the active hierarchy path receive an ancestor indication;
- an app at App level receives the normal focus border;
- opening that app's App menu changes the app to a distinct contextual double
  border;
- the same contextual double border remains unchanged throughout Move and
  Resize; and
- leaving the App menu restores the App-level focus border.

Only the current app may receive the contextual border. App title bars show the
app name only; visible apps do not display a redundant `RUNNING` label.

## 6. Top-level behavior

At exact SilOS level, Back moves to App level and restores focus to the current
visible app. At exact App level, Back moves to SilOS level and restores the
previous SilOS-menu focus. Thus Back toggles directly between the two top
levels.

If no apps are running, attempting to enter App level must retain a usable
Shell route and direct the user to Run.

`Enter` on a SilOS action behaves as follows:

| Action | Result |
|---|---|
| Mode | Toggle Single App/Multi App when supported; remain Single App on tiny displays. |
| Run | Replace the workspace with the app start/stop page. |
| Settings | Replace the workspace with SilOS Settings. |
| Power | Replace the workspace with shutdown confirmation. |

Back from any SilOS page returns to SilOS-menu focus on the action that opened
the page and restores the app workspace.

The Run page lists every loaded app and its running state. Enter toggles the
focused app. Stopped apps do not render or participate in visible navigation.
Stopping the current app transfers current-app identity to another running app
when one exists.

SilOS Settings must include `Auto-hide SilOS Menu`. The selected monochrome
display polarity may also be changed there. Multi App layout is always
two-dimensional, so no tile-axis setting exists.

Power confirmation defaults to the non-destructive choice. Completing either
choice returns focus to the Power action; actual shutdown behavior is supplied
by the target platform.

## 7. Enter and Back within an app

At App level, Enter is interpreted as a single/double gesture:

- one Enter enters the app's first mounted template;
- two Enter presses inside the gesture window open that app's App menu; and
- navigation or Back cancels a pending single Enter.

The prototype uses a 360 ms gesture window. Production may tune this per input
platform, but the result must be deterministic and a held key must not trigger
the App menu.

The normal descent path is Template → Row, when present → Field → Data Entry,
or Template → Field → Data Entry for a non-list template. Enter on an action
field invokes it. Enter on a read-only field reports that it is read-only.

Back ascends one level and retains the relevant selection:

| From | Back returns to |
|---|---|
| Data Entry | the field that opened it |
| Field in a row | that row |
| Field in a non-list template | that template |
| Row | its template |
| Template | its app at App level |
| App menu | its app at App level |
| App settings | that app's App menu with Settings selected |
| Move or Resize | that app's App menu with the operation selected |

## 8. App menu

Double Enter at App level opens a bottom-line menu owned by the current app:

```text
<app name> APP MENU:  MOVE | RESIZE | SETTINGS
```

Left and Right cycle through the three choices; Up and Down do nothing. Enter
opens the selected choice. Back returns to App level.

Move and Resize require Multi App mode with at least two visible apps. When the
requirement is not met, the Shell remains in the App menu and reports why the
operation is unavailable. Settings remains available in either display mode.

Enter or Back while Move or Resize is active accepts the current layout and
returns to the App menu. Layout adjustment is live; there is no rollback on
exit.

## 9. Single App mode

Single App mode renders exactly one running app while other apps may continue
running. Directional navigation at App level switches the visible current app:

- Left and Up select the previous running app;
- Right and Down select the next running app; and
- selection wraps through the running-app sequence.

Displays that cannot provide a useful Multi App layout, including the reference
128×64 display, force Single App mode.

## 10. Multi App layout model

Multi App mode renders a two-dimensional, non-overlapping tiled layout. The
model is column-major:

- columns are ordered left-to-right;
- apps within a column are ordered top-to-bottom;
- each column has a relative width;
- each app tile remembers a relative height; and
- an empty column collapses immediately.

A stopped app is omitted from rendering. A column with one visible app renders
that app at full column height, regardless of its remembered relative height.

### 10.1 App-level navigation

App navigation observes hard edges and never changes layout:

- Up selects the immediately preceding visible app in the same column;
- Down selects the immediately following visible app in the same column;
- Up at the top and Down at the bottom do nothing;
- Left and Right select the adjacent visible column only;
- Left in the first column and Right in the last column do nothing; and
- on entering an adjacent column, focus selects the lowest app whose top edge
  is at or above the previous app's top edge.

The last rule is equivalent to selecting the app occupying the destination
column at the previous app's top coordinate. It must remain deterministic when
columns contain different row counts or relative heights.

### 10.2 Move

Move changes tile membership or ordering while keeping the current app focused:

- Up swaps with the app immediately above when one exists.
- Down swaps with the app immediately below when one exists.
- Up at the top transfers the app to the top of the column on the left; when no
  left column exists, it creates a new leftmost column for that app.
- Down at the bottom transfers the app to the bottom of the column on the right;
  when no right column exists, it creates a new rightmost column for that app.
- Left transfers the app directly into the column on the left at the nearest
  row position; when none exists, it creates a new leftmost column.
- Right transfers the app directly into the column on the right at the nearest
  row position; when none exists, it creates a new rightmost column.
- Attempting to create a new outer column when the app already occupies an
  outer column alone does nothing.

Moving a tile preserves its remembered relative height. A newly created column
uses the platform's default column width. Removing the final tile from a column
removes that column.

### 10.3 Resize

Resize meanings are invariant with tile position:

- Left makes the containing column narrower.
- Right makes the containing column wider.
- Up makes the current app shorter within its column.
- Down makes the current app taller within its column.

Width changes apply to the whole column. Height changes adjust the current
tile's relative height against the other visible apps in that column.

When the current app is the only visible app in its column, Up and Down are
strict no-ops: they must not change its remembered height. The app renders full
height while alone. If it later joins a multi-app column, its unchanged
remembered height becomes effective again.

The prototype uses 0.2-unit resize steps and clamps relative width and height to
0.4–3.0. Production must use fixed, predictable steps and enforce usable
minimum app dimensions; exact numeric units may be adapted to target geometry.

## 11. Mounted templates, rows, and fields

Every app renders one or more labelled mounted templates inside its app region.
Template content is clipped to the app and scrolls within that app when needed;
it never overlaps another app or the bottom line.

A non-list template renders a two-dimensional flow of fields, left-to-right and
wrapping top-to-bottom. A list template renders ordered rows, each containing
its own field flow. An empty list renders an explicit empty state and exposes no
row focus targets.

Static fields contribute presentation but cannot receive focus. Interactive
fields receive focus and Enter invokes their app-defined action. Editable fields
are visibly distinguishable and Enter opens Data Entry.

Arrow navigation between templates, rows, and fields remains at the current
hierarchy level and within the current parent. It follows the general spatial
selection rule in section 3.

## 12. Data Entry and live binding

Data Entry replaces the bottom-line content with an editor for the selected
field. It must use the platform's normal text-field navigation and editing
behavior.

Every edit is live:

- each accepted keystroke updates the application-visible bound value;
- every rendered occurrence of that value updates through the normal UiRef
  rendering path; and
- when the value is store-bound, the edit also enters the normal live storage
  path.

Enter or Back ends Data Entry and returns focus to the initiating field. Both
accept the current live value; Back is not undo. A future cancel operation would
require a separately specified retained-original-value mechanism.

Validation, incomplete typed values, concurrent external changes, and stale
edit resolution must follow the UI and storage API specifications when those
rules are finalized.

## 13. App settings

Choosing Settings from an App menu replaces the entire app workspace with a
page owned by that app. Its title must identify both the app and context, for
example:

```text
APP TODO - SETTINGS
```

The page uses the normal page and focus grammar. Back restores the app workspace
and returns to the same App menu with Settings selected. The prototype's Date
Format choice is an example app setting, not a mandatory setting for every app.

## 14. State ownership and persistence boundary

The Shell owns:

- current interaction level and rendered-occurrence focus;
- the current app and prior SilOS-menu focus;
- Single App/Multi App mode;
- SilOS-menu visibility and settings;
- app-menu selection and active layout operation; and
- column order, tile order, relative widths, and remembered tile heights.

Apps own their data, mounted templates, field actions, and app-specific
settings. Visibility and running state are distinct: an app may run without
being visible in Single App mode.

The prototype keeps all state in memory. Production persistence policy for
Shell layout and preferences is a separate implementation decision; the layout
model must be representable independently of rendered pixel coordinates so it
can be persisted per display profile later.

## 15. Prototype fixtures

The prototype uses three running apps to exercise the specification:

- To-do: list rows, editable fields, an empty-list state, and a summary template;
- Status: a compact read-only template; and
- Calculator: a readout plus a four-by-four grid of action fields.

Calculator arithmetic, the To-do empty/populated switch, the floating display
profile selector, and the full state readout are prototype diagnostics. They are
not production Shell menu items or Shell API requirements.

The prototype remains available at
[`../../experiments/shell-ui-prototype/index.html`](../../experiments/shell-ui-prototype/index.html)
as executable design evidence. Production code must implement this specification
rather than promote the prototype source directly.

## 16. Conformance criteria

A Shell UI implementation conforms when all of the following are demonstrated
on each applicable display class:

1. Every hierarchy level can be entered, spatially navigated, and left while
   retaining the specified focus.
2. Back toggles exact SilOS/App focus and ascends every deeper level correctly.
3. Single Enter enters an app and double Enter opens its bottom-line App menu.
4. The normal App-level border and contextual App-menu/Move/Resize border remain
   visually distinct in monochrome.
5. The SilOS menu follows auto-hide and tiny-display rules without covering the
   bottom line.
6. Shell pages and app settings replace and restore the workspace without a
   pop-over.
7. Multi App navigation respects columns, top/bottom edges, outer edges, and
   top-edge alignment without wrapping or changing layout.
8. Move can produce three columns, one column with three rows, and two columns
   with mixed row counts without overlap.
9. Resize direction remains invariant for every column and row position.
10. A sole app fills its column; vertical resize leaves remembered height
    unchanged; rejoining a multi-app column restores that height.
11. Focused overflow scrolls into view without escaping its app region.
12. Live Data Entry updates all visible bindings and returns focus to its field
    on both Enter and Back.
13. Starting and stopping apps always leaves a valid, recoverable focus state.
