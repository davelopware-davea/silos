# SilOS Shell UI prototype

This is a throwaway, in-memory prototype of the selected Nested Frame Shell.
It is not production UI code.

Run:

```sh
experiments/shell-ui-prototype/run.sh
```

Then open <http://localhost:8765/?profile=browser>.

- Arrow keys move focus at the current hierarchy level.
- `Enter` activates or descends; `Escape` or `Backspace` goes back.
- At SilOS or app-level focus, `Escape` toggles directly between those two
  levels; there is no Apps item in the system menu.
- At app-level focus, press `Enter` twice within 360 ms to open that app's
  bottom-row menu. A single `Enter` enters the app normally.
- The app menu offers Move, Resize, and Settings. In Move, `Up`/`Down` reorder
  within a column and cross into the adjacent column at an edge; `Left`/`Right`
  transfer directly between columns. A new outer column is created when there
  is no neighbour. In Resize, `Left`/`Right` change column width and `Up`/`Down`
  change row height. `Enter` or `Escape` returns to the app menu.
- A sole app always fills its column vertically. `Up`/`Down` resizing does
  nothing in that state and does not alter the height remembered from its last
  multi-app column.
- App focus has a normal focus border. Opening its bottom-row menu changes that
  app to a distinct double border, which remains the same during Move or Resize.
- To-do, Status, and a working four-operation Calculator make three-app,
  two-column, and one-column layouts available to try. Multi App always tiles
  in both dimensions, so there is no tile-axis setting.
- At Multi App focus, `Up`/`Down` select only neighbours in the same column.
  `Left`/`Right` select the adjacent column's lowest app whose top is at or above
  the previous app's top. Navigation does not wrap at row or column edges.
- App Settings replaces the apps with a page titled for the focused app. The
  prototype Date Format setting cycles when activated. `Escape` returns to the
  app menu, and another `Escape` returns to app focus.
- The floating control selects a display profile.
- The Settings menu includes `Auto-hide SilOS Menu`. When enabled, the menu is
  hidden throughout app interaction. At 128×64 it always hides outside exact
  SilOS-level focus, regardless of the setting.
- Run, Settings, and Power are full Shell pages that replace the app workspace;
  the prototype has no pop-over windows. Going back restores the apps.
- The prototype state is shown in the floating bar.

See [`../../docs/design/PrototypePlan-Shell-UI.md`](../../docs/design/PrototypePlan-Shell-UI.md)
for the design question and evaluation checklist.
