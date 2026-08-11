#include <stdio.h>
#include <stdlib.h>

#include <emscripten.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#define APP_TASK_STACK_BYTES  16384U
#define MESSAGE_COUNT         3

static QueueHandle_t xQueue;
static TimerHandle_t xProofTimer;
static int xReceived[ MESSAGE_COUNT ];
static int xReceivedCount;
static int xTimerFired;
static char cTrace[ 64 ];
static size_t xTraceLength;
static double dStartTime;
static double dTimerTime;
static double dProducerTimes[ MESSAGE_COUNT ];

static void prvTrace( char event )
{
    configASSERT( xTraceLength + 1U < sizeof( cTrace ) );
    cTrace[ xTraceLength++ ] = event;
    cTrace[ xTraceLength ] = '\0';
}

void vFreeWispAssert( const char * file,
                      int line )
{
    fprintf( stderr, "FreeWisp assertion failed at %s:%d\n", file, line );
    abort();
}

void vApplicationMallocFailedHook( void )
{
    vFreeWispAssert( "FreeRTOS heap exhausted", 0 );
}

void vApplicationIdleHook( void )
{
    vPortWaitForTick();
}

static void prvTimerCallback( TimerHandle_t timer )
{
    configASSERT( timer == xProofTimer );
    ++xTimerFired;
    dTimerTime = emscripten_get_now();
    prvTrace( 'T' );
}

static void prvProducerTask( void * argument )
{
    static const int values[ MESSAGE_COUNT ] = { 10, 20, 30 };
    int index;

    ( void ) argument;
    for( index = 0; index < MESSAGE_COUNT; ++index )
    {
        dProducerTimes[ index ] = emscripten_get_now();
        configASSERT( xQueueSend( xQueue, &values[ index ], portMAX_DELAY ) == pdPASS );
        prvTrace( 'P' );
        vTaskDelay( 2 );
    }

    for( ; ; )
    {
        vTaskDelay( 1 );
    }
}

static void prvConsumerTask( void * argument )
{
    int value;
    int passed;
    double timer_elapsed;
    double producer_gap_one;
    double producer_gap_two;

    ( void ) argument;
    while( xReceivedCount < MESSAGE_COUNT )
    {
        configASSERT( xQueueReceive( xQueue, &value, portMAX_DELAY ) == pdPASS );
        xReceived[ xReceivedCount++ ] = value;
        prvTrace( 'C' );
    }

    while( xTimerFired == 0 )
    {
        vTaskDelay( 1 );
    }

    timer_elapsed = dTimerTime - dStartTime;
    producer_gap_one = dProducerTimes[ 1 ] - dProducerTimes[ 0 ];
    producer_gap_two = dProducerTimes[ 2 ] - dProducerTimes[ 1 ];

    passed = ( xReceived[ 0 ] == 10 ) &&
             ( xReceived[ 1 ] == 20 ) &&
             ( xReceived[ 2 ] == 30 ) &&
             ( xTimerFired == 1 ) &&
             ( timer_elapsed >= 35.0 ) &&
             ( timer_elapsed < 1000.0 ) &&
             ( producer_gap_one >= 8.0 ) &&
             ( producer_gap_two >= 8.0 );

    printf( "trace=%s ticks=%lu received=%d timer=%d "
            "timer_ms=%.2f producer_gaps_ms=%.2f,%.2f\n",
            cTrace,
            ( unsigned long ) xTaskGetTickCount(),
            xReceivedCount,
            xTimerFired,
            timer_elapsed,
            producer_gap_one,
            producer_gap_two );
    printf( "%s\n", passed ? "FREEWISP_KERNEL_PROOF_PASS" : "FREEWISP_KERNEL_PROOF_FAIL" );

    if( !passed )
    {
        abort();
    }

    vTaskEndScheduler();
    abort();
}

int main( void )
{
    xQueue = xQueueCreate( 1, sizeof( int ) );
    configASSERT( xQueue != NULL );

    xProofTimer = xTimerCreate( "proof",
                                5,
                                pdFALSE,
                                NULL,
                                prvTimerCallback );
    configASSERT( xProofTimer != NULL );

    configASSERT( xTaskCreate( prvConsumerTask,
                               "consumer",
                               APP_TASK_STACK_BYTES,
                               NULL,
                               2,
                               NULL ) == pdPASS );
    configASSERT( xTaskCreate( prvProducerTask,
                               "producer",
                               APP_TASK_STACK_BYTES,
                               NULL,
                               2,
                               NULL ) == pdPASS );
    configASSERT( xTimerStart( xProofTimer, 0 ) == pdPASS );

    dStartTime = emscripten_get_now();
    vTaskStartScheduler();
    return 0;
}
