(let ((status-view-ui-temp
        (ui-template status-view-ui-temp
          (ui-text "System ready"))))
  (ui-mount status-view-ui-temp)
  (shell-app-on-event (lambda (event) event)))
