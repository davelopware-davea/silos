(let ((status-view-ui-temp nil)
      (status-view-ui-mount nil)
      (app-init-stage-1 nil))
  (setq app-init-stage-1
    (lambda ()
      (setq status-view-ui-temp
            (ui-template status-view-ui-temp
              (ui-text "System ready")))
      (setq status-view-ui-mount
            (ui-mount status-view-ui-temp))))

  (shell-app-on-event
    (lambda (event)
      (case (shell-event-type event)
        (shell-app-initialise
         (funcall app-init-stage-1))))))
