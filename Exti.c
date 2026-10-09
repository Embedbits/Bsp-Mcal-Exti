/**
 * \author Mr.Nobody
 * \file Exti.c
 * \ingroup Exti
 * \brief External Interrupt module common functionality
 *
 * STM32L4 / STM32L4+ EXTI specifics:
 *  - GPIO port of the line is selected in SYSCFG EXTICR (SYSCFG clock is
 *    activated by \ref Exti_Init).
 *  - One pending register PR1 for both edges (write 1 to clear).
 *  - Lines 5 - 9 share NVIC vector EXTI9_5, lines 10 - 15 share EXTI15_10.
 *    Priority of the shared vector is the priority of the last initialized
 *    line of the group.
 *  - ISRs end with DSB - Cortex-M4 r0p1 core erratum 838869 (Arm ID 838869,
 *    "store immediate overlapping exception return might vector to incorrect
 *    interrupt"), STM32L4 device errata not reviewed yet.
 */
/* ============================== INCLUDES ================================== */
#include "Exti.h"                           /* Self include                   */
#include "Exti_Port.h"                      /* Own port file include          */
#include "Exti_Types.h"                     /* Module types definitions       */
#include "Nvic_Port.h"                      /* NVIC handler functionality     */
#include "Gpio_Port.h"                      /* GPIO handler functionality     */
#include "Rcc_Port.h"                       /* SYSCFG clock control           */
#include "Stm32_exti.h"                     /* EXTI Registers definitions     */
#include "Stm32_system.h"                   /* SYSCFG EXTI source selection   */
/* ============================== TYPEDEFS ================================== */

/** \brief External interrupt line configuration structure. */
typedef struct
{
    gpio_PinId_t         GpioPinId;     /**< GPIO pin of the line                  */
    nvic_PeriphIrqList_t NvicIrqId;     /**< NVIC vector of the line               */
    nvic_IsrCallback_t   NvicIsr;       /**< ISR registered in NVIC                */
    uint32_t             SysLine;       /**< SYSCFG EXTICR field of the line       */
    uint32_t             ExtiLine;      /**< EXTI line bit                         */
    uint32_t             IrqLinesMask;  /**< EXTI lines sharing the NVIC vector    */
}   exti_LinesConfigStruct_t;


/** \brief GPIO port configuration structure. */
typedef struct
{
    exti_PortId_t ExtiPortId;           /**< EXTI port identification              */
    gpio_PortId_t GpioPortId;           /**< GPIO module port identification       */
    uint32_t      SysPortReg;           /**< SYSCFG EXTICR value of the port       */
}   exti_PortConfigStruct_t;


/** \brief Runtime state of the line. */
typedef struct
{
    exti_PinId_t            ExtiPinId;       /**< Line identification               */
    exti_ExtiIsrCallback_t *ExtiIsrCallback; /**< User callback, can be NULL        */
    exti_TriggerEdge_t      ExtiTriggerEdge; /**< Configured trigger edge(s)        */
    gpio_PortId_t           GpioPortId;      /**< Port connected to the line        */
}   exti_LineState_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static inline void          Exti_IsrHandler         ( exti_PinId_t lineId );
static exti_TriggerEdge_t   Exti_Get_DetectedEdge   ( exti_PinId_t lineId );

static void Exti_Line0_IsrHandler( void );
static void Exti_Line1_IsrHandler( void );
static void Exti_Line2_IsrHandler( void );
static void Exti_Line3_IsrHandler( void );
static void Exti_Line4_IsrHandler( void );
static void Exti_Line5_9_IsrHandler( void );
static void Exti_Line10_15_IsrHandler( void );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define EXTI_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define EXTI_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define EXTI_PATCH_VERSION           ( 0u )

/** Default interrupt priority used by \ref Exti_Get_DefaultConfig */
#define EXTI_DEFAULT_IRQ_PRIO        ( 10u )

/** Lines with own NVIC vector (EXTI0 - EXTI4) */
#define EXTI_IRQ_LINES_0             ( LL_EXTI_LINE_0 )
#define EXTI_IRQ_LINES_1             ( LL_EXTI_LINE_1 )
#define EXTI_IRQ_LINES_2             ( LL_EXTI_LINE_2 )
#define EXTI_IRQ_LINES_3             ( LL_EXTI_LINE_3 )
#define EXTI_IRQ_LINES_4             ( LL_EXTI_LINE_4 )

/** Lines sharing NVIC vector EXTI9_5 */
#define EXTI_IRQ_LINES_5_9           ( LL_EXTI_LINE_5  | LL_EXTI_LINE_6  | LL_EXTI_LINE_7  | \
                                       LL_EXTI_LINE_8  | LL_EXTI_LINE_9                      )

/** Lines sharing NVIC vector EXTI15_10 */
#define EXTI_IRQ_LINES_10_15         ( LL_EXTI_LINE_10 | LL_EXTI_LINE_11 | LL_EXTI_LINE_12 | \
                                       LL_EXTI_LINE_13 | LL_EXTI_LINE_14 | LL_EXTI_LINE_15   )

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

/** Runtime state of the lines (written by thread, read by ISR) */
static volatile exti_LineState_t        exti_LineState[ EXTI_PIN_CNT ] =
{
  { .ExtiPinId = EXTI_PIN_0  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_1  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_2  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_3  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_4  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_5  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_6  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_7  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_8  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_9  , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_10 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_11 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_12 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_13 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_14 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
  { .ExtiPinId = EXTI_PIN_15 , .ExtiIsrCallback = EXTI_NULL_PTR , .ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING , .GpioPortId = GPIO_PORT_CNT },
};

/** External interrupt lines configuration array */
static exti_LinesConfigStruct_t const     exti_PinConf[ EXTI_PIN_CNT ] =
{
  { .GpioPinId = GPIO_PIN_ID_0  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI0     , .ExtiLine = LL_EXTI_LINE_0  , .SysLine = LL_SYSCFG_EXTI_LINE0  , .IrqLinesMask = EXTI_IRQ_LINES_0     , .NvicIsr = Exti_Line0_IsrHandler     },
  { .GpioPinId = GPIO_PIN_ID_1  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI1     , .ExtiLine = LL_EXTI_LINE_1  , .SysLine = LL_SYSCFG_EXTI_LINE1  , .IrqLinesMask = EXTI_IRQ_LINES_1     , .NvicIsr = Exti_Line1_IsrHandler     },
  { .GpioPinId = GPIO_PIN_ID_2  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI2     , .ExtiLine = LL_EXTI_LINE_2  , .SysLine = LL_SYSCFG_EXTI_LINE2  , .IrqLinesMask = EXTI_IRQ_LINES_2     , .NvicIsr = Exti_Line2_IsrHandler     },
  { .GpioPinId = GPIO_PIN_ID_3  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI3     , .ExtiLine = LL_EXTI_LINE_3  , .SysLine = LL_SYSCFG_EXTI_LINE3  , .IrqLinesMask = EXTI_IRQ_LINES_3     , .NvicIsr = Exti_Line3_IsrHandler     },
  { .GpioPinId = GPIO_PIN_ID_4  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI4     , .ExtiLine = LL_EXTI_LINE_4  , .SysLine = LL_SYSCFG_EXTI_LINE4  , .IrqLinesMask = EXTI_IRQ_LINES_4     , .NvicIsr = Exti_Line4_IsrHandler     },
  { .GpioPinId = GPIO_PIN_ID_5  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9_5   , .ExtiLine = LL_EXTI_LINE_5  , .SysLine = LL_SYSCFG_EXTI_LINE5  , .IrqLinesMask = EXTI_IRQ_LINES_5_9   , .NvicIsr = Exti_Line5_9_IsrHandler   },
  { .GpioPinId = GPIO_PIN_ID_6  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9_5   , .ExtiLine = LL_EXTI_LINE_6  , .SysLine = LL_SYSCFG_EXTI_LINE6  , .IrqLinesMask = EXTI_IRQ_LINES_5_9   , .NvicIsr = Exti_Line5_9_IsrHandler   },
  { .GpioPinId = GPIO_PIN_ID_7  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9_5   , .ExtiLine = LL_EXTI_LINE_7  , .SysLine = LL_SYSCFG_EXTI_LINE7  , .IrqLinesMask = EXTI_IRQ_LINES_5_9   , .NvicIsr = Exti_Line5_9_IsrHandler   },
  { .GpioPinId = GPIO_PIN_ID_8  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9_5   , .ExtiLine = LL_EXTI_LINE_8  , .SysLine = LL_SYSCFG_EXTI_LINE8  , .IrqLinesMask = EXTI_IRQ_LINES_5_9   , .NvicIsr = Exti_Line5_9_IsrHandler   },
  { .GpioPinId = GPIO_PIN_ID_9  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9_5   , .ExtiLine = LL_EXTI_LINE_9  , .SysLine = LL_SYSCFG_EXTI_LINE9  , .IrqLinesMask = EXTI_IRQ_LINES_5_9   , .NvicIsr = Exti_Line5_9_IsrHandler   },
  { .GpioPinId = GPIO_PIN_ID_10 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_10 , .SysLine = LL_SYSCFG_EXTI_LINE10 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_11 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_11 , .SysLine = LL_SYSCFG_EXTI_LINE11 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_12 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_12 , .SysLine = LL_SYSCFG_EXTI_LINE12 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_13 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_13 , .SysLine = LL_SYSCFG_EXTI_LINE13 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_14 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_14 , .SysLine = LL_SYSCFG_EXTI_LINE14 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_15 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15_10 , .ExtiLine = LL_EXTI_LINE_15 , .SysLine = LL_SYSCFG_EXTI_LINE15 , .IrqLinesMask = EXTI_IRQ_LINES_10_15 , .NvicIsr = Exti_Line10_15_IsrHandler },
};


/** GPIO ports configuration array */
static exti_PortConfigStruct_t const    exti_PortConf[ EXTI_PORT_CNT ] =
{
#if defined(GPIOA)
  { .ExtiPortId = EXTI_PORT_A , .GpioPortId = GPIO_PORT_A , .SysPortReg = LL_SYSCFG_EXTI_PORTA },
#endif
#if defined(GPIOB)
  { .ExtiPortId = EXTI_PORT_B , .GpioPortId = GPIO_PORT_B , .SysPortReg = LL_SYSCFG_EXTI_PORTB },
#endif
#if defined(GPIOC)
  { .ExtiPortId = EXTI_PORT_C , .GpioPortId = GPIO_PORT_C , .SysPortReg = LL_SYSCFG_EXTI_PORTC },
#endif
#if defined(GPIOD)
  { .ExtiPortId = EXTI_PORT_D , .GpioPortId = GPIO_PORT_D , .SysPortReg = LL_SYSCFG_EXTI_PORTD },
#endif
#if defined(GPIOE)
  { .ExtiPortId = EXTI_PORT_E , .GpioPortId = GPIO_PORT_E , .SysPortReg = LL_SYSCFG_EXTI_PORTE },
#endif
#if defined(GPIOF)
  { .ExtiPortId = EXTI_PORT_F , .GpioPortId = GPIO_PORT_F , .SysPortReg = LL_SYSCFG_EXTI_PORTF },
#endif
#if defined(GPIOG)
  { .ExtiPortId = EXTI_PORT_G , .GpioPortId = GPIO_PORT_G , .SysPortReg = LL_SYSCFG_EXTI_PORTG },
#endif
#if defined(GPIOH)
  { .ExtiPortId = EXTI_PORT_H , .GpioPortId = GPIO_PORT_H , .SysPortReg = LL_SYSCFG_EXTI_PORTH },
#endif
#if defined(GPIOI)
  { .ExtiPortId = EXTI_PORT_I , .GpioPortId = GPIO_PORT_I , .SysPortReg = LL_SYSCFG_EXTI_PORTI },
#endif
#if defined(GPIOJ)
  { .ExtiPortId = EXTI_PORT_J , .GpioPortId = GPIO_PORT_J , .SysPortReg = LL_SYSCFG_EXTI_PORTJ },
#endif
#if defined(GPIOK)
  { .ExtiPortId = EXTI_PORT_K , .GpioPortId = GPIO_PORT_K , .SysPortReg = LL_SYSCFG_EXTI_PORTK },
#endif
};

/* ========================= EXPORTED FUNCTIONS ============================= */

/**
 * \brief Returns module SW version
 *
 * \return Module SW version
 */
exti_ModuleVersion_t Exti_Get_ModuleVersion( void )
{
    exti_ModuleVersion_t retVersion;

    retVersion.Major = EXTI_MAJOR_VERSION;
    retVersion.Minor = EXTI_MINOR_VERSION;
    retVersion.Patch = EXTI_PATCH_VERSION;

    return (retVersion);
}


/**
 * \brief Configures external interrupt line
 *
 * Activates SYSCFG clock, configures the GPIO pin as input, connects it to the
 * EXTI line (SYSCFG EXTICR), configures trigger edges, clears pending flag of
 * the line and enables the line interrupt in EXTI and NVIC.
 *
 * \note Lines 5 - 9 and 10 - 15 share one NVIC vector - the priority of the
 *       vector is set by the last initialized line of the group.
 *
 * \param extiConfig [in]: Pointer to external interrupt configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Init( exti_PeriphConfig_t * const extiConfig )
{
    exti_RequestState_t retState = EXTI_REQUEST_ERROR;

    if( ( EXTI_NULL_PTR             != extiConfig                  ) &&
        ( EXTI_PIN_CNT               > extiConfig->ExtiPin         ) &&
        ( EXTI_PORT_CNT              > extiConfig->ExtiPort        ) &&
        ( EXTI_PIN_PULL_DOWN        >= extiConfig->ExtiPinPull     ) &&
        ( EXTI_PIN_SPEED_VERY_HIGH  >= extiConfig->ExtiPinSpeed    ) &&
        ( EXTI_TRIGGER_EDGE_BOTH    >= extiConfig->ExtiTriggerEdge )    )
    {
        const exti_PinId_t  pinId            = extiConfig->ExtiPin;
        const uint32_t      extiLine         = exti_PinConf[ pinId ].ExtiLine;
        rcc_RequestState_t  rccRequestState  = RCC_REQUEST_ERROR;
        gpio_RequestState_t gpioRequestState = GPIO_REQUEST_ERROR;
        nvic_RequestState_t nvicReq          = NVIC_REQUEST_ERROR;
        gpio_Config_t       gpioConfig       = { 0u };

        gpioConfig.PortId  = exti_PortConf[ extiConfig->ExtiPort ].GpioPortId;
        gpioConfig.PinId   = exti_PinConf[ pinId ].GpioPinId;
        gpioConfig.PinMode = GPIO_PIN_MODE_INPUT;

        if( EXTI_PIN_PULL_DOWN == extiConfig->ExtiPinPull )
        {
            gpioConfig.PinPull = GPIO_PIN_PULL_DOWN;
        }
        else if( EXTI_PIN_PULL_UP == extiConfig->ExtiPinPull )
        {
            gpioConfig.PinPull = GPIO_PIN_PULL_UP;
        }
        else
        {
            gpioConfig.PinPull = GPIO_PIN_PULL_NONE;
        }

        if( EXTI_PIN_SPEED_LOW == extiConfig->ExtiPinSpeed )
        {
            gpioConfig.PinSpeed = GPIO_PIN_SPEED_LOW;
        }
        else if( EXTI_PIN_SPEED_MEDIUM == extiConfig->ExtiPinSpeed )
        {
            gpioConfig.PinSpeed = GPIO_PIN_SPEED_MEDIUM;
        }
        else if( EXTI_PIN_SPEED_HIGH == extiConfig->ExtiPinSpeed )
        {
            gpioConfig.PinSpeed = GPIO_PIN_SPEED_HIGH;
        }
        else
        {
            gpioConfig.PinSpeed = GPIO_PIN_SPEED_VERY_HIGH;
        }

        /* -1- SYSCFG clock - EXTICR is not writable without it */
        rccRequestState = Rcc_Set_PeriphActive( RCC_PERIPH_SYSCFG );

        /* -2- Configure GPIO pin as input */
        if( RCC_REQUEST_OK == rccRequestState )
        {
            gpioRequestState = Gpio_Init( &gpioConfig );
        }
        else
        {
            /* SYSCFG clock activation failed */
        }

        if( GPIO_REQUEST_OK == gpioRequestState )
        {
            /* Line interrupt is disabled during configuration */
            LL_EXTI_DisableIT_0_31( extiLine );

            /* Store line runtime state */
            exti_LineState[ pinId ].ExtiIsrCallback = extiConfig->ExtiCallback;
            exti_LineState[ pinId ].ExtiTriggerEdge = extiConfig->ExtiTriggerEdge;
            exti_LineState[ pinId ].GpioPortId      = gpioConfig.PortId;

            /* -3- Connect External Line to the GPIO */
            LL_SYSCFG_SetEXTISource( exti_PortConf[ extiConfig->ExtiPort ].SysPortReg, exti_PinConf[ pinId ].SysLine );

            /* -4- Configure trigger edges */
            LL_EXTI_DisableFallingTrig_0_31( extiLine );
            LL_EXTI_DisableRisingTrig_0_31( extiLine );

            if( ( EXTI_TRIGGER_EDGE_FALLING == extiConfig->ExtiTriggerEdge ) ||
                ( EXTI_TRIGGER_EDGE_BOTH    == extiConfig->ExtiTriggerEdge )    )
            {
                LL_EXTI_EnableFallingTrig_0_31( extiLine );
            }
            else
            {
                /* Falling edge is not used */
            }

            if( ( EXTI_TRIGGER_EDGE_RAISING == extiConfig->ExtiTriggerEdge ) ||
                ( EXTI_TRIGGER_EDGE_BOTH    == extiConfig->ExtiTriggerEdge )    )
            {
                LL_EXTI_EnableRisingTrig_0_31( extiLine );
            }
            else
            {
                /* Rising edge is not used */
            }

            /* -5- Clear pending flag of previous configuration and enable line interrupt */
            LL_EXTI_ClearFlag_0_31( extiLine );

            LL_EXTI_EnableIT_0_31( extiLine );

            /* -6- Configure NVIC */
            nvicReq = Nvic_Set_PeriphIrq_Prio( exti_PinConf[ pinId ].NvicIrqId, (nvic_IrqPrio_t)extiConfig->ExtiPriority );
        }
        else
        {
            /* GPIO configuration failed */
            nvicReq = NVIC_REQUEST_ERROR;
        }

        if( NVIC_REQUEST_OK == nvicReq )
        {
            nvicReq = Nvic_Set_PeriphIrq_Handler( exti_PinConf[ pinId ].NvicIrqId, exti_PinConf[ pinId ].NvicIsr );
        }
        else
        {
            /* Error during initialization process */
        }

        if( NVIC_REQUEST_OK == nvicReq )
        {
            nvicReq = Nvic_Set_PeriphIrq_Active( exti_PinConf[ pinId ].NvicIrqId );
        }
        else
        {
            /* Error during initialization process */
        }

        if( NVIC_REQUEST_OK == nvicReq )
        {
            retState = EXTI_REQUEST_OK;
        }
        else
        {
            retState = EXTI_REQUEST_ERROR;
        }
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Configures external interrupt line to default values and deactivate it.
 *
 * Disables the line interrupt and both trigger edges in EXTI, clears pending
 * flag, removes user callback, connects the line back to port A (reset value)
 * and configures the pin as analog. NVIC vector is disabled only when no other
 * line sharing the vector (EXTI9_5, EXTI15_10) is enabled. NVIC handler stays
 * registered (NVIC module does not accept NULL handler), it is not called
 * while the NVIC vector is disabled.
 *
 * \param extiConfig [in]: Pointer to external interrupt configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Deinit( exti_PeriphConfig_t * const extiConfig )
{
    exti_RequestState_t retState = EXTI_REQUEST_ERROR;

    if( ( EXTI_NULL_PTR != extiConfig           ) &&
        ( EXTI_PIN_CNT   > extiConfig->ExtiPin  ) &&
        ( EXTI_PORT_CNT  > extiConfig->ExtiPort )    )
    {
        const exti_PinId_t  pinId            = extiConfig->ExtiPin;
        const uint32_t      extiLine         = exti_PinConf[ pinId ].ExtiLine;
        const uint32_t      otherIrqLines    = exti_PinConf[ pinId ].IrqLinesMask & ~extiLine;
        nvic_RequestState_t nvicReq          = NVIC_REQUEST_OK;
        gpio_RequestState_t gpioRequestState = GPIO_REQUEST_ERROR;
        gpio_Config_t       gpioConfig       = { 0u };

        /* -1- Disable EXTI line interrupt and triggers, clear pending flag */
        LL_EXTI_DisableIT_0_31( extiLine );
        LL_EXTI_DisableFallingTrig_0_31( extiLine );
        LL_EXTI_DisableRisingTrig_0_31( extiLine );
        LL_EXTI_ClearFlag_0_31( extiLine );

        /* Remove line runtime state */
        exti_LineState[ pinId ].ExtiIsrCallback = EXTI_NULL_PTR;
        exti_LineState[ pinId ].ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING;
        exti_LineState[ pinId ].GpioPortId      = GPIO_PORT_CNT;

        /* -2- Connect the line to reset port A */
        LL_SYSCFG_SetEXTISource( LL_SYSCFG_EXTI_PORTA, exti_PinConf[ pinId ].SysLine );

        /* -3- Disable NVIC vector when not used by other line of the group */
        const uint32_t otherLinesActive = READ_BIT( EXTI->IMR1, otherIrqLines );

        if( 0u == otherLinesActive )
        {
            nvicReq = Nvic_Set_PeriphIrq_Inactive( exti_PinConf[ pinId ].NvicIrqId );
        }
        else
        {
            /* Vector is shared with other enabled line */
        }

        /* -4- Configure GPIO pin as analog (default state) */
        gpioConfig.PortId   = exti_PortConf[ extiConfig->ExtiPort ].GpioPortId;
        gpioConfig.PinId    = exti_PinConf[ pinId ].GpioPinId;
        gpioConfig.PinMode  = GPIO_PIN_MODE_ANALOG;
        gpioConfig.PinPull  = GPIO_PIN_PULL_NONE;
        gpioConfig.PinSpeed = GPIO_PIN_SPEED_LOW;

        gpioRequestState = Gpio_Init( &gpioConfig );

        if( ( NVIC_REQUEST_OK == nvicReq          ) &&
            ( GPIO_REQUEST_OK == gpioRequestState )    )
        {
            retState = EXTI_REQUEST_OK;
        }
        else
        {
            retState = EXTI_REQUEST_ERROR;
        }
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Main task of module Exti
 *
 * This function shall be called in the main loop of the application or the task
 * scheduler. It shall be called periodically, depending on the module's
 * requirements.
 */
void Exti_Task( void )
{
    return;
}



/**
 * \brief Initialize configuration structure with default values
 *
 * This function will initialize external interrupt on pin GPIOA.0 with
 * following parameters:
 * No internal pull up or down used
 * Pin speed is set to low
 * Falling edge in pin is set as trigger
 * Priority is set to \ref EXTI_DEFAULT_IRQ_PRIO
 * No callback address is used
 *
 * \param extiConfig [out]: Pointer to external interrupt configuration structure. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Get_DefaultConfig( exti_PeriphConfig_t * const extiConfig )
{
    exti_RequestState_t retState = EXTI_REQUEST_ERROR;

    if( EXTI_NULL_PTR != extiConfig )
    {
        extiConfig->ExtiPin         = EXTI_PIN_0;
        extiConfig->ExtiPort        = EXTI_PORT_A;
        extiConfig->ExtiPinPull     = EXTI_PIN_PULL_NONE;
        extiConfig->ExtiPinSpeed    = EXTI_PIN_SPEED_LOW;
        extiConfig->ExtiPriority    = EXTI_DEFAULT_IRQ_PRIO;
        extiConfig->ExtiTriggerEdge = EXTI_TRIGGER_EDGE_FALLING;
        extiConfig->ExtiCallback    = EXTI_NULL_PTR;

        retState = EXTI_REQUEST_OK;
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Common handler of external interrupt lines.
 *
 * Clears pending flag of the line (also without registered callback) and calls
 * user callback with detected edge. Flag is cleared before the callback, so a
 * new edge during the callback is not lost.
 *
 * \param lineId [in]: External interrupt line identification, value from \ref exti_PinId_t.
 */
static inline void Exti_IsrHandler( exti_PinId_t lineId )
{
    const uint32_t extiLine = exti_PinConf[ lineId ].ExtiLine;

    const uint32_t lineFlag = LL_EXTI_IsActiveFlag_0_31( extiLine );

    if( 0u != lineFlag )
    {
        LL_EXTI_ClearFlag_0_31( extiLine );

        exti_ExtiIsrCallback_t * const userCallback = exti_LineState[ lineId ].ExtiIsrCallback;

        if( EXTI_NULL_PTR != userCallback )
        {
            userCallback( Exti_Get_DetectedEdge( lineId ) );
        }
        else
        {
            /* No user callback registered */
        }
    }
    else
    {
        /* Flag inactive */
    }
}


/**
 * \brief Returns edge which triggered the line interrupt.
 *
 * STM32L4 EXTI has one pending flag for both edges. Line configured for one
 * edge reports the configured edge. Line configured for both edges reports
 * the edge according to the actual pin level (high - rising, low - falling).
 *
 * \param lineId [in]: External interrupt line identification, value from \ref exti_PinId_t.
 *
 * \return Detected edge - \ref EXTI_TRIGGER_EDGE_FALLING or \ref EXTI_TRIGGER_EDGE_RAISING.
 */
static exti_TriggerEdge_t Exti_Get_DetectedEdge( exti_PinId_t lineId )
{
    exti_TriggerEdge_t retEdge = exti_LineState[ lineId ].ExtiTriggerEdge;

    if( EXTI_TRIGGER_EDGE_BOTH == retEdge )
    {
        gpio_PinLevel_t pinLevel = GPIO_PIN_LEVEL_LOW;

        const gpio_RequestState_t gpioRequestState = Gpio_Get_PinLevel( exti_LineState[ lineId ].GpioPortId, exti_PinConf[ lineId ].GpioPinId, &pinLevel );

        if( ( GPIO_REQUEST_OK     == gpioRequestState ) &&
            ( GPIO_PIN_LEVEL_HIGH == pinLevel         )    )
        {
            retEdge = EXTI_TRIGGER_EDGE_RAISING;
        }
        else
        {
            retEdge = EXTI_TRIGGER_EDGE_FALLING;
        }
    }
    else
    {
        /* Single edge configured - reported as is */
    }

    return ( retEdge );
}

/* =========================== INTERRUPT HANDLERS =========================== */

/**
 * \brief External interrupt line 0 interrupt service routine
 */
static void Exti_Line0_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_0 );
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt line 1 interrupt service routine
 */
static void Exti_Line1_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_1 );
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt line 2 interrupt service routine
 */
static void Exti_Line2_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_2 );
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt line 3 interrupt service routine
 */
static void Exti_Line3_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_3 );
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt line 4 interrupt service routine
 */
static void Exti_Line4_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_4 );
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt lines 5 to 9 interrupt service routine
 */
static void Exti_Line5_9_IsrHandler( void )
{
    for( uint32_t lineId = EXTI_PIN_5; EXTI_PIN_9 >= lineId; lineId++ )
    {
        Exti_IsrHandler( (exti_PinId_t)lineId );
    }
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}


/**
 * \brief External interrupt lines 10 to 15 interrupt service routine
 */
static void Exti_Line10_15_IsrHandler( void )
{
    for( uint32_t lineId = EXTI_PIN_10; EXTI_PIN_15 >= lineId; lineId++ )
    {
        Exti_IsrHandler( (exti_PinId_t)lineId );
    }
    __DSB();    /* Cortex-M4 r0p1 erratum 838869: stores completed before the exception return */
}

/* ================================ TASKS =================================== */
