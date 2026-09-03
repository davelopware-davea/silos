# Prototype plan: Shell UI

**Status:** validated prototype record. The adopted behavior has been promoted
to [`Spec-ShellUI.md`](Spec-ShellUI.md). This document remains historical
prototype evidence rather than an implementation specification.

## Question

How should a character-led, monochrome SilOS Shell expose the same hierarchy
and navigation grammar across a 128×64 display, larger embedded screens, desktop,
and Browser targets while supporting both Single App and Multi App modes?

## Prototype

Create a dependency-free Browser experiment under
`experiments/shell-ui-prototype/`. It uses realistic To-do and Status content,
keeps all changes in memory, and does not change the production renderer or
public APIs.

The interaction hierarchy is:

```text
SilOS -> App -> Mounted Template -> Row (for lists) -> Field -> Data Entry
```

- `Enter` descends or activates, `Back` ascends, and arrow keys select a
  rendered peer at the current level.
- Template fields flow left-to-right and wrap top-to-bottom. Shell, app,
  template, row, and field boundaries are visually demarcated.
- Single App and tiled Multi App modes are selected at the SilOS level when the
  display supports both. The 128×64 profile is Single App only.
- At app-level focus, a double `Enter` opens that app's bottom-row menu. It
  offers `Move`, `Resize`, and `Settings`; `Enter` opens the selected option.
  App regions do not overlap: movement reflows a column-major tile layout and
  resizing adjusts column width or row height. App Settings replaces the app workspace with a page owned by
  the focused app. `Back` returns first to the app menu and then to app focus.
- Overflow is clipped to an app region and follows the focused component.
- Data Entry occupies a dedicated bottom strip. Editing updates every visible
  occurrence of the in-memory value immediately; `Enter` or `Back` leaves the
  editor without undoing the live changes.
- App start/stop, settings, mode switching, calculator input, and power
  confirmation are simulated in memory. The To-do, Status, and Calculator apps
  provide enough regions to exercise mixed horizontal and vertical tiling.

## Visual direction

The selected **Nested Frame** design expresses application hierarchy through
labelled nested components. Its SilOS menu occupies a scrollable left rail and
the dedicated bottom row spans the full screen. The screen and workspace have
no non-semantic outer border. A prototype-only control changes display profile
while displaying the complete focus and layout state.

## Evaluation

Evaluate the Nested Frame design at 128×64, 480×320, 1024×768, and a responsive Browser
viewport.

- Traverse every hierarchy level with arrows, `Enter`, and `Back`/`Escape`.
- Switch apps and modes while retaining valid focus.
- Move and resize apps without overlap or inaccessible regions.
- Exercise wrapping, clipping, scrolling, stopped apps, and empty lists.
- Edit a field and confirm immediate updates in every occurrence.
- Confirm focus remains clear in monochrome.

Record evaluation findings and their rationale here. Capture the full
throwaway prototype on a separate branch when its question has been answered;
only the validated design should continue into production.

## Findings so far

- **Nested Frame** is the preferred baseline.
- Its system-control rail belongs on the left. It scrolls within the workspace
  row and never overlays the dedicated bottom row.
- There is no top-level Apps action. At the two highest interaction levels,
  `Back` toggles directly between SilOS controls and app-level focus.
- Layout is not a SilOS-level action. Double-pressing `Enter` at app focus opens
  a bottom-row app menu containing Move, Resize, and Settings.
- App Settings uses the same workspace-replacement rule as SilOS pages and is
  titled for the focused app. The prototype supplies Date Format as an example.
- During Move or Resize, either `Enter` or `Back` finishes the operation and
  returns to the bottom-row app menu; another `Back` returns to app focus.
- Multi App layout is always two-dimensional; there is no global tile-axis
  setting. Apps are grouped into left-to-right columns and ordered top-to-bottom
  within each column.
- In Move, `Up` and `Down` reorder an app within its column. Crossing the top or
  bottom transfers it to the corresponding edge of the adjacent column, or
  creates a new outer column when none exists. `Left` and `Right` transfer it
  directly to the adjacent column at the nearest row, likewise creating an
  outer column when necessary. Empty columns collapse.
- In Resize, `Left` and `Right` change the containing column's width; `Up` and
  `Down` change the app's relative height within that column. Direction always
  means narrower, wider, shorter, or taller respectively, independent of the
  column or row position. An app alone in its column always fills the column's
  height, and vertical resize is a no-op there. Its prior relative height is
  retained and becomes effective again if it rejoins a multi-app column.
- At Multi App focus, `Up` and `Down` move only to the immediately adjacent app
  in the same column and stop at its ends. `Left` and `Right` move only to an
  adjacent column and stop at the first or last column. The destination is the
  lowest app in that column whose top is at or above the previous app's top.
- App-level focus uses the normal focus border. Opening the bottom-row app menu
  changes its owning app to a distinct double border, which remains unchanged
  throughout Move or Resize so the app context is visually stable.
- App title bars show only the app name; a `RUNNING` label is redundant for an
  app that is already visible.
- A SilOS setting controls whether the left menu automatically hides in app
  interaction. When disabled, the menu remains visible on non-tiny displays.
  On the 128×64 profile, it always hides whenever focus is not exactly at the
  SilOS level.
- SilOS does not use pop-over windows. Entering a menu destination such as Run,
  Settings, or Power replaces the app workspace region. `Back`
  returns to SilOS-level menu focus and restores the apps in that region.
- The physical screen edge and the right-hand workspace are not boxed. Borders
  are reserved for meaningful components such as menus, apps, templates, rows,
  fields, and system pages.
