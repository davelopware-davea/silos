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

**UiRef**:
A Ref retaining one named uLisp binding for use by Shell-owned UI resources.
_Avoid_: UI handle, binding handle
