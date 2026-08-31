# SilOS

SilOS is a small operating environment whose applications and portable system
behaviour are written in uLisp.

## Language

**Ref**:
A stable uLisp-visible live reference whose value or state may change through a
SilOS service.
_Avoid_: Handle, observable

**StoreRef**:
A Ref representing an asynchronous Store operation or live bounded row result.
_Avoid_: Store handle

**StoreRowRef**:
A Ref representing one identified, revisioned row in a Store.
_Avoid_: Row handle, record handle

**BoundStore**:
The shared live coordination state for one named Store, used by every binding
to that Store across all applications.
_Avoid_: StoreBound, per-app Store

**StoreBinding**:
One application's requested live field and row window over a BoundStore.
_Avoid_: BoundStore, Store handle

**StoreAppBinding**:
The collection of StoreBindings owned by one application.
_Avoid_: StoreBinding

**UiRef**:
A Ref retaining one named uLisp binding for use by Shell-owned UI resources.
_Avoid_: UI handle, binding handle
