; These declarations are source-load-safe: they retain only bounded template
; metadata. They do not bind the StoreRef or mount a view while source loads.
(ui-type todo-item (desc string 0) (status string 1))

; The outer bindings are this app instance's private state. The registered
; event-handler closure retains them between later event turns.
(let ((todo-row-ui-temp
        ; Each field directive names a typed row field and its bounded
        ; character width. The Shell owns this immutable description, never a
        ; current row value.
        (ui-template todo-row-ui-temp (item todo-item)
          (ui-text "TODO:")
          (ui-text (ui-field item desc) :width 32 :overflow ui-chop)
          (ui-text (ui-field item status) :width 16 :overflow ui-chop)))
      (todo-items-store nil)
      (todo-items-ui nil)
      (todo-list-ui-ltemp nil)
      (todo-list-ui-mount nil)
      (todo-count-ui-temp nil)
      (todo-count-ui-mount nil))
  ; SHELL-APP-ON-EVENT is intentionally the final action during source
  ; evaluation. The Shell sends SHELL-APP-INITIALISE later, after this closure
  ; has been registered.
  (shell-app-on-event
    (lambda (event)
      ; Every event starts with its type. This app chooses INIT with a stage
      ; parameter as its own convention; nothing native interprets either value.
      (case (shell-event-type event)
        (shell-app-initialise
         (shell-request-poke 'init 1))
        (init
         (case (nth 1 event)
           (1
            (progn
              ; Stage one is the first permitted Store bind. The critical
              ; StoreRef watch is attached before requesting another turn.
              (setq todo-items-store
                    (store-bind "todo/items.csv" '(desc status) 0 5))
              (store-watch
                todo-items-store
                (lambda (live old-value)
                  ; Store changes belong to this watch, never to the app event
                  ; handler. The test sink merely proves pending -> ready
                  ; observation.
                  (silos-test-watch-observation
                    (store-status live)
                    (store-row-count live)
                    (store-row-field (store-row-at live 0) 'desc)
                    (store-status old-value))))
              ; UI-BIND retains the TODO-ITEMS-STORE lexical location and
              ; returns its stable UI handle.
              (setq todo-items-ui
                    (ui-bind todo-items-store (store-ref (ui-list-of todo-item))))
              (silos-test-app-stage 1)
              (shell-request-poke 'init 2)))
           (2
            (progn
              (setq todo-list-ui-ltemp
                    (ui-template-list todo-list-ui-ltemp
                      :source todo-items-ui
                      :item-template todo-row-ui-temp
                      :offset 0
                      :limit 5
                      :pending "Loading to-dos..."
                      :empty "No to-dos."
                      :error "To-dos unavailable."))
              ; The Shell chooses placement. UI code never chooses a coordinate,
              ; font, pixel buffer, or an app render loop.
              (setq todo-list-ui-mount
                    (ui-mount todo-list-ui-ltemp))
              (setq todo-count-ui-temp
                    (ui-template todo-count-ui-temp
                      (ui-text "Count")
                      (ui-text (ui-field todo-items-ui count)
                        :width 8 :overflow ui-chop)))
              (setq todo-count-ui-mount
                    (ui-mount todo-count-ui-temp))
              (silos-test-app-stage 2)))))))))
