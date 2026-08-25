# Emscripten FreeRTOS port

This Browser-specific cooperative FreeRTOS port was promoted from the
successful FreeWisp experiment. It maps tasks to Emscripten fibers, returns
each yield through a central event-loop scheduler, and keeps Asyncify
continuation storage separate from conventional task C stacks.

The Browser target supplies `FreeRTOSConfig.h`, including its tick rate. It may
override `configEMSCRIPTEN_MAX_TASKS` and
`configEMSCRIPTEN_ASYNCIFY_STACK_BYTES`; the defaults are 8 tasks and 4096
bytes per Asyncify continuation stack.
