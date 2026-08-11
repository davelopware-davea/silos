#ifndef PORTMACRO_H
#define PORTMACRO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define portSTACK_TYPE              uint8_t
#define portBASE_TYPE               int

typedef portSTACK_TYPE StackType_t;
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;

#define portMAX_DELAY               ( TickType_t ) UINT32_MAX
#define portTICK_TYPE_IS_ATOMIC      1
#define portSTACK_GROWTH             ( -1 )
#define portBYTE_ALIGNMENT           16
#define portPOINTER_SIZE_TYPE        uintptr_t
#define portHAS_STACK_OVERFLOW_CHECKING 1
#define portCRITICAL_NESTING_IN_TCB  0
#define portUSING_MPU_WRAPPERS       0
#define portARCH_NAME                "Emscripten fibers"

#define portTICK_PERIOD_MS           ( ( TickType_t ) 1000U / configTICK_RATE_HZ )
#define portNOP()                    __asm__ __volatile__( "nop" )
#define portMEMORY_BARRIER()         __asm__ __volatile__( "" ::: "memory" )
#define portSOFTWARE_BARRIER()       portMEMORY_BARRIER()

void vPortYield( void );
void vPortWaitForTick( void );
void vPortEnterCritical( void );
void vPortExitCritical( void );
void vPortCleanUpTCB( void * tcb );

#define portYIELD()                  vPortYield()
#define portYIELD_WITHIN_API()       vPortYield()
#define portYIELD_FROM_ISR( value )  do { if( ( value ) != 0 ) { vPortYield(); } } while( 0 )
#define portEND_SWITCHING_ISR( value ) portYIELD_FROM_ISR( value )

#define portENTER_CRITICAL()         vPortEnterCritical()
#define portEXIT_CRITICAL()          vPortExitCritical()
#define portDISABLE_INTERRUPTS()     vPortEnterCritical()
#define portENABLE_INTERRUPTS()      vPortExitCritical()

#define portCLEAN_UP_TCB( tcb )      vPortCleanUpTCB( tcb )

#define portTASK_FUNCTION_PROTO( function, parameters ) void function( void * parameters )
#define portTASK_FUNCTION( function, parameters )       void function( void * parameters )

#ifdef __cplusplus
}
#endif

#endif
