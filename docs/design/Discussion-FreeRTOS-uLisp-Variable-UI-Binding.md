# Discussion: FreeRTOS, uLisp, and Variable-to-UI Binding

Conversation recorded on 8 August 2026.

## Initial question

**User**

Could I use FreeRTOS, uLisp and my own C/C++ library to enable Lisp code to interact in a very specific controlled way with a UI implemented in my C library?

Specifically, I want my C code to be able to map the memory storing specific Lisp variables directly into the UI so that, when the Lisp code changes a variable value, the C library will draw the updated value the next time it renders the UI.

It might be that the Lisp code needs to call my library to allocate the variable in the first place, but it should be able to modify it in a "normal" Lisp way after that.

**Assistant**

Yes. This fits SilOS's "live memory-to-display binding" idea closely.

The important distinction is: bind the UI to the Lisp variable's **binding cell**, not directly to its current numeric or string object.

In current uLisp, `setq` finds an environment pair and replaces its `cdr` with the newly evaluated object:

```cpp
object *pair = findvalue(variable, env);
object *new_value = eval(expression, env);
cdr(pair) = new_value;
```

So an `int*` pointing into the old value would become stale. The binding pair, however, continues to represent the variable while its `cdr` points to the latest value. This behaviour is visible in the [current uLisp ESP source](https://github.com/technoblogy/ulisp-esp/blob/master/ulisp-esp.ino#L2486-L2495).

I would expose something like:

```lisp
(defui temperature 20 :integer :widget 3)

(setq temperature 21)
(incf temperature)
```

`defui` would:

1. Create a normal global uLisp variable.
2. Locate its global environment binding pair.
3. Register that pair with a fixed-size C++ UI-binding table.
4. Record its permitted type, formatter and target widget.

Conceptually:

```cpp
struct UiBinding {
    object* binding_pair;
    UiValueType expected_type;
    WidgetId widget;
    bool dirty;
};
```

At render time:

```cpp
object* value = cdr(binding.binding_pair);
NativeUiValue snapshot = checked_convert(value, binding.expected_type);
draw(binding.widget, snapshot);
```

For change detection, either:

- inspect every registered binding each frame; simplest for a small bounded UI; or
- add a small write barrier to `setq`, `incf`, `setf`, etc. that marks the matching binding dirty.

Important constraints:

- Initially permit only scalars: integer, float, Boolean, character and perhaps immutable strings.
- Do not expose arbitrary addresses to Lisp.
- Prefer global UI variables; lexical variables can expire when their environment becomes unreachable.
- `makunbound`, `load-image`, and especially `save-image` need handling. uLisp's ordinary mark-and-sweep collection is non-moving, but `save-image` explicitly compacts the workspace and relocates objects, invalidating cached binding pointers. Re-resolve bindings by symbol afterward or teach the compactor to relocate UI handles. [The source shows both the rooted global environment and this compaction](https://github.com/technoblogy/ulisp-esp/blob/master/ulisp-esp.ino#L724-L787).
- Do not let one FreeRTOS task traverse the uLisp heap while another evaluates Lisp or performs garbage collection. Ideally one task owns both evaluation and snapshotting. Otherwise use a mutex, copy into a native UI snapshot, release the mutex, then render. FreeRTOS task notifications can efficiently wake a separate renderer if needed. [FreeRTOS documents these as lightweight task signalling](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/04-As-event-group).

So normal `(setq temperature ...)` syntax can work with a very small uLisp integration. For the prototype, binding to the global environment pair is considerably simpler than inventing externally allocated Lisp variables.

## Supporting strings

**User**

It's important that it supports strings. Would that be possible?

**Assistant**

Yes. Strings are entirely practical, including variable-length strings:

```lisp
(defui status "Starting..." :string :widget 3)

(setq status "Connected")
(setq status (format nil "Items: ~a" count))
```

The UI binding still points to the variable's binding pair. At render time, C++ obtains the current value and copies it into a native UI buffer:

```cpp
void snapshot_string(UiBinding& binding) {
    object* value = cdr(binding.binding_pair);

    if (!stringp(value)) {
        binding.set_type_error();
        return;
    }

    const size_t length = stringlength(value);
    const size_t copied = std::min(length, binding.capacity - 1);

    for (size_t i = 0; i < copied; ++i)
        binding.buffer[i] = nthchar(value, i);

    binding.buffer[copied] = '\0';
}
```

Copying is necessary with a conventional C string interface because uLisp strings are not contiguous C strings. They are stored as garbage-collected chains of packed character objects; uLisp itself accesses them using `stringlength()` and `nthchar()`. See the [current implementation](https://github.com/technoblogy/ulisp-esp/blob/master/ulisp-esp.ino#L1508-L1619).

The safe sequence is:

1. Lock or otherwise ensure exclusive access to uLisp.
2. Read `cdr(binding_pair)`.
3. Confirm it is currently a string.
4. Copy it into a C-owned snapshot buffer.
5. Release the lock.
6. Render from the snapshot.

The renderer must never retain pointers into the string itself after releasing the lock.

One additional detail is mutation. These both change what is displayed:

```lisp
(setq status "Ready")             ; replaces the string object
(setf (char status 0) #\X)        ; modifies the existing string
```

If bindings are inspected every frame, both work automatically. If dirty flags are used, both variable assignment operations such as `setq` and in-place string operations such as `(setf (char ...))` must mark the binding dirty.

For the first prototype:

- Poll and snapshot bound values during rendering.
- Impose a configurable maximum displayed length, perhaps 128 or 256 bytes.
- Truncate with an ellipsis when necessary.
- Initially treat strings as ASCII, matching standard uLisp strings.
- Resolve cached binding handles again after workspace save/load or compaction.

Strings do not require a special externally allocated Lisp variable. They require type checking and a short, synchronized copy into UI-owned memory.

## Avoiding persistent copies

**User**

One of the points is to reduce the duplication of the data as much as possible. Only copying it in the act of rendering is fair enough, but not having a copy of it kicking around in memory for any length of time.

Could this be simplified by adding special handling for these in the Lisp memory management or garbage collector rather than "coping" with the fact that they will move around?

**Assistant**

Yes. GC-aware handles plus streamed rendering are a cleaner fit for that goal than maintaining UI snapshots.

Add a small **external-handle table** to uLisp:

```cpp
struct UiBinding {
    object* binding_pair;   // (symbol . current-value)
    WidgetId widget;
    UiType expected_type;
};
```

Then integrate it with memory management:

```cpp
void gc(...) {
    markobject(GlobalEnv);

    for (UiBinding& binding : bindings)
        markobject(binding.binding_pair);

    // Mark remaining roots, then sweep.
}

void relocate(object* from, object* to) {
    for (UiBinding& binding : bindings)
        if (binding.binding_pair == from)
            binding.binding_pair = to;
}
```

The handle points to the **binding pair**, not the current string. Consequently:

```lisp
(setq status "Ready")
```

only changes `cdr(binding_pair)`. The UI handle remains valid and automatically sees the new string.

For rendering, stream characters directly from uLisp storage:

```cpp
void drawBoundString(const UiBinding& binding) {
    object* string = cdr(binding.binding_pair);

    if (!stringp(string)) {
        drawTypeError(binding.widget);
        return;
    }

    forEachLispCharacter(string, [&](char ch) {
        drawGlyph(binding.widget, ch);
    });
}
```

No persistent text buffer is required. uLisp already uses essentially this pattern: `printstring()` traverses the string's packed cells and sends each character to a callback. See [current uLisp string output](https://github.com/technoblogy/ulisp-esp/blob/master/ulisp-esp.ino#L6049-L6073).

Pinning strings should be avoided. A uLisp string consists of several linked workspace cells, so pinning would complicate compaction and cause fragmentation. Relocatable handles are simpler:

- Ordinary GC marks the binding and its current value.
- Workspace compaction updates the handle if its binding pair moves.
- The string's internal pointers are relocated by the existing compactor.
- `setq` can replace the entire string normally.
- `setf` can modify individual characters normally.
- Rendering traverses the authoritative Lisp value directly.

Two synchronization rules remain:

- GC, Lisp mutation and rendering must not access the heap concurrently. The simplest solution is for one FreeRTOS task to own all three operations.
- If separate tasks are necessary, the renderer holds the Lisp heap mutex while measuring and drawing the text. It must not retain any Lisp pointer afterward.

After `load-image`, rebuild the handle table by resolving each registered symbol again, because the entire environment has been replaced. During ordinary GC and `save-image` compaction, the relocation hook keeps handles current.

The desired model can therefore be genuinely direct:

```text
Lisp binding -> Lisp string cells -> glyph renderer
```

There need be no second character-string allocation at all--only transient character values while glyphs are emitted.

## Multiple templates and render locations

**User**

One more aspect to consider--although I suspect it won't affect this aspect of memory management--is that the idea will be that a variable *can* be something that is UI-rendered, equivalent to a simplified version of the `defui` in your examples.

But it might be rendered in more than one place, so the rendering format etc. will be defined by different Lisp code that articulates the shape of the UI. For example:

```typescript
let desc = defui('string');
let target = defui('datetime');
let status = defui('string');

const todo = defuitemplate('todo-item');
defuiaddstr(todo, desc, 16); // 16 characters wide, either packed or chopped
defuiadddate(todo, target, 'yyyy-mm-dd');
defuiaddstr(todo, status, 6);

const todoShort = defuitemplate('todo-item-short');
defuiaddstr(todoShort, desc, 8);
defuiadddate(todoShort, target, 'yymmdd');
```

Hopefully you can see that we might render those two templates in different places on the screen, but want both to be updated when `desc`, `target` or `status` are updated, with each template having its own formatting details.

**Assistant**

Yes. A UI-capable variable is the single authoritative value, while templates are independent views of it. This does not fundamentally change the GC approach, but it suggests one important refinement: **templates should reference a stable UI-value handle, rather than each holding a GC handle directly.**

```text
Lisp binding: desc
       |
       v
UiValue handle #7 -----+-- template "todo-item": width 16
                       +-- template "todo-item-short": width 8
                       +-- any additional render locations
```

A possible representation is:

```cpp
struct UiValue {
    object* binding_pair;       // relocated by GC
    UiType declared_type;
    uint32_t revision;
};

struct UiField {
    UiValueId value;            // stable table index + generation
    Formatter formatter;
    LayoutConstraints layout;
};

struct UiTemplate {
    Span<UiField> fields;
};

struct UiInstance {
    UiTemplateId template_id;
    Rect destination;
};
```

Only `UiValue` knows about the Lisp heap. Every template and rendered instance refers to it through the small `UiValueId`.

Equivalent Lisp could look like:

```lisp
(defui desc string "")
(defui target datetime nil)
(defui status string "")

(defuitemplate todo-item
  (str desc :width 16 :overflow packed-or-chopped)
  (date target :format "yyyy-mm-dd")
  (str status :width 6))

(defuitemplate todo-item-short
  (str desc :width 8 :overflow packed-or-chopped)
  (date target :format "yymmdd"))
```

Here `str` and `date` must treat `desc` and `target` as binding references--not evaluate them immediately and capture their current values. That could be implemented through special-form syntax, macros, or explicit syntax such as `(ui-ref 'desc)`.

When rendering either template:

1. Resolve its `UiValueId`.
2. Read the binding pair's current `cdr`.
3. Validate its declared type.
4. Apply that field's formatter.
5. Stream the resulting characters directly to the glyph renderer.

Thus `"Description"` is stored once. Width, truncation and date-format information belongs to each template, but none of those templates contains another copy of the value.

For updating, `setq` would increment the value's revision:

```cpp
cdr(uiValue.binding_pair) = newValue;
++uiValue.revision;
```

There are then two choices:

- Render all visible instances each frame.
- Let each rendered field remember the last revision it drew and invalidate itself when it differs.

The second adds only a revision number per rendered field, not a data copy. A subscription list is optional; scanning a small number of visible fields may be smaller.

GC behaviour remains straightforward:

- Each `UiValue` provides one GC root or relocatable handle.
- Ten templates referencing it still produce only one GC handle.
- Removing the last template does not necessarily destroy the value; it remains a normal Lisp variable.
- Removing the `defui` binding invalidates its `UiValueId`, with the generation preventing stale references.
- String and date formatting happens during rendering without retaining formatted copies.

This separation--**value, template, instance**--looks like the right minimal model.

## List templates and item contexts

**User**

The next layer of complexity would be that I want to be able to use a template-type thing as the item rendered for a list. There would be a list template that could include a limitation on the number of items to show on the screen, and the item renderer.

That item renderer would not be passed explicit values from variables, but would be applied to each item in the list. In declaring the template, we therefore need a way to refer to individual fields we expect to find in each list item. Does that make sense?

**Assistant**

Yes. The item template needs an implicit **item context**. Its field references are paths relative to "the current list item," rather than references to global UI variables.

For example:

```lisp
(defuitype todo-item
  (desc string)
  (target datetime)
  (status string))

(defui todos (list-of todo-item) '())

(defuitemplate todo-row (item todo-item)
  (str (field item desc) :width 16 :overflow chop)
  (date (field item target) :format "yyyy-mm-dd")
  (str (field item status) :width 6))

(defuilist todo-list
  :source todos
  :item-template todo-row
  :limit 5)
```

Here, `item` is not a stored variable. It is a template parameter meaning "whichever item is currently being rendered."

Conceptually, rendering becomes:

```cpp
void renderList(const UiListTemplate& view) {
    object* list = resolveUiValue(view.source);

    unsigned count = 0;
    for (object* node = list;
         node != nullptr && count < view.limit;
         node = cdr(node), ++count) {

        object* item = car(node);
        renderTemplate(view.itemTemplate, ItemContext{item});
    }
}
```

A field inside the row template contains metadata such as:

```cpp
struct UiItemField {
    FieldId field;          // e.g. TodoDescription
    Formatter formatter;
    LayoutConstraints layout;
};
```

During rendering, `(field item desc)` is compiled or resolved to something like:

```cpp
object* value = getField(context.item, TodoDescription);
```

The formatter then streams that value directly to the display. No row object and no formatted string needs to be retained.

For constrained hardware, fixed-layout typed records are preferable:

```text
todo-item:
    slot 0 = desc
    slot 1 = target
    slot 2 = status
```

The template can then store a tiny numeric slot ID. Association lists such as `((desc . "...") ...)` would also work, but require more memory and repeated symbol lookup.

The complete conceptual model becomes:

- `UiValue`: a global renderable value, such as `todos`.
- `UiType`: the expected structure of a value or list item.
- `UiTemplate`: formatting and layout instructions applied in a context.
- `UiListTemplate`: source list, visible range and item template.
- `UiInstance`: where a template is rendered.
- `ItemContext`: a transient pointer to the current item.

The list's GC root automatically keeps its cons cells, items and item fields alive. The template does not need separate GC handles for every item.

For the first implementation, redraw the visible rows whenever the list view renders. That reliably catches:

- items added or removed;
- fields replaced;
- strings modified in place;
- ordering changes.

With only perhaps five rows on the OLED, this is likely cheaper and substantially simpler than maintaining per-item dependency subscriptions. Later, typed records could carry revision counters if measurements show that invalidation is worthwhile.

List scrolling follows naturally: add `offset` alongside `limit`, skip that many items, and instantiate the same row template only for the visible range.

## Fixed-layout to-do example

**User**

Can you show me how the uLisp code for that might look for your above example, but with the fixed-layout record? Show code that declares three to-do items in a list and then modifies the status of one of them.

**Assistant**

Using existing uLisp arrays, each to-do can be a three-slot fixed-layout record. This part is valid current-style uLisp:

```lisp
;; Fixed slot layout for todo-item
(defvar todo-desc-slot   0)
(defvar todo-target-slot 1)
(defvar todo-status-slot 2)

(defun make-todo (desc target status)
  (let ((item (make-array 3)))
    (setf (aref item todo-desc-slot) desc)
    (setf (aref item todo-target-slot) target)
    (setf (aref item todo-status-slot) status)
    item))

;; Dates are represented as YYYYMMDD integers for this example.
(defvar todos
  (list
    (make-todo "Buy milk"       20260809 "to do")
    (make-todo "Write report"   20260810 "in progress")
    (make-todo "Call the bank"  20260812 "to do")))

;; Change the second item's status:
(setf
  (aref (nth 1 todos) todo-status-slot)
  "done")
```

After the modification, the logical contents are:

```lisp
(
  #("Buy milk"      20260809 "to do")
  #("Write report"  20260810 "done")
  #("Call the bank" 20260812 "to do")
)
```

uLisp supports one-dimensional arrays through `make-array`, reads them with `aref`, and allows array elements to be changed using `setf`. See the [current uLisp implementation](https://github.com/technoblogy/ulisp-esp/blob/master/ulisp-esp.ino#L2614-L2626).

With the proposed SilOS UI extensions, the surrounding declarations might be:

```lisp
(defuitype todo-item
  (desc   string   0)
  (target datetime 1)
  (status string   2))

(defui todos (list-of todo-item)
  (list
    (make-todo "Buy milk"      20260809 "to do")
    (make-todo "Write report"  20260810 "in progress")
    (make-todo "Call the bank" 20260812 "to do")))

(defuitemplate todo-row (item todo-item)
  (str  (field item desc)
        :width 16
        :overflow chop)

  (date (field item target)
        :format "yyyy-mm-dd")

  (str  (field item status)
        :width 6
        :overflow chop))

(defuilist todo-list
  :source todos
  :item-template todo-row
  :limit 5)
```

`defuitype` would compile the field names to array positions:

```text
desc   -> 0
target -> 1
status -> 2
```

Consequently, while rendering a row:

```lisp
(field item status)
```

effectively means:

```lisp
(aref item 2)
```

The status update replaces only slot 2's reference with the new Lisp string. The list, record array, templates and other fields are unchanged, and the renderer reads `"done"` directly from that slot on its next pass.

If dirty invalidation is introduced, the existing `setf` path for `aref` is the appropriate place to notify the UI that an observed record changed.
