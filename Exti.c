/**
 * \author Mr.Nobody
 * \file Exti.h
 * \ingroup Exti
 * \brief External Interrupt module common functionality
 *
 */
/* ============================== INCLUDES ================================== */
#include "Exti.h"                           /* Self include                   */
#include "Exti_Port.h"                      /* Own port file include          */
#include "Exti_Types.h"                     /* Module types definitions       */

#include "Nvic_Port.h"                      /* NVIC handler functionality     */
#include "Gpio_Port.h"                      /* GPIO handler functionality     */
#include "Stm32.h"                          /* MCU common functionality       */
#include "Stm32_exti.h"                     /* EXTI module functionality      */
#include "Stm32_utils.h"                    /* MCU utilities functionality    */
/* ============================== TYPEDEFS ================================== */

/** \brief Available peripherals configuration structure. */
typedef struct
{
    gpio_PinId_t         GpioPinId;
    nvic_PeriphIrqList_t NvicIrqId;
    nvic_IsrCallback_t   NvicIsr;
    uint32_t             SysLine;
    uint32_t             ExtiLine;
}   exti_LinesConfigStruct_t;


typedef struct
{
    exti_PortId_t ExtiPortId;
    gpio_PortId_t GpioPortId;
    uint32_t      SysPortReg;
}   exti_PortConfigStruct_t;


typedef struct
{
    exti_PinId_t            ExtiPinId;
    exti_ExtiIsrCallback_t *extiIsrCallback;
}   exti_IsrCallback_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static inline void Exti_IsrHandler( exti_PinId_t lineId );

static void Exti_Line0_IsrHandler( void );
static void Exti_Line1_IsrHandler( void );
static void Exti_Line2_IsrHandler( void );
static void Exti_Line3_IsrHandler( void );
static void Exti_Line4_IsrHandler( void );
static void Exti_Line5_IsrHandler( void );
static void Exti_Line6_IsrHandler( void );
static void Exti_Line7_IsrHandler( void );
static void Exti_Line8_IsrHandler( void );
static void Exti_Line9_IsrHandler( void );
static void Exti_Line10_IsrHandler( void );
static void Exti_Line11_IsrHandler( void );
static void Exti_Line12_IsrHandler( void );
static void Exti_Line13_IsrHandler( void );
static void Exti_Line14_IsrHandler( void );
static void Exti_Line15_IsrHandler( void );

/* ========================== SYMBOLIC CONSTANTS ============================ */

/** Value of major version of SW module */
#define EXTI_MAJOR_VERSION           ( 1u )

/** Value of minor version of SW module */
#define EXTI_MINOR_VERSION           ( 0u )

/** Value of patch version of SW module */
#define EXTI_PATCH_VERSION           ( 0u )

/** Default interrupt priority used by \ref Exti_Get_DefaultConfig */
#define EXTI_DEFAULT_IRQ_PRIO        ( 10u )

/** Maximum wait extie for configuration request confirmation */
#define EXTI_EXTIEOUT_RAW               ( 0x84FCB )

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

static volatile exti_IsrCallback_t              exti_UserCallback[ EXTI_PIN_CNT ] =
{
  { .ExtiPinId = EXTI_PIN_0  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_1  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_2  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_3  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_4  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_5  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_6  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_7  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_8  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_9  , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_10 , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_11 , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_12 , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_13 , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_14 , .extiIsrCallback = EXTI_NULL_PTR },
  { .ExtiPinId = EXTI_PIN_15 , .extiIsrCallback = EXTI_NULL_PTR },
};

/** External interrupt lines configuration array */
static exti_LinesConfigStruct_t const volatile  exti_PinConf[ EXTI_PIN_CNT ] =
{
  { .GpioPinId = GPIO_PIN_ID_0  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI0  , .ExtiLine = LL_EXTI_LINE_0  , .SysLine = LL_EXTI_EXTI_LINE0  , .NvicIsr = Exti_Line0_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_1  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI1  , .ExtiLine = LL_EXTI_LINE_1  , .SysLine = LL_EXTI_EXTI_LINE1  , .NvicIsr = Exti_Line1_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_2  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI2  , .ExtiLine = LL_EXTI_LINE_2  , .SysLine = LL_EXTI_EXTI_LINE2  , .NvicIsr = Exti_Line2_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_3  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI3  , .ExtiLine = LL_EXTI_LINE_3  , .SysLine = LL_EXTI_EXTI_LINE3  , .NvicIsr = Exti_Line3_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_4  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI4  , .ExtiLine = LL_EXTI_LINE_4  , .SysLine = LL_EXTI_EXTI_LINE4  , .NvicIsr = Exti_Line4_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_5  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI5  , .ExtiLine = LL_EXTI_LINE_5  , .SysLine = LL_EXTI_EXTI_LINE5  , .NvicIsr = Exti_Line5_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_6  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI6  , .ExtiLine = LL_EXTI_LINE_6  , .SysLine = LL_EXTI_EXTI_LINE6  , .NvicIsr = Exti_Line6_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_7  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI7  , .ExtiLine = LL_EXTI_LINE_7  , .SysLine = LL_EXTI_EXTI_LINE7  , .NvicIsr = Exti_Line7_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_8  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI8  , .ExtiLine = LL_EXTI_LINE_8  , .SysLine = LL_EXTI_EXTI_LINE8  , .NvicIsr = Exti_Line8_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_9  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9  , .ExtiLine = LL_EXTI_LINE_9  , .SysLine = LL_EXTI_EXTI_LINE9  , .NvicIsr = Exti_Line9_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_10 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI10 , .ExtiLine = LL_EXTI_LINE_10 , .SysLine = LL_EXTI_EXTI_LINE10 , .NvicIsr = Exti_Line10_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_11 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI11 , .ExtiLine = LL_EXTI_LINE_11 , .SysLine = LL_EXTI_EXTI_LINE11 , .NvicIsr = Exti_Line11_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_12 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI12 , .ExtiLine = LL_EXTI_LINE_12 , .SysLine = LL_EXTI_EXTI_LINE12 , .NvicIsr = Exti_Line12_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_13 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI13 , .ExtiLine = LL_EXTI_LINE_13 , .SysLine = LL_EXTI_EXTI_LINE13 , .NvicIsr = Exti_Line13_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_14 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI14 , .ExtiLine = LL_EXTI_LINE_14 , .SysLine = LL_EXTI_EXTI_LINE14 , .NvicIsr = Exti_Line14_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_15 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15 , .ExtiLine = LL_EXTI_LINE_15 , .SysLine = LL_EXTI_EXTI_LINE15 , .NvicIsr = Exti_Line15_IsrHandler },
};


static exti_PortConfigStruct_t const            exti_PortConf[ EXTI_PORT_CNT ] =
{
#if defined(GPIOA)
  { .ExtiPortId = EXTI_PORT_A , .GpioPortId = GPIO_PORT_A , .SysPortReg = LL_EXTI_EXTI_PORTA },
#endif
#if defined(GPIOB)
  { .ExtiPortId = EXTI_PORT_B , .GpioPortId = GPIO_PORT_B , .SysPortReg = LL_EXTI_EXTI_PORTB },
#endif
#if defined(GPIOC)
  { .ExtiPortId = EXTI_PORT_C , .GpioPortId = GPIO_PORT_C , .SysPortReg = LL_EXTI_EXTI_PORTC },
#endif
#if defined(GPIOD)
  { .ExtiPortId = EXTI_PORT_D , .GpioPortId = GPIO_PORT_D , .SysPortReg = LL_EXTI_EXTI_PORTD },
#endif
#if defined(GPIOE)
  { .ExtiPortId = EXTI_PORT_E , .GpioPortId = GPIO_PORT_E , .SysPortReg = LL_EXTI_EXTI_PORTE },
#endif
#if defined(GPIOF)
  { .ExtiPortId = EXTI_PORT_F , .GpioPortId = GPIO_PORT_F , .SysPortReg = LL_EXTI_EXTI_PORTF },
#endif
#if defined(GPIOG)
  { .ExtiPortId = EXTI_PORT_G , .GpioPortId = GPIO_PORT_G , .SysPortReg = LL_EXTI_EXTI_PORTG },
#endif
#if defined(GPIOH)
  { .ExtiPortId = EXTI_PORT_H , .GpioPortId = GPIO_PORT_H , .SysPortReg = LL_EXTI_EXTI_PORTH },
#endif
#if defined(GPIOI)
  { .ExtiPortId = EXTI_PORT_I , .GpioPortId = GPIO_PORT_I , .SysPortReg = LL_EXTI_EXTI_PORTI },
#endif
#if defined(GPIOJ)
  { .ExtiPortId = EXTI_PORT_J , .GpioPortId = GPIO_PORT_J , .SysPortReg = LL_EXTI_EXTI_PORTJ },
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
 * Configures the GPIO pin as input, connects it to the EXTI line, configures
 * trigger edges, clears pending flags of the line and enables the line
 * interrupt in EXTI and NVIC.
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

        /* -1- Configure GPIO pin as input */
        gpioRequestState = Gpio_Init( &gpioConfig );

        if( GPIO_REQUEST_OK == gpioRequestState )
        {
            /* Line interrupt is disabled during configuration */
            LL_EXTI_DisableIT_0_31( extiLine );

            /* Store ISR callback address */
            exti_UserCallback[ pinId ].extiIsrCallback = extiConfig->ExtiCallback;

            /* -2- Connect External Line to the GPIO */
            LL_EXTI_SetEXTISource( exti_PortConf[ extiConfig->ExtiPort ].SysPortReg, exti_PinConf[ pinId ].SysLine );

            /* -3- Configure trigger edges */
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

            /* -4- Clear pending flags of previous configuration and enable line interrupt */
            LL_EXTI_ClearFallingFlag_0_31( extiLine );
            LL_EXTI_ClearRisingFlag_0_31( extiLine );

            LL_EXTI_EnableIT_0_31( extiLine );

            /* -5- Configure NVIC */
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
 * Disables the line interrupt in NVIC and EXTI, disables both trigger edges,
 * clears pending flags, removes user callback and configures the pin as analog.
 * NVIC handler stays registered (NVIC module does not accept NULL handler), it
 * is not called while the NVIC line is disabled.
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
        const exti_PinId_t        pinId            = extiConfig->ExtiPin;
        const uint32_t            extiLine         = exti_PinConf[ pinId ].ExtiLine;
        gpio_RequestState_t       gpioRequestState = GPIO_REQUEST_ERROR;
        gpio_Config_t             gpioConfig       = { 0u };

        /* -1- Disable interrupt in NVIC */
        const nvic_RequestState_t nvicReq = Nvic_Set_PeriphIrq_Inactive( exti_PinConf[ pinId ].NvicIrqId );

        /* -2- Disable EXTI line interrupt and triggers, clear pending flags */
        LL_EXTI_DisableIT_0_31( extiLine );
        LL_EXTI_DisableFallingTrig_0_31( extiLine );
        LL_EXTI_DisableRisingTrig_0_31( extiLine );
        LL_EXTI_ClearFallingFlag_0_31( extiLine );
        LL_EXTI_ClearRisingFlag_0_31( extiLine );

        /* Remove ISR callback address */
        exti_UserCallback[ pinId ].extiIsrCallback = EXTI_NULL_PTR;

        /* -3- Configure GPIO pin as analog (default state) */
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
 * Reads rising / falling edge flags of the line, clears every active flag and
 * calls user callback with detected edge (both edges are handled if both flags
 * are active). Flag is cleared before the callback, so a new edge during the
 * callback is not lost. Flags are cleared also without registered callback.
 *
 * \param lineId [in]: External interrupt line identification, value from \ref exti_PinId_t.
 */
static inline void Exti_IsrHandler( exti_PinId_t lineId )
{
    const uint32_t                 extiLine         = exti_PinConf[ lineId ].ExtiLine;
    const uint32_t                 raisingFlagState = LL_EXTI_ReadRisingFlag_0_31( extiLine );
    const uint32_t                 fallingFlagState = LL_EXTI_ReadFallingFlag_0_31( extiLine );
    exti_ExtiIsrCallback_t * const userCallback     = exti_UserCallback[ lineId ].extiIsrCallback;

    if( 0u != raisingFlagState )
    {
        LL_EXTI_ClearRisingFlag_0_31( extiLine );

        if( EXTI_NULL_PTR != userCallback )
        {
            userCallback( EXTI_TRIGGER_EDGE_RAISING );
        }
        else
        {
            /* No user callback registered */
        }
    }
    else
    {
        /* Rising edge not detected */
    }

    if( 0u != fallingFlagState )
    {
        LL_EXTI_ClearFallingFlag_0_31( extiLine );

        if( EXTI_NULL_PTR != userCallback )
        {
            userCallback( EXTI_TRIGGER_EDGE_FALLING );
        }
        else
        {
            /* No user callback registered */
        }
    }
    else
    {
        /* Falling edge not detected */
    }
}

/* =========================== INTERRUPT HANDLERS =========================== */

/**
 * \brief External interrupt line 0 interrupt service routine
 */
static void Exti_Line0_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_0 );
}


/**
 * \brief External interrupt line 1 interrupt service routine
 */
static void Exti_Line1_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_1 );
}


/**
 * \brief External interrupt line 2 interrupt service routine
 */
static void Exti_Line2_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_2 );
}


/**
 * \brief External interrupt line 3 interrupt service routine
 */
static void Exti_Line3_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_3 );
}


/**
 * \brief External interrupt line 4 interrupt service routine
 */
static void Exti_Line4_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_4 );
}


/**
 * \brief External interrupt line 5 interrupt service routine
 */
static void Exti_Line5_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_5 );
}

/**
 * \brief External interrupt line 6 interrupt service routine
 */
static void Exti_Line6_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_6 );
}


/**
 * \brief External interrupt line 7 interrupt service routine
 */
static void Exti_Line7_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_7 );
}


/**
 * \brief External interrupt line 8 interrupt service routine
 */
static void Exti_Line8_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_8 );
}


/**
 * \brief External interrupt line 9 interrupt service routine
 */
static void Exti_Line9_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_9 );
}


/**
 * \brief External interrupt line 10 interrupt service routine
 */
static void Exti_Line10_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_10 );
}


/**
 * \brief External interrupt line 11 interrupt service routine
 */
static void Exti_Line11_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_11 );
}


/**
 * \brief External interrupt line 12 interrupt service routine
 */
static void Exti_Line12_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_12 );
}


/**
 * \brief External interrupt line 13 interrupt service routine
 */
static void Exti_Line13_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_13 );
}


/**
 * \brief External interrupt line 14 interrupt service routine
 */
static void Exti_Line14_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_14 );
}


/**
 * \brief External interrupt line 15 interrupt service routine
 */
static void Exti_Line15_IsrHandler( void )
{
    Exti_IsrHandler( EXTI_PIN_15 );
}



/* ================================ TASKS =================================== */
