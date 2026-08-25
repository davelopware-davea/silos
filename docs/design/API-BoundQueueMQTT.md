# BoundQueueMQTT API

Draft recorded on 15 August 2026.

## Status

This document collects the current proposed uLisp API for applying the Ref and
queue metaphors to MQTT networking. It accompanies the exploratory
[Discussion-QueueMetaphors.md](Discussion-QueueMetaphors.md) and follows the
conventions in [API-BoundQueueStore.md](API-BoundQueueStore.md). It is a working
API specification, not yet a committed SilOS interface.

Examples use JSON-like notation where the exact uLisp representation remains
undecided.

## Contents

1. [Status](#status)
2. [Model](#model)
3. [Topics and filters](#topics-and-filters)
4. [Common values](#common-values)
5. [Publishing](#publishing)
6. [Subscribing to message streams](#subscribing-to-message-streams)
7. [Backlog and memory](#backlog-and-memory)
8. [Retained state binding](#retained-state-binding)
9. [UI composition](#ui-composition)
10. [Scheduling and ownership](#scheduling-and-ownership)
11. [Deferred decisions](#deferred-decisions)

## Model

BoundQueueMQTT exposes MQTT operations, subscriptions, and retained values
through asynchronous Refs. Queues, sockets, MQTT packets, reconnect logic, and
native buffers remain implementation details.

The initial uLisp application API may deliberately expose MQTT only. It need
not expose TCP, UDP, HTTP, broker connection objects, or byte streams. Broker
configuration, credentials, and connection policy belong outside this draft.

The model distinguishes:

- an **MqttRef**, representing an asynchronous publish or a subscription;
- an immutable **MessageRef**, representing one received MQTT message; and
- an **MqttValueRef**, representing the latest value of a retained state topic.

Only the uLisp task invokes Lisp callbacks or changes uLisp-visible Refs.

## Topics and filters

Publishing uses one concrete MQTT topic:

```text
chat/outbox/alice
house/temperature
```

Subscribing may use MQTT topic filters:

```text
chat/inbox/+
sensors/+/temperature
alerts/#
```

MQTT's topic and filter rules apply. In particular, `+` and `#` are subscription
wildcards and are not valid as concrete publish destinations. SilOS does not
add filesystem or BoundQueueStore path semantics to MQTT topics.

## Common values

### MqttRef

Every MQTT operation returns an MqttRef immediately:

```text
{
  meta: {
    operation: subscribe,
    status: silos-pending,
    error: nil
  },
  value: nil
}
```

`meta.operation` is immutable and identifies what produced the Ref:

```text
publish | subscribe | bind
```

Basic operation states are:

```text
silos-pending -> silos-ready
silos-pending -> silos-error
```

Publish and long-lived subscription Refs have additional states and metadata
described below. The meaning of `value` depends on the operation.

### MessageRef

An incoming MQTT message is immutable:

```text
{
  meta: {
    topic: "chat/inbox/alice",
    qos: 1,
    retained: nil,
    sequence: 27
  },
  value: "Hello"
}
```

`sequence` is a local receipt identity, not an MQTT revision or globally stable
message ID. Further protocol metadata may be added only when application code
needs it.

A MessageRef has no two-way update, save, revision, or conflict semantics.
System metadata and payload remain separate so an encoded structured payload
cannot collide with MQTT metadata.

## Publishing

### `mqtt-publish`

```lisp
(mqtt-publish topic payload [qos] [retained])
```

Publishes one immutable payload and immediately returns an MqttRef with
`operation: publish`.

```lisp
(defvar sending
  (mqtt-publish "chat/outbox/alice" "Hello" 1 nil))
```

Its progress is conceptually:

```text
silos-pending -> silos-queued -> silos-sent -> silos-acknowledged
                                     -> silos-error
```

The exact terminal state depends on QoS. A QoS 0 publish cannot promise broker
acknowledgement; QoS 1 or 2 may remain pending until the corresponding MQTT
exchange completes. Defaults, retry policy, and the meaning of failure across a
reconnect remain open.

## Subscribing to message streams

### `mqtt-subscribe`

```lisp
(mqtt-subscribe filter mode limit)
```

Creates a long-lived subscription and immediately returns an MqttRef with
`operation: subscribe`. Its value is a stable, bounded window containing at
most `limit` immutable MessageRefs.

```lisp
(defvar inbox
  (mqtt-subscribe "chat/inbox/+" 'mqtt-fifo 5))
```

When ready, its conceptual shape is:

```text
{
  meta: {
    operation: subscribe,
    status: silos-ready,
    mode: mqtt-fifo,
    limit: 5,
    waiting: 3,
    dropped: 0,
    error: nil
  },
  value: [up to five immutable MessageRefs]
}
```

The current `value` window does not change while application code may still be
processing it. New arrivals enter a bounded backlog and increase
`meta.waiting`.

### Subscription modes

Two initial modes are proposed:

- `mqtt-fifo` consumes the current window and selects the next oldest waiting
  messages; it is intended for chat, commands, and ordered work;
- `mqtt-latest` selects the newest waiting messages and skips superseded arrivals;
  it is intended for telemetry and dashboards.

Mode is fixed when the subscription is created. Positional mutation, editing,
and deletion of individual MessageRefs are not supported.

### `mqtt-watch`

```lisp
(mqtt-watch mqtt-ref
  (lambda (live waiting)
    ...))
```

Watches a subscription's top-level state and backlog availability. `waiting`
is the number of messages received since the current window was selected. The
callback does not receive the messages and new arrivals do not mutate the
current window.

Notifications may be coalesced: application code must treat `waiting` as the
current count rather than assuming one callback per message. The callback runs
on the uLisp task.

### `mqtt-update`

```lisp
(mqtt-update mqtt-ref)
```

Declares that the application has finished with the current window and updates
that same MqttRef according to its mode.

For `mqtt-fifo`, the old window is consumed and replaced by up to `limit` oldest
waiting messages. Later arrivals remain waiting. For `mqtt-latest`, the old
window is replaced by up to `limit` newest messages and older superseded arrivals are
skipped.

The update is asynchronous if loading the next window requires durable storage.
The exact status transition during an update and whether skipped messages have
a separate counter from overflow losses remain open.

## Backlog and memory

A subscription retains only:

- its current window;
- a bounded backlog of message descriptors and payloads; and
- fixed bookkeeping such as counts, limits, and watch handles.

Transient subscriptions may use a bounded native RAM ring. An `mqtt-latest`
window of one value can often retain only the current and newest pending payload.

Reliable chat or command subscriptions may spool arrivals into a
BoundQueueStore store. For the initial SilOS chat use case, a FIFO subscription
backed by the existing durable chat-message store is the likely default. This
avoids an unbounded MQTT inbox in RAM and avoids maintaining a second durable
copy solely for networking.

Where supported by the MQTT implementation, a QoS 1 acknowledgement should not
claim durable receipt before a message that requires durability has been
committed. Precise acknowledgement, capacity exhaustion, and reconnect
behaviour require an explicit policy.

`meta.dropped` exposes messages lost because the configured bounded backlog
could not retain them. Whether intentional `latest` supersession contributes to
that count remains undecided.

## Retained state binding

### `mqtt-bind`

```lisp
(mqtt-bind topic)
```

Returns an MqttValueRef representing the latest retained value of one concrete
state topic:

```lisp
(defvar temperature
  (mqtt-bind "house/temperature"))
```

Its conceptual shape is:

```text
{
  meta: {
    operation: bind,
    status: silos-pending,
    topic: "house/temperature",
    error: nil
  },
  value: nil
}
```

When a retained value arrives, the same Ref becomes ready and exposes it. A
two-way form may allow changing `value` to publish a replacement retained
value:

```text
MQTT retained topic <-> MqttValueRef <-> uLisp/UI
```

This is appropriate only when the topic is defined as state. It must not turn
ordinary event messages into mutable values. Watch syntax, optimistic publish
state, absence of a retained value, and concurrent-writer behaviour remain to
be specified.

## UI composition

MqttRefs, MessageRefs, and MqttValueRefs are uLisp-visible values and may be
exposed through SilOS UI bindings:

```text
MQTT -> bounded MqttRef window -> uLisp/UI
MQTT retained topic <-> MqttValueRef -> uLisp/UI
```

A template can display connection/request state, waiting and dropped counts,
message metadata and payloads, or a retained value. UI code advances a message
window explicitly; it does not observe the window changing underneath it.

## Scheduling and ownership

The MQTT task owns protocol processing and native receive buffers. It sends
bounded arrival and completion messages to the uLisp task but never invokes a
Lisp callback or mutates a uLisp-visible Ref directly.

The uLisp task owns MqttRefs, MessageRefs, MqttValueRefs, watch invocation, and
garbage-collector roots. Bulk payloads may be carried through stable bounded
native buffers or durable storage handles rather than copied into queue items.

Subscription and watch tables must be bounded. Releasing a subscription must
eventually remove its broker subscription, backlog, watches, handles, and GC
roots; the exact release API remains open.

## Deferred decisions

- broker configuration, credentials, permissions, and certificate handling;
- connection status, reconnection, persistent sessions, and offline publishing;
- exact QoS defaults and publish completion semantics;
- subscription release, cancellation, retry, timeout, and watch removal;
- RAM and durable backlog sizes and overflow behaviour;
- acknowledgement timing relative to durable storage;
- payload types, encodings, maximum sizes, and binary buffer representation;
- initial-window behaviour and update status transitions;
- skipped-versus-dropped accounting for `mqtt-latest` mode;
- retained binding watches, writes, conflicts, and deletion; and
- whether any MQTT protocol metadata beyond the minimal fields is exposed.
