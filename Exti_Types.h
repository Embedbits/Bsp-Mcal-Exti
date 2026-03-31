/*
 *    Mr.Nobody, COPYRIGHT (c) 2021
 *    ALL RIGHTS RESERVED
 *
 */

/**
 * \file Exti_Types.h
 * \ingroup Exti
 * \brief Exti module global types definition
 *
 * This file contains the types definitions used across the module and are 
 * available for other modules through Port file.
 *
 */

#ifndef EXTI_EXTI_TYPES_H
#define EXTI_EXTI_TYPES_H
/* ============================== INCLUDES ================================== */
#include "stdint.h"                         /* Module types definition        */
#include "Stm32.h"                          /* GPIO utilities functionality   */
#include "Stm32_system.h"
#include "Stm32_exti.h"                     /* MCU EXTI RAL functionality     */
/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Null pointer definition */
#define EXTI_NULL_PTR                        ( ( void* ) 0u )

/* ========================== EXPORTED MACROS =============================== */

/* ============================== TYPEDEFS ================================== */

/** \brief Type signaling major version of SW module */
typedef uint8_t exti_MajorVersion_t;


/** \brief Type signaling minor version of SW module */
typedef uint8_t exti_MinorVersion_t;


/** \brief Type signaling patch version of SW module */
typedef uint8_t exti_PatchVersion_t;


/** \brief Type signaling actual version of SW module */
typedef struct
{
    exti_MajorVersion_t Major; /**< Major version */
    exti_MinorVersion_t Minor; /**< Minor version */
    exti_PatchVersion_t Patch; /**< Patch version */
}   exti_ModuleVersion_t;


/** Function status enumeration */
typedef enum
{
    EXTI_FUNCTION_INACTIVE = 0u, /**< Function status is inactive */
    EXTI_FUNCTION_ACTIVE         /**< Function status is active   */
}   exti_FunctionState_t;


/** Flag states enumeration */
typedef enum
{
    EXTI_FLAG_INACTIVE = 0u, /**< Inactive flag state */
    EXTI_FLAG_ACTIVE         /**< Active flag state   */
}   exti_FlagState_t;


/** Enumeration used to signal request processing state */
typedef enum
{
    EXTI_REQUEST_ERROR = 0u, /**< Processing request failed  */
    EXTI_REQUEST_OK          /**< Processing request succeed */
}   exti_RequestState_t;


/** GPIO Pins identification enumeration */
typedef enum
{
    EXTI_PIN_0  , /**< GPIO Pin 0  */
    EXTI_PIN_1  , /**< GPIO Pin 1  */
    EXTI_PIN_2  , /**< GPIO Pin 2  */
    EXTI_PIN_3  , /**< GPIO Pin 3  */
    EXTI_PIN_4  , /**< GPIO Pin 4  */
    EXTI_PIN_5  , /**< GPIO Pin 5  */
    EXTI_PIN_6  , /**< GPIO Pin 6  */
    EXTI_PIN_7  , /**< GPIO Pin 7  */
    EXTI_PIN_8  , /**< GPIO Pin 8  */
    EXTI_PIN_9  , /**< GPIO Pin 9  */
    EXTI_PIN_10 , /**< GPIO Pin 10 */
    EXTI_PIN_11 , /**< GPIO Pin 11 */
    EXTI_PIN_12 , /**< GPIO Pin 12 */
    EXTI_PIN_13 , /**< GPIO Pin 13 */
    EXTI_PIN_14 , /**< GPIO Pin 14 */
    EXTI_PIN_15 , /**< GPIO Pin 15 */
    EXTI_PIN_CNT,
}   exti_PinId_t;


/** GPIO peripheral identification enumeration */
typedef enum
{
#if defined(GPIOA)
    EXTI_PORT_A = 0u, /**< GPIO Port A */
#endif
#if defined(GPIOB)
    EXTI_PORT_B,      /**< GPIO Port B */
#endif
#if defined(GPIOC)
    EXTI_PORT_C,      /**< GPIO Port C */
#endif
#if defined(GPIOD)
    EXTI_PORT_D,      /**< GPIO Port D */
#endif
#if defined(GPIOE)
    EXTI_PORT_E,      /**< GPIO Port E */
#endif
#if defined(GPIOF)
    EXTI_PORT_F,      /**< GPIO Port F */
#endif
#if defined(GPIOG)
    EXTI_PORT_G,      /**< GPIO Port G */
#endif
#if defined(GPIOH)
    EXTI_PORT_H,      /**< GPIO Port H */
#endif
#if defined(GPIOI)
    EXTI_PORT_I,      /**< GPIO Port I */
#endif
#if defined(GPIOJ)
    EXTI_PORT_J,      /**< GPIO Port J */
#endif
    EXTI_PORT_CNT
}   exti_PortId_t;


/** Enumeration used to signal pull-up, pull-down or no pull for pin */
typedef enum
{
    EXTI_PIN_PULL_NONE = 0u, /**< No pull up/down is applied/ */
    EXTI_PIN_PULL_UP,        /**< Pull up resistor is used    */
    EXTI_PIN_PULL_DOWN,      /**< Pull down resistor is used  */
}   exti_PinPullCfg_t;


typedef enum
{
    EXTI_PIN_SPEED_LOW = 0u , /**< Select I/O low output speed    */
    EXTI_PIN_SPEED_MEDIUM   , /**< Select I/O medium output speed */
    EXTI_PIN_SPEED_HIGH     , /**< Select I/O fast output speed   */
    EXTI_PIN_SPEED_VERY_HIGH, /**< Select I/O high output speed   */
}   exti_PinSpeed_t;


typedef enum
{
    EXTI_TRIGGER_EDGE_FALLING = 0u,
    EXTI_TRIGGER_EDGE_RAISING,
    EXTI_TRIGGER_EDGE_BOTH
}   exti_TriggerEdge_t;


/** \brief Error ISR routine type definition */
typedef void ( exti_ExtiIsrCallback_t )( exti_TriggerEdge_t );


typedef uint32_t exti_IrqPrio_t;


typedef struct
{
    exti_PinId_t            ExtiPin;
    exti_PortId_t           ExtiPort;
    exti_PinPullCfg_t       ExtiPinPull;
    exti_PinSpeed_t         ExtiPinSpeed;
    exti_IrqPrio_t          ExtiPriority;
    exti_TriggerEdge_t      ExtiTriggerEdge;
    exti_ExtiIsrCallback_t *ExtiCallback;
}   exti_PeriphConfig_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* EXTI_EXTI_TYPES_H */
