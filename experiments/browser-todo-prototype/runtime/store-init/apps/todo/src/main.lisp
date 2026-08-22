; These declarations are source-load-safe: they retain only bounded template
; metadata. They do not bind the StoreRef or mount a view while source loads.
(defuitype todo-item (desc string 0) (status string 1))

; Each field directive names a typed row field and its bounded character width.
; The Shell owns this immutable description, never a current row value.
(defvar todo-row
  (defuitemplate todo-row (item todo-item)
    (str (field item desc) :width 32 :overflow chop)
    (str (field item status) :width 16 :overflow chop)))

; APP-START is intentionally the final action during source evaluation. The
; Shell sends APP-INITIALISE later, after this closure has been registered.
(app-start
  (lambda (event)
    ; The first lifecycle event has no app-specific native meaning. This app
    ; chooses INIT stage one as its own payload convention and asks for a generic poke.
    ; Every later event is an app-owned poke tag followed by a stage number.
    ; Nothing native recognises INIT or either stage number.
    (cond
      ((equal event 'app-initialise)
       (app-request-poke 'init 1))
      ((and (listp event) (eq (car event) 'poke) (equal (car (cdr event)) 'init)
            (eq (car (cdr (cdr event))) 1))
       (progn
                ; Stage one is the first permitted Store bind. The critical
                ; StoreRef watch is attached before requesting another turn.
                (defvar todo-items
                  (store-bind "todo/items.csv" '(desc status) 0 5))
                (store-ref-watch
                  todo-items
                  (lambda (live old-value)
                    ; Store changes belong to this watch, never to APP-START.
                    ; The test sink merely proves pending -> ready observation.
                    (silos-test-watch-observation
                      (field (field live 'meta) 'status)
                      (if (field live 'value) (length (field live 'value)) 0)
                      (if (field live 'value)
                          (field (field (car (field live 'value)) 'value) 'desc)
                        nil)
                      (field (field old-value 'meta) 'status)
                      (field old-value 'value))))
                ; DEFUI returns the stable UiRef handle for this live StoreRef.
                (defvar todo-items-ui
                  (defui todo-items (store-ref (list-of todo-item)) todo-items))
                (silos-test-app-stage 1)
                (app-request-poke 'init 2)))
      ((and (listp event) (eq (car event) 'poke) (equal (car (cdr event)) 'init)
            (eq (car (cdr (cdr event))) 2))
       (progn
                  (defvar todo-list
                    (defuilist todo-list
                      :source todo-items-ui
                      :item-template todo-row
                      :offset 0
                      :limit 5
                      :pending "Loading to-dos..."
                      :empty "No to-dos."
                      :error "To-dos unavailable."))
                  ; MAIN is a semantic Shell region. UI code never chooses a
                  ; coordinate, font, pixel buffer, or an app render loop.
                  (defvar todo-list-mount (ui-mount todo-list :region 'main))
                  (silos-test-app-stage 2))))))
