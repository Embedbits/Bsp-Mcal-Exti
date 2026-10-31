/**
 * \defgroup Exti Exti
 * \brief Exti module
 */

/**
 * \author Mr.Nobody
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


/** Enumeration used to select the GPIO pin speed of the EXTI input pin */
typedef enum
{
    EXTI_PIN_SPEED_LOW = 0u , /**< Select I/O low output speed    */
    EXTI_PIN_SPEED_MEDIUM   , /**< Select I/O medium output speed */
    EXTI_PIN_SPEED_HIGH     , /**< Select I/O fast output speed   */
    EXTI_PIN_SPEED_VERY_HIGH, /**< Select I/O high output speed   */
}   exti_PinSpeed_t;


/**
 * \brief Trigger edge of an EXTI line.
 *
 * GPIO lines (\ref exti_PeriphConfig_t) and configurable peripheral event lines
 * (\ref exti_LineConfig_t) require one of the edge items. \ref EXTI_TRIGGER_EDGE_NONE
 * is required by direct peripheral event lines (they have no trigger configuration),
 * is rejected by \ref Exti_Init and by configurable lines and is reported by
 * \ref Exti_Get_LinePending when no edge is pending.
 */
typedef enum
{
    EXTI_TRIGGER_EDGE_FALLING = 0u, /**< Falling edge triggers the line            */
    EXTI_TRIGGER_EDGE_RAISING,      /**< Rising edge triggers the line             */
    EXTI_TRIGGER_EDGE_BOTH,         /**< Both edges trigger the line               */
    EXTI_TRIGGER_EDGE_NONE          /**< No trigger edge (direct lines, no pending) */
}   exti_TriggerEdge_t;


/** \brief Error ISR routine type definition */
typedef void ( exti_ExtiIsrCallback_t )( exti_TriggerEdge_t );


/** \brief Interrupt priority of the EXTI line (passed to the NVIC module) */
typedef uint32_t exti_IrqPrio_t;


/** \brief Configuration structure of an EXTI line connected to a GPIO pin (lines 0 .. 15) */
typedef struct
{
    exti_PinId_t            ExtiPin;         /**< GPIO pin number (EXTI line 0 .. 15)      */
    exti_PortId_t           ExtiPort;        /**< GPIO port connected to the line          */
    exti_PinPullCfg_t       ExtiPinPull;     /**< Pull resistor of the pin                 */
    exti_PinSpeed_t         ExtiPinSpeed;    /**< Speed of the pin                         */
    exti_IrqPrio_t          ExtiPriority;    /**< Interrupt priority of the line           */
    exti_TriggerEdge_t      ExtiTriggerEdge; /**< Trigger edge(s) of the line              */
    exti_ExtiIsrCallback_t *ExtiCallback;    /**< Callback called from the ISR, can be NULL */
}   exti_PeriphConfig_t;


/**
 * \brief Peripheral event lines (EXTI lines 16 .. 66) named by the connected peripheral.
 *
 * The list holds only the lines present on the device line: every item is guarded
 * by the CMSIS bit definition of its interrupt mask (EXTI_IMR1_IMxx, EXTI_IMR2_IMxx,
 * EXTI_IMR3_IMxx) as the LL driver does, lines whose source peripheral depends on
 * the device are guarded by the peripheral (ETH, USB_OTG_HS) or by the configurable
 * trigger bit (EXTI_RTSR2_RT58) and lines 28 / 29 of STM32H503 by the device.
 * Sources: RM0481 Rev 5 Table 157, RM0492 Rev 3 Table 100, RM0517 Rev 1 Table 169.
 * The line type (configurable / direct) is reported by \ref Exti_Get_LineType.
 * The NVIC vector of a peripheral line belongs to the module of the peripheral.
 */
typedef enum
{
    EXTI_LINE_PVD_AVD = 0u,     /**< Line 16: PVD / AVD output (configurable)                      */
    EXTI_LINE_RTC,              /**< Line 17: RTC (nonsecure) (direct)                             */
#if defined(EXTI_IMR1_IM18)
    EXTI_LINE_RTC_S,            /**< Line 18: RTC secure (direct)                                  */
#endif
    EXTI_LINE_TAMP,             /**< Line 19: TAMP (nonsecure) (direct)                            */
#if defined(EXTI_IMR1_IM20)
    EXTI_LINE_TAMP_S,           /**< Line 20: TAMP secure (direct)                                 */
#endif
    EXTI_LINE_I2C1_WKUP,        /**< Line 21: I2C1 wake-up (direct)                                */
    EXTI_LINE_I2C2_WKUP,        /**< Line 22: I2C2 wake-up (direct)                                */
#if defined(EXTI_IMR1_IM23)
    EXTI_LINE_I2C3_WKUP,        /**< Line 23: I2C3 wake-up (direct)                                */
#endif
    EXTI_LINE_I3C1_WKUP,        /**< Line 24: I3C1 wake-up (direct)                                */
    EXTI_LINE_USART1_WKUP,      /**< Line 25: USART1 wake-up (direct)                              */
    EXTI_LINE_USART2_WKUP,      /**< Line 26: USART2 wake-up (direct)                              */
    EXTI_LINE_USART3_WKUP,      /**< Line 27: USART3 wake-up (direct)                              */
#if defined(STM32H503xx)
    EXTI_LINE_I3C2_WKUP,        /**< Line 28: I3C2 wake-up (direct) - STM32H503                    */
    EXTI_LINE_COMP1,            /**< Line 29: COMP1 output wake-up (direct) - STM32H503            */
#else
    EXTI_LINE_UART4_WKUP,       /**< Line 28: UART4 wake-up (direct)                               */
    EXTI_LINE_UART5_WKUP,       /**< Line 29: UART5 wake-up (direct)                               */
#endif
#if defined(EXTI_IMR1_IM30)
    EXTI_LINE_USART6_WKUP,      /**< Line 30: USART6 wake-up (direct)                              */
#endif
#if defined(EXTI_IMR1_IM31)
    EXTI_LINE_UART7_WKUP,       /**< Line 31: UART7 wake-up (direct)                               */
#endif
#if defined(EXTI_IMR2_IM32)
    EXTI_LINE_UART8_WKUP,       /**< Line 32: UART8 wake-up (direct)                               */
#endif
#if defined(EXTI_IMR2_IM33)
    EXTI_LINE_UART9_WKUP,       /**< Line 33: UART9 wake-up (direct)                               */
#endif
#if defined(EXTI_IMR2_IM34)
    EXTI_LINE_USART10_WKUP,     /**< Line 34: USART10 wake-up (direct)                             */
#endif
#if defined(EXTI_IMR2_IM35)
    EXTI_LINE_USART11_WKUP,     /**< Line 35: USART11 wake-up (direct)                             */
#endif
#if defined(EXTI_IMR2_IM36)
    EXTI_LINE_UART12_WKUP,      /**< Line 36: UART12 wake-up (direct)                              */
#endif
    EXTI_LINE_LPUART1_WKUP,     /**< Line 37: LPUART1 wake-up (direct)                             */
    EXTI_LINE_LPTIM1,           /**< Line 38: LPTIM1 (direct)                                      */
    EXTI_LINE_LPTIM2,           /**< Line 39: LPTIM2 (direct)                                      */
    EXTI_LINE_SPI1_WKUP,        /**< Line 40: SPI1 wake-up (direct)                                */
    EXTI_LINE_SPI2_WKUP,        /**< Line 41: SPI2 wake-up (direct)                                */
    EXTI_LINE_SPI3_WKUP,        /**< Line 42: SPI3 wake-up (direct)                                */
#if defined(EXTI_IMR2_IM43)
    EXTI_LINE_SPI4_WKUP,        /**< Line 43: SPI4 wake-up (direct)                                */
#endif
#if defined(EXTI_IMR2_IM44)
    EXTI_LINE_SPI5_WKUP,        /**< Line 44: SPI5 wake-up (direct)                                */
#endif
#if defined(EXTI_IMR2_IM45)
    EXTI_LINE_SPI6_WKUP,        /**< Line 45: SPI6 wake-up (direct)                                */
#endif
#if defined(ETH)
    EXTI_LINE_ETH_WKUP,         /**< Line 46: Ethernet wake-up (configurable)                      */
#endif
    EXTI_LINE_USB_WKUP,         /**< Line 47: USB FS wake-up (direct)                              */
#if defined(EXTI_IMR2_IM48)
    EXTI_LINE_UCPD1_WKUP,       /**< Line 48: UCPD1 wake-up (direct)                               */
#endif
    EXTI_LINE_LPTIM2_CH1,       /**< Line 49: LPTIM2 channel 1 (configurable where EXTI_RTSR2_RT49
                                     exists: STM32H543 / H553 / H5Ex / H5Fx, else direct)          */
    EXTI_LINE_DTS_WKUP,         /**< Line 50: DTS wake-up (configurable)                           */
#if defined(EXTI_IMR2_IM51)
    EXTI_LINE_CEC_WKUP,         /**< Line 51: HDMI-CEC wake-up (direct)                            */
#endif
#if defined(EXTI_IMR2_IM52)
    EXTI_LINE_I2C4_WKUP,        /**< Line 52: I2C4 wake-up (direct)                                */
#endif
    EXTI_LINE_UVM,              /**< Line 53: USB voltage monitor output (configurable)            */
#if defined(EXTI_IMR2_IM54)
    EXTI_LINE_LPTIM3,           /**< Line 54: LPTIM3 (direct)                                      */
#endif
#if defined(EXTI_IMR2_IM55)
    EXTI_LINE_LPTIM4,           /**< Line 55: LPTIM4 (direct)                                      */
#endif
#if defined(EXTI_IMR2_IM56)
    EXTI_LINE_LPTIM5,           /**< Line 56: LPTIM5 (direct)                                      */
#endif
#if defined(EXTI_IMR2_IM57)
    EXTI_LINE_LPTIM6,           /**< Line 57: LPTIM6 (direct)                                      */
#endif
#if defined(EXTI_IMR2_IM58)
#if defined(EXTI_RTSR2_RT58)
    EXTI_LINE_COMP1,            /**< Line 58: COMP1 output (configurable) - STM32H543 / H553 / H5Ex / H5Fx */
#else
    EXTI_LINE_I3C2_WKUP,        /**< Line 58: I3C2 wake-up (direct) - STM32H523 / H533             */
#endif
#endif
#if defined(EXTI_IMR2_IM59)
    EXTI_LINE_COMP2,            /**< Line 59: COMP2 output (configurable)                          */
#endif
#if defined(EXTI_IMR2_IM60) && \
    defined(USB_OTG_HS)
    EXTI_LINE_USB_OTG_HS_WKUP,  /**< Line 60: USB OTG HS wake-up (direct)                          */
#endif
#if defined(EXTI_IMR2_IM61)
    EXTI_LINE_I3C2_WKUP,        /**< Line 61: I3C2 wake-up (direct)                                */
#endif
#if defined(EXTI_IMR2_IM62)
    EXTI_LINE_ADF1,             /**< Line 62: ADF1 (direct)                                        */
#endif
#if defined(EXTI_IMR2_IM63)
    EXTI_LINE_MDF1,             /**< Line 63: MDF1 (direct)                                        */
#endif
#if defined(EXTI_IMR3_IM64)
    EXTI_LINE_PLAY1_OUT14,      /**< Line 64: PLAY1 output 14 (configurable)                       */
#endif
#if defined(EXTI_IMR3_IM65)
    EXTI_LINE_IWDG,             /**< Line 65: IWDG (direct)                                        */
#endif
#if defined(EXTI_IMR3_IM66)
    EXTI_LINE_VDDIO2_VM,        /**< Line 66: VDDIO2 voltage monitor (configurable)                */
#endif
    EXTI_LINE_CNT               /**< Count of the peripheral event lines of the device line        */
}   exti_LineId_t;


/** Type of a peripheral event line */
typedef enum
{
    EXTI_LINE_TYPE_CONFIGURABLE = 0u, /**< Edge triggers and pending flags in the EXTI (RTSR / FTSR / RPR / FPR) */
    EXTI_LINE_TYPE_DIRECT             /**< No trigger and no pending flag in the EXTI (status in the peripheral) */
}   exti_LineType_t;


/** Mode of an active peripheral event line */
typedef enum
{
    EXTI_LINE_MODE_INTERRUPT = 0u, /**< Wake-up with interrupt (IMR)            */
    EXTI_LINE_MODE_EVENT,          /**< Wake-up with event (EMR)                */
    EXTI_LINE_MODE_BOTH            /**< Wake-up with interrupt and event        */
}   exti_LineMode_t;


/** \brief Configuration structure of a peripheral event line (\ref Exti_Set_LineActive) */
typedef struct
{
    exti_LineMode_t    LineMode;    /**< Interrupt, event or both                                            */
    exti_TriggerEdge_t TriggerEdge; /**< Edge(s) of a configurable line, \ref EXTI_TRIGGER_EDGE_NONE for a
                                         direct line                                                        */
}   exti_LineConfig_t;

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */


#endif /* EXTI_EXTI_TYPES_H */
