;;; Editor-only Common Lisp descriptions of the proposed SilOS uLisp API.
;;;
;;; Alive runs an SBCL language server. Loading this file into that process
;;; gives Alive function and macro bindings to introspect for semantic syntax
;;; highlighting and signature help. This file is not part of the SilOS/uLisp
;;; runtime and its definitions must never be loaded by an application.
;;;
;;; Keep these lambda lists aligned with docs/design/API-*.md while those
;;; documents remain the authoritative descriptions of the proposed APIs.
;;; API docs SHA-256: 3c755e5dc46c8819e4637cbd58b855149e7eb4d42a485cae2be8c641b264fa21

(in-package #:cl-user)

(shadow '(shell-app-register
          shell-app-on-event
          shell-event-type
          shell-module-import
          shell-module-require
          shell-request-poke
          mqtt-bind
          mqtt-publish
          mqtt-ref-update
          mqtt-ref-watch
          mqtt-subscribe
          store-bind
          store-create
          store-delete
          store-error
          store-get
          store-list
          store-meta
          store-row-add
          store-row-at
          store-row-count
          store-row-delete
          store-row-delete-id
          store-row-error
          store-row-field
          store-row-id
          store-row-revision
          store-row-status
          store-row-watch
          store-rows-watch
          store-status
          store-value
          store-watch
          ui-bind
          ui-date
          ui-field
          ui-invalidate
          ui-mount
          ui-release
          ui-template
          ui-template-list
          ui-text
          ui-type
          ui-unmount))

(defun %silos-editor-only (name)
  (error "~A is an editor-only description of a SilOS/uLisp primitive." name))

;;; BoundQueueStore API

(defun store-create (name kind &optional schema)
  (declare (ignore name kind schema))
  (%silos-editor-only 'store-create))

(defun store-list (&optional prefix)
  (declare (ignore prefix))
  (%silos-editor-only 'store-list))

(defun store-meta (name)
  (declare (ignore name))
  (%silos-editor-only 'store-meta))

(defun store-delete (name)
  (declare (ignore name))
  (%silos-editor-only 'store-delete))

(defun store-get (name fields start count)
  (declare (ignore name fields start count))
  (%silos-editor-only 'store-get))

(defun store-bind (name fields start count)
  (declare (ignore name fields start count))
  (%silos-editor-only 'store-bind))

(defun store-status (ref)
  (declare (ignore ref))
  (%silos-editor-only 'store-status))

(defun store-error (ref)
  (declare (ignore ref))
  (%silos-editor-only 'store-error))

(defun store-value (ref)
  (declare (ignore ref))
  (%silos-editor-only 'store-value))

(defun store-row-count (ref)
  (declare (ignore ref))
  (%silos-editor-only 'store-row-count))

(defun store-row-at (ref index)
  (declare (ignore ref index))
  (%silos-editor-only 'store-row-at))

(defun store-row-id (row)
  (declare (ignore row))
  (%silos-editor-only 'store-row-id))

(defun store-row-revision (row)
  (declare (ignore row))
  (%silos-editor-only 'store-row-revision))

(defun store-row-status (row)
  (declare (ignore row))
  (%silos-editor-only 'store-row-status))

(defun store-row-error (row)
  (declare (ignore row))
  (%silos-editor-only 'store-row-error))

(defun store-row-field (row field)
  (declare (ignore row field))
  (%silos-editor-only 'store-row-field))

(defun (setf store-row-field) (new-value row field)
  (declare (ignore new-value row field))
  (%silos-editor-only '(setf store-row-field)))

(defun store-row-add (name record)
  (declare (ignore name record))
  (%silos-editor-only 'store-row-add))

(defun store-row-delete (row-ref)
  (declare (ignore row-ref))
  (%silos-editor-only 'store-row-delete))

(defun store-row-delete-id (name id)
  (declare (ignore name id))
  (%silos-editor-only 'store-row-delete-id))

(defun store-watch (store-ref callback)
  (declare (ignore store-ref callback))
  (%silos-editor-only 'store-watch))

(defun store-row-watch (row-ref callback)
  (declare (ignore row-ref callback))
  (%silos-editor-only 'store-row-watch))

(defun store-rows-watch (store-ref callback)
  (declare (ignore store-ref callback))
  (%silos-editor-only 'store-rows-watch))

;;; BoundQueueMQTT API

(defun mqtt-publish (topic payload &optional qos retained)
  (declare (ignore topic payload qos retained))
  (%silos-editor-only 'mqtt-publish))

(defun mqtt-subscribe (filter mode limit)
  (declare (ignore filter mode limit))
  (%silos-editor-only 'mqtt-subscribe))

(defun mqtt-ref-watch (mqtt-ref callback)
  (declare (ignore mqtt-ref callback))
  (%silos-editor-only 'mqtt-ref-watch))

(defun mqtt-ref-update (mqtt-ref)
  (declare (ignore mqtt-ref))
  (%silos-editor-only 'mqtt-ref-update))

(defun mqtt-bind (topic)
  (declare (ignore topic))
  (%silos-editor-only 'mqtt-bind))

;;; Shell API

(defmacro shell-app-register (&rest options)
  (declare (ignore options))
  '(%silos-editor-only 'shell-app-register))

(defun shell-app-on-event (handler)
  (declare (ignore handler))
  (%silos-editor-only 'shell-app-on-event))

(defun shell-event-type (event)
  (declare (ignore event))
  (%silos-editor-only 'shell-event-type))

(defun shell-request-poke (type &rest parameters)
  (declare (ignore type parameters))
  (%silos-editor-only 'shell-request-poke))

(defun shell-module-require (library)
  (declare (ignore library))
  (%silos-editor-only 'shell-module-require))

(defun shell-module-import (store-name)
  (declare (ignore store-name))
  (%silos-editor-only 'shell-module-import))

;;; UI API. These declarations follow the API's current syntactic roles. The
;;; documents deliberately leave their final reader/macro implementation open.

(defmacro ui-bind (name type)
  (declare (ignore name type))
  '(%silos-editor-only 'ui-bind))

(defmacro ui-type (name &rest fields)
  (declare (ignore name fields))
  '(%silos-editor-only 'ui-type))

(defmacro ui-field (item field-name)
  (declare (ignore item field-name))
  '(%silos-editor-only 'ui-field))

(defmacro ui-template (name item-binding &body instructions)
  (declare (ignore name item-binding instructions))
  '(%silos-editor-only 'ui-template))

(defmacro ui-text (value &key width overflow)
  (declare (ignore value width overflow))
  '(%silos-editor-only 'ui-text))

(defmacro ui-date (value &key format)
  (declare (ignore value format))
  '(%silos-editor-only 'ui-date))

(defmacro ui-template-list (name &rest options)
  (declare (ignore name options))
  '(%silos-editor-only 'ui-template-list))

(defun ui-mount (template)
  (declare (ignore template))
  (%silos-editor-only 'ui-mount))

(defun ui-invalidate (ui-ref)
  (declare (ignore ui-ref))
  (%silos-editor-only 'ui-invalidate))

(defun ui-unmount (mount)
  (declare (ignore mount))
  (%silos-editor-only 'ui-unmount))

(defun ui-release (resource)
  (declare (ignore resource))
  (%silos-editor-only 'ui-release))
