; The entry creates one lexical app instance and then returns to the uLisp task.
(let ((todo-items
; STORE-BIND returns a live StoreRef immediately, before its rows are ready.
       (store-bind "todo/items.csv" '(desc status) 0 8)))
; A StoreRef watch is different from APP-START's Shell event handler.
; It runs after the uLisp task has changed this particular live reference.
  (store-ref-watch
    todo-items
    (lambda (live old-value)
; A StoreRef separates its request metadata from its current row collection.
      (let ((live-status (field (field live 'meta) 'status))
; COUNT reads the live row collection. It is NIL while the bind is pending.
            (rows (field live 'value)))
; This observes one bound StoreRowRef's named application field.
; It makes no ordering promise; ordering is a separate future Store API.
        (silos-test-watch-observation
          live-status
          (if rows (length rows) 0)
          (if rows (field (field (car rows) 'value) 'desc) nil)
; OLD-VALUE is a bounded non-live snapshot from before the ready mutation.
          (field (field old-value 'meta) 'status)
          (field old-value 'value)))))
; APP-START retains an app-level handler for later Shell lifecycle and
; inter-application events. StoreRef state is deliberately not tested here.
  (app-start
    (lambda (event)
      event)))
