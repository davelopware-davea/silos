#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <emscripten/fiber.h>
#include <emscripten.h>

#include "FreeRTOS.h"
#include "task.h"

#ifndef configEMSCRIPTEN_MAX_TASKS
    #define configEMSCRIPTEN_MAX_TASKS             8U
#endif

#ifndef configEMSCRIPTEN_ASYNCIFY_STACK_BYTES
    #define configEMSCRIPTEN_ASYNCIFY_STACK_BYTES  4096U
#endif

typedef struct PortTaskContext
{
    emscripten_fiber_t fiber;
    TaskFunction_t entry;
    void * parameter;
    uint8_t asyncify_stack[ configEMSCRIPTEN_ASYNCIFY_STACK_BYTES ];
} PortTaskContext;

typedef struct PortTaskSlot
{
    StackType_t * stack_marker;
    PortTaskContext * context;
} PortTaskSlot;

extern void * volatile pxCurrentTCB;

static PortTaskSlot xTaskSlots[ configEMSCRIPTEN_MAX_TASKS ];
static emscripten_fiber_t xSchedulerFiber;
static uint8_t ucSchedulerAsyncifyStack[ configEMSCRIPTEN_ASYNCIFY_STACK_BYTES ];
static UBaseType_t uxCriticalNesting;
static BaseType_t xSchedulerStarted;
static BaseType_t xWaitForTickRequested;
static double dNextTickTime;
static FreeWispPortStats xPortStats;

static StackType_t * prvCurrentStackMarker( void )
{
    configASSERT( pxCurrentTCB != NULL );
    return *( ( StackType_t ** ) pxCurrentTCB );
}

static PortTaskContext * prvFindContext( StackType_t * stack_marker )
{
    UBaseType_t index;

    for( index = 0; index < configEMSCRIPTEN_MAX_TASKS; ++index )
    {
        if( xTaskSlots[ index ].stack_marker == stack_marker )
        {
            return xTaskSlots[ index ].context;
        }
    }

    configASSERT( 0 );
    return NULL;
}

static void prvTaskEntry( void * argument )
{
    PortTaskContext * context = ( PortTaskContext * ) argument;

    context->entry( context->parameter );
    vTaskDelete( NULL );

    for( ; ; )
    {
        vPortYield();
    }
}

StackType_t * pxPortInitialiseStack( StackType_t * pxTopOfStack,
                                     StackType_t * pxEndOfStack,
                                     TaskFunction_t pxCode,
                                     void * pvParameters )
{
    UBaseType_t index;
    PortTaskContext * context = NULL;
    size_t c_stack_size = ( size_t ) ( pxTopOfStack - pxEndOfStack );

    configASSERT( c_stack_size >= 4096U );

    for( index = 0; index < configEMSCRIPTEN_MAX_TASKS; ++index )
    {
        if( xTaskSlots[ index ].context == NULL )
        {
            context = ( PortTaskContext * ) pvPortMalloc( sizeof( *context ) );
            configASSERT( context != NULL );
            xTaskSlots[ index ].stack_marker = pxTopOfStack;
            xTaskSlots[ index ].context = context;
            break;
        }
    }

    configASSERT( context != NULL );
    context->entry = pxCode;
    context->parameter = pvParameters;

    emscripten_fiber_init( &context->fiber,
                           prvTaskEntry,
                           context,
                           pxEndOfStack,
                           c_stack_size,
                           context->asyncify_stack,
                           sizeof( context->asyncify_stack ) );

    return pxTopOfStack;
}

BaseType_t xPortStartScheduler( void )
{
    PortTaskContext * next;
    const double tick_period_ms = 1000.0 / ( double ) configTICK_RATE_HZ;

    emscripten_fiber_init_from_current_context( &xSchedulerFiber,
                                                 ucSchedulerAsyncifyStack,
                                                 sizeof( ucSchedulerAsyncifyStack ) );
    xSchedulerStarted = pdTRUE;
    uxCriticalNesting = 0;
    dNextTickTime = emscripten_get_now() + tick_period_ms;

    while( xSchedulerStarted != pdFALSE )
    {
        double now;

        next = prvFindContext( prvCurrentStackMarker() );
        emscripten_fiber_swap( &xSchedulerFiber, &next->fiber );

        if( xSchedulerStarted == pdFALSE )
        {
            break;
        }

        if( xWaitForTickRequested != pdFALSE )
        {
            int sleep_ms;

            now = emscripten_get_now();
            sleep_ms = ( int ) ( dNextTickTime - now );
            if( sleep_ms < 1 )
            {
                sleep_ms = 1;
            }
            xWaitForTickRequested = pdFALSE;
            emscripten_sleep( sleep_ms );
        }
        else
        {
            emscripten_sleep( 0 );
        }

        now = emscripten_get_now();
        ++xPortStats.scheduler_passes;
        if( now >= dNextTickTime )
        {
            const double lateness_ms = now - dNextTickTime;

            if( lateness_ms > xPortStats.max_tick_lateness_ms )
            {
                xPortStats.max_tick_lateness_ms = lateness_ms;
            }
        }

        {
            uint32_t ticks_advanced = 0;

            while( now >= dNextTickTime )
            {
                ( void ) xTaskIncrementTick();
                dNextTickTime += tick_period_ms;
                ++ticks_advanced;
            }

            if( ticks_advanced > 1U )
            {
                ++xPortStats.tick_catchup_events;
            }
            if( ticks_advanced > xPortStats.max_ticks_per_pass )
            {
                xPortStats.max_ticks_per_pass = ticks_advanced;
            }
        }

        vTaskSwitchContext();
    }

    return pdFALSE;
}

void vPortResetStats( void )
{
    xPortStats.max_tick_lateness_ms = 0.0;
    xPortStats.scheduler_passes = 0U;
    xPortStats.tick_catchup_events = 0U;
    xPortStats.max_ticks_per_pass = 0U;
}

void vPortGetStats( FreeWispPortStats * stats )
{
    configASSERT( stats != NULL );
    *stats = xPortStats;
}

void vPortEndScheduler( void )
{
    PortTaskContext * current = prvFindContext( prvCurrentStackMarker() );

    xSchedulerStarted = pdFALSE;
    emscripten_fiber_swap( &current->fiber, &xSchedulerFiber );
    abort();
}

void vPortYield( void )
{
    PortTaskContext * current;

    if( xSchedulerStarted == pdFALSE )
    {
        return;
    }

    configASSERT( uxCriticalNesting == 0U );
    current = prvFindContext( prvCurrentStackMarker() );
    emscripten_fiber_swap( &current->fiber, &xSchedulerFiber );
}

void vPortWaitForTick( void )
{
    xWaitForTickRequested = pdTRUE;
    vPortYield();
}

void vPortEnterCritical( void )
{
    ++uxCriticalNesting;
}

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting > 0U );
    --uxCriticalNesting;
}

void vPortCleanUpTCB( void * tcb )
{
    StackType_t * stack_marker;
    UBaseType_t index;

    configASSERT( tcb != NULL );
    stack_marker = *( ( StackType_t ** ) tcb );

    for( index = 0; index < configEMSCRIPTEN_MAX_TASKS; ++index )
    {
        if( xTaskSlots[ index ].stack_marker == stack_marker )
        {
            vPortFree( xTaskSlots[ index ].context );
            xTaskSlots[ index ].context = NULL;
            xTaskSlots[ index ].stack_marker = NULL;
            return;
        }
    }

    configASSERT( 0 );
}
