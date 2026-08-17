# Emscripten FreeRTOS port

**Related:** [FreeWisp plan](../plan.md) and
[FreeRTOS uLisp task proof](../freertos-ulisp-task/README.md).

Shared cooperative FreeRTOS port for FreeWisp targets. It maps tasks to
Emscripten fibers, returns every yield through a central event-loop scheduler,
and keeps Asyncify continuation storage separate from conventional task C
stacks.

Each target supplies its own `FreeRTOSConfig.h`, including its platform tick
rate. Targets may override `configEMSCRIPTEN_MAX_TASKS` and
`configEMSCRIPTEN_ASYNCIFY_STACK_BYTES`; the defaults are 8 tasks and 4096
bytes per Asyncify continuation stack.
