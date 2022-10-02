/**
 * @file
 *
 * @brief Configuration for security engine manager
 *
 * This glues the SE OSAL to the FreeRTOS routines.
 */
#ifndef EFWB_SE_MANAGER_CONFIG_H
#define EFWB_SE_MANAGER_CONFIG_H

// Set up for threading support (via FreeRTOS)
#define SL_SE_MANAGER_THREADING 1
#define SL_CATALOG_FREERTOS_KERNEL_PRESENT 1
#define SL_SE_MANAGER_YIELD_WHILE_WAITING_FOR_COMMAND_COMPLETION 1

// Required to check config consistency
#include "sl_se_manager_check_config.h"

/*
// Define OSAL stuff
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Defines

/// In order to wait forever in blocking functions the user can pass the
/// following value.
#define SE_MANAGER_OSAL_WAIT_FOREVER  (osWaitForever)
/// In order to return immediately in blocking functions the user can pass the
/// following value.
#define SE_MANAGER_OSAL_NON_BLOCKING  (0)

/// Priority to use for SEMBRX IRQ
#if defined(SE_MANAGER_USER_SEMBRX_IRQ_PRIORITY)
  #if (SE_MANAGER_USER_SEMBRX_IRQ_PRIORITY >= (1U << __NVIC_PRIO_BITS) )
      #error Illegal SEMBRX priority level.
  #endif
  #if defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
    #if (SE_MANAGER_USER_SEMBRX_IRQ_PRIORITY < (configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS) ) )
      #error Illegal SEMBRX priority level.
    #endif
  #else
    #if (SE_MANAGER_USER_SEMBRX_IRQ_PRIORITY < CORE_ATOMIC_BASE_PRIORITY_LEVEL)
      #error Illegal SEMBRX priority level.
    #endif
  #endif
  #define SE_MANAGER_SEMBRX_IRQ_PRIORITY SE_MANAGER_USER_SEMBRX_IRQ_PRIORITY
#else
  #if defined(SL_CATALOG_FREERTOS_KERNEL_PRESENT)
    #define SE_MANAGER_SEMBRX_IRQ_PRIORITY (configMAX_SYSCALL_INTERRUPT_PRIORITY >> (8U - __NVIC_PRIO_BITS) )
  #else
    #define SE_MANAGER_SEMBRX_IRQ_PRIORITY (CORE_ATOMIC_BASE_PRIORITY_LEVEL)
  #endif
#endif

/// Determine if executing at interrupt level on ARM Cortex-M.
#define RUNNING_AT_INTERRUPT_LEVEL (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)

#ifdef __cplusplus
}
#endif
*/

#endif
