/**
 * \author Mr.Nobody
 * \file Exti.c
 * \ingroup Exti
 * \brief External Interrupt module common functionality
 *
 * GPIO lines 0 .. 15 (\ref Exti_Init, \ref Exti_Deinit) and peripheral event
 * lines 16 .. 66 (\ref Exti_Set_LineActive, \ref Exti_Set_LineInactive,
 * \ref Exti_Get_LineType, \ref Exti_Get_LinePending,
 * \ref Exti_Set_LinePendingClear) of the STM32H5 EXTI block.
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
#include <stddef.h>                         /* Table sizes (size_t)           */
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


/** \brief Register group of a peripheral event line (register set of the EXTI block) */
typedef enum
{
    EXTI_LINE_REG_GROUP_0_31 = 0u, /**< Lines 0 .. 31: IMR1, EMR1, RTSR1, FTSR1, RPR1, FPR1   */
    EXTI_LINE_REG_GROUP_32_63,     /**< Lines 32 .. 63: IMR2, EMR2, RTSR2, FTSR2, RPR2, FPR2  */
    EXTI_LINE_REG_GROUP_64_95      /**< Lines 64 .. 95: IMR3, EMR3, RTSR3, FTSR3, RPR3, FPR3  */
}   exti_LineRegGroup_t;


/** \brief Peripheral event line description (one row of the line table) */
typedef struct
{
    exti_LineId_t       LineId;   /**< Line identification (equal to the row index)        */
    uint32_t            LineMask; /**< Bit mask of the line in the registers of the group  */
    exti_LineRegGroup_t RegGroup; /**< Register group of the line                          */
    exti_LineType_t     LineType; /**< Configurable or direct line                         */
}   exti_LineConfStruct_t;

/* ======================== FORWARD DECLARATIONS ============================ */

static inline void Exti_IsrHandler( exti_PinId_t lineId );

static const exti_LineConfStruct_t * Exti_Get_LineConf        ( exti_LineId_t lineId );
static void                          Exti_Line_Set_Masks      ( const exti_LineConfStruct_t * const lineConf,
                                                                exti_FunctionState_t itState,
                                                                exti_FunctionState_t eventState );
static void                          Exti_Line_Set_Triggers   ( const exti_LineConfStruct_t * const lineConf,
                                                                exti_TriggerEdge_t triggerEdge );
static void                          Exti_Line_Set_PendingClear( const exti_LineConfStruct_t * const lineConf );
static exti_TriggerEdge_t            Exti_Line_Get_Pending    ( const exti_LineConfStruct_t * const lineConf );

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
#define EXTI_MINOR_VERSION           ( 1u )

/** Value of patch version of SW module */
#define EXTI_PATCH_VERSION           ( 0u )

/** Default interrupt priority used by \ref Exti_Get_DefaultConfig */
#define EXTI_DEFAULT_IRQ_PRIO        ( 10u )

/** Maximum wait extie for configuration request confirmation */
#define EXTI_EXTIEOUT_RAW               ( 0x84FCB )

/**
 * Type of line 49 (LPTIM2 channel 1): configurable on the devices with the trigger
 * bit EXTI_RTSR2_RT49 (STM32H543 / H553, STM32H5Ex / H5Fx), direct elsewhere
 * (RM0481 Rev 5 Table 157 note 4, RM0517 Rev 1 Table 169, RM0492 Rev 3 Table 100).
 */
#if defined(EXTI_RTSR2_RT49)
#define EXTI_LINE_49_TYPE            ( EXTI_LINE_TYPE_CONFIGURABLE )
#else
#define EXTI_LINE_49_TYPE            ( EXTI_LINE_TYPE_DIRECT )
#endif

/* =============================== MACROS =================================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* =========================== LOCAL VARIABLES ============================== */

static volatile exti_IsrCallback_t              exti_UserCallback[] =
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

_Static_assert( ( ( sizeof( exti_UserCallback ) / sizeof( exti_UserCallback[ 0u ] ) ) == ( (size_t)EXTI_PIN_CNT) ), "User callback table does not match exti_PinId_t" );


/** External interrupt lines configuration array */
static exti_LinesConfigStruct_t const volatile  exti_PinConf[] =
{
  { .GpioPinId = GPIO_PIN_ID_0  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI0  , .ExtiLine = LL_EXTI_LINE_0  , .SysLine = LL_EXTI_EXTI_LINE0,  .NvicIsr = Exti_Line0_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_1  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI1  , .ExtiLine = LL_EXTI_LINE_1  , .SysLine = LL_EXTI_EXTI_LINE1,  .NvicIsr = Exti_Line1_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_2  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI2  , .ExtiLine = LL_EXTI_LINE_2  , .SysLine = LL_EXTI_EXTI_LINE2,  .NvicIsr = Exti_Line2_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_3  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI3  , .ExtiLine = LL_EXTI_LINE_3  , .SysLine = LL_EXTI_EXTI_LINE3,  .NvicIsr = Exti_Line3_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_4  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI4  , .ExtiLine = LL_EXTI_LINE_4  , .SysLine = LL_EXTI_EXTI_LINE4,  .NvicIsr = Exti_Line4_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_5  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI5  , .ExtiLine = LL_EXTI_LINE_5  , .SysLine = LL_EXTI_EXTI_LINE5,  .NvicIsr = Exti_Line5_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_6  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI6  , .ExtiLine = LL_EXTI_LINE_6  , .SysLine = LL_EXTI_EXTI_LINE6,  .NvicIsr = Exti_Line6_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_7  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI7  , .ExtiLine = LL_EXTI_LINE_7  , .SysLine = LL_EXTI_EXTI_LINE7,  .NvicIsr = Exti_Line7_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_8  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI8  , .ExtiLine = LL_EXTI_LINE_8  , .SysLine = LL_EXTI_EXTI_LINE8,  .NvicIsr = Exti_Line8_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_9  , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI9  , .ExtiLine = LL_EXTI_LINE_9  , .SysLine = LL_EXTI_EXTI_LINE9,  .NvicIsr = Exti_Line9_IsrHandler  },
  { .GpioPinId = GPIO_PIN_ID_10 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI10 , .ExtiLine = LL_EXTI_LINE_10 , .SysLine = LL_EXTI_EXTI_LINE10, .NvicIsr = Exti_Line10_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_11 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI11 , .ExtiLine = LL_EXTI_LINE_11 , .SysLine = LL_EXTI_EXTI_LINE11, .NvicIsr = Exti_Line11_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_12 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI12 , .ExtiLine = LL_EXTI_LINE_12 , .SysLine = LL_EXTI_EXTI_LINE12, .NvicIsr = Exti_Line12_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_13 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI13 , .ExtiLine = LL_EXTI_LINE_13 , .SysLine = LL_EXTI_EXTI_LINE13, .NvicIsr = Exti_Line13_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_14 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI14 , .ExtiLine = LL_EXTI_LINE_14 , .SysLine = LL_EXTI_EXTI_LINE14, .NvicIsr = Exti_Line14_IsrHandler },
  { .GpioPinId = GPIO_PIN_ID_15 , .NvicIrqId = NVIC_PERIPH_IRQ_EXTI15 , .ExtiLine = LL_EXTI_LINE_15 , .SysLine = LL_EXTI_EXTI_LINE15, .NvicIsr = Exti_Line15_IsrHandler },
};

_Static_assert( ( ( sizeof( exti_PinConf ) / sizeof( exti_PinConf[ 0u ] ) ) == ( (size_t)EXTI_PIN_CNT) ), "Peripheral pin lines table does not match exti_PinId_t" );


static exti_PortConfigStruct_t const            exti_PortConf[] =
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

_Static_assert( ( ( sizeof( exti_PortConf ) / sizeof( exti_PortConf[ 0u ] ) ) == ( (size_t)EXTI_PORT_CNT) ), "Peripheral port lines table does not match exti_PortId_t" );


/**
 * Peripheral event lines table - the rows follow \ref exti_LineId_t (same guards, same
 * order), the size is checked against EXTI_LINE_CNT at compile time and the row index
 * against the LineId at run time.
 */
static const exti_LineConfStruct_t              exti_LineConf[] =
{
  { .LineId   = EXTI_LINE_PVD_AVD         , .LineMask = LL_EXTI_LINE_16 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
  { .LineId   = EXTI_LINE_RTC             , .LineMask = LL_EXTI_LINE_17 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(EXTI_IMR1_IM18)
  { .LineId   = EXTI_LINE_RTC_S           , .LineMask = LL_EXTI_LINE_18 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_TAMP            , .LineMask = LL_EXTI_LINE_19 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(EXTI_IMR1_IM20)
  { .LineId   = EXTI_LINE_TAMP_S          , .LineMask = LL_EXTI_LINE_20 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_I2C1_WKUP       , .LineMask = LL_EXTI_LINE_21 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_I2C2_WKUP       , .LineMask = LL_EXTI_LINE_22 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(EXTI_IMR1_IM23)
  { .LineId   = EXTI_LINE_I2C3_WKUP       , .LineMask = LL_EXTI_LINE_23 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_I3C1_WKUP       , .LineMask = LL_EXTI_LINE_24 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_USART1_WKUP     , .LineMask = LL_EXTI_LINE_25 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_USART2_WKUP     , .LineMask = LL_EXTI_LINE_26 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_USART3_WKUP     , .LineMask = LL_EXTI_LINE_27 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(STM32H503xx)
  { .LineId   = EXTI_LINE_I3C2_WKUP       , .LineMask = LL_EXTI_LINE_28 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_COMP1           , .LineMask = LL_EXTI_LINE_29 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#else
  { .LineId   = EXTI_LINE_UART4_WKUP      , .LineMask = LL_EXTI_LINE_28 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_UART5_WKUP      , .LineMask = LL_EXTI_LINE_29 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR1_IM30)
  { .LineId   = EXTI_LINE_USART6_WKUP     , .LineMask = LL_EXTI_LINE_30 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR1_IM31)
  { .LineId   = EXTI_LINE_UART7_WKUP      , .LineMask = LL_EXTI_LINE_31 , .RegGroup = EXTI_LINE_REG_GROUP_0_31  , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM32)
  { .LineId   = EXTI_LINE_UART8_WKUP      , .LineMask = LL_EXTI_LINE_32 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM33)
  { .LineId   = EXTI_LINE_UART9_WKUP      , .LineMask = LL_EXTI_LINE_33 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM34)
  { .LineId   = EXTI_LINE_USART10_WKUP    , .LineMask = LL_EXTI_LINE_34 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM35)
  { .LineId   = EXTI_LINE_USART11_WKUP    , .LineMask = LL_EXTI_LINE_35 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM36)
  { .LineId   = EXTI_LINE_UART12_WKUP     , .LineMask = LL_EXTI_LINE_36 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_LPUART1_WKUP    , .LineMask = LL_EXTI_LINE_37 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_LPTIM1          , .LineMask = LL_EXTI_LINE_38 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_LPTIM2          , .LineMask = LL_EXTI_LINE_39 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_SPI1_WKUP       , .LineMask = LL_EXTI_LINE_40 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_SPI2_WKUP       , .LineMask = LL_EXTI_LINE_41 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
  { .LineId   = EXTI_LINE_SPI3_WKUP       , .LineMask = LL_EXTI_LINE_42 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(EXTI_IMR2_IM43)
  { .LineId   = EXTI_LINE_SPI4_WKUP       , .LineMask = LL_EXTI_LINE_43 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM44)
  { .LineId   = EXTI_LINE_SPI5_WKUP       , .LineMask = LL_EXTI_LINE_44 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM45)
  { .LineId   = EXTI_LINE_SPI6_WKUP       , .LineMask = LL_EXTI_LINE_45 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(ETH)
  { .LineId   = EXTI_LINE_ETH_WKUP        , .LineMask = LL_EXTI_LINE_46 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#endif
  { .LineId   = EXTI_LINE_USB_WKUP        , .LineMask = LL_EXTI_LINE_47 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#if defined(EXTI_IMR2_IM48)
  { .LineId   = EXTI_LINE_UCPD1_WKUP      , .LineMask = LL_EXTI_LINE_48 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_LPTIM2_CH1      , .LineMask = LL_EXTI_LINE_49 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_49_TYPE           },
  { .LineId   = EXTI_LINE_DTS_WKUP        , .LineMask = LL_EXTI_LINE_50 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#if defined(EXTI_IMR2_IM51)
  { .LineId   = EXTI_LINE_CEC_WKUP        , .LineMask = LL_EXTI_LINE_51 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM52)
  { .LineId   = EXTI_LINE_I2C4_WKUP       , .LineMask = LL_EXTI_LINE_52 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
  { .LineId   = EXTI_LINE_UVM             , .LineMask = LL_EXTI_LINE_53 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#if defined(EXTI_IMR2_IM54)
  { .LineId   = EXTI_LINE_LPTIM3          , .LineMask = LL_EXTI_LINE_54 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM55)
  { .LineId   = EXTI_LINE_LPTIM4          , .LineMask = LL_EXTI_LINE_55 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM56)
  { .LineId   = EXTI_LINE_LPTIM5          , .LineMask = LL_EXTI_LINE_56 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM57)
  { .LineId   = EXTI_LINE_LPTIM6          , .LineMask = LL_EXTI_LINE_57 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM58)
#if defined(EXTI_RTSR2_RT58)
  { .LineId   = EXTI_LINE_COMP1           , .LineMask = LL_EXTI_LINE_58 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#else
  { .LineId   = EXTI_LINE_I3C2_WKUP       , .LineMask = LL_EXTI_LINE_58 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#endif
#if defined(EXTI_IMR2_IM59)
  { .LineId   = EXTI_LINE_COMP2           , .LineMask = LL_EXTI_LINE_59 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#endif
#if defined(EXTI_IMR2_IM60) && \
    defined(USB_OTG_HS)
  { .LineId   = EXTI_LINE_USB_OTG_HS_WKUP , .LineMask = LL_EXTI_LINE_60 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM61)
  { .LineId   = EXTI_LINE_I3C2_WKUP       , .LineMask = LL_EXTI_LINE_61 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM62)
  { .LineId   = EXTI_LINE_ADF1            , .LineMask = LL_EXTI_LINE_62 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR2_IM63)
  { .LineId   = EXTI_LINE_MDF1            , .LineMask = LL_EXTI_LINE_63 , .RegGroup = EXTI_LINE_REG_GROUP_32_63 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR3_IM64)
  { .LineId   = EXTI_LINE_PLAY1_OUT14     , .LineMask = LL_EXTI_LINE_64 , .RegGroup = EXTI_LINE_REG_GROUP_64_95 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#endif
#if defined(EXTI_IMR3_IM65)
  { .LineId   = EXTI_LINE_IWDG            , .LineMask = LL_EXTI_LINE_65 , .RegGroup = EXTI_LINE_REG_GROUP_64_95 , .LineType = EXTI_LINE_TYPE_DIRECT       },
#endif
#if defined(EXTI_IMR3_IM66)
  { .LineId   = EXTI_LINE_VDDIO2_VM       , .LineMask = LL_EXTI_LINE_66 , .RegGroup = EXTI_LINE_REG_GROUP_64_95 , .LineType = EXTI_LINE_TYPE_CONFIGURABLE },
#endif
};

_Static_assert( ( ( sizeof( exti_LineConf ) / sizeof( exti_LineConf[ 0u ] ) ) == ( (size_t)EXTI_LINE_CNT) ), "Peripheral event lines table does not match exti_LineId_t" );

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
            nvicReq = Nvic_Set_PeriphIrq_Prio( exti_PinConf[ pinId ].NvicIrqId,
                                               (nvic_IrqPrio_t)extiConfig->ExtiPriority );
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


/**
 * \brief Activates a peripheral event line (lines 16 .. 66).
 *
 * Enables the line as wake-up with interrupt, event or both. A configurable line
 * requires a trigger edge: both masks are disabled, the rising / falling
 * triggers are set as requested and then the masks are enabled. A direct line
 * requires \ref EXTI_TRIGGER_EDGE_NONE (it has no trigger configuration), only
 * the masks are set. The parameters and the combination of the line type and
 * the trigger edge are checked before any register is written. The NVIC vector
 * of the line belongs to the module of the peripheral, it is not configured
 * here. The EXTI block needs no clock enable.
 *
 * \note Pending flags of a configurable line are not touched (a line pending
 *       from before the activation stays pending) - \ref Exti_Set_LinePendingClear
 *       is the only function that clears them.
 *
 * \param lineId     [in]: Peripheral event line, value from \ref exti_LineId_t.
 * \param lineConfig [in]: Pointer to the line configuration. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Set_LineActive( exti_LineId_t lineId, const exti_LineConfig_t * const lineConfig )
{
    exti_RequestState_t                 retState = EXTI_REQUEST_ERROR;
    const exti_LineConfStruct_t * const lineConf = Exti_Get_LineConf( lineId );

    if( ( EXTI_NULL_PTR          != lineConf                ) &&
        ( EXTI_NULL_PTR          != lineConfig              ) &&
        ( EXTI_LINE_MODE_BOTH    >= lineConfig->LineMode    ) &&
        ( EXTI_TRIGGER_EDGE_NONE >= lineConfig->TriggerEdge )    )
    {
        const exti_TriggerEdge_t triggerEdge = lineConfig->TriggerEdge;
        exti_FunctionState_t     itState     = EXTI_FUNCTION_INACTIVE;
        exti_FunctionState_t     eventState  = EXTI_FUNCTION_INACTIVE;

        if( EXTI_LINE_TYPE_CONFIGURABLE == lineConf->LineType )
        {
            if( EXTI_TRIGGER_EDGE_NONE != triggerEdge )
            {
                /* -1- Masks are disabled during the trigger configuration */
                Exti_Line_Set_Masks( lineConf, EXTI_FUNCTION_INACTIVE, EXTI_FUNCTION_INACTIVE );

                /* -2- Configure trigger edges, pending flags are not touched */
                Exti_Line_Set_Triggers( lineConf, triggerEdge );

                retState = EXTI_REQUEST_OK;
            }
            else
            {
                /* A configurable line requires a trigger edge */
            }
        }
        else
        {
            if( EXTI_TRIGGER_EDGE_NONE == triggerEdge )
            {
                retState = EXTI_REQUEST_OK;
            }
            else
            {
                /* A direct line has no trigger edge configuration */
            }
        }

        if( EXTI_REQUEST_OK == retState )
        {
            if( EXTI_LINE_MODE_EVENT != lineConfig->LineMode )
            {
                itState = EXTI_FUNCTION_ACTIVE;
            }
            else
            {
                /* Interrupt is not requested */
            }

            if( EXTI_LINE_MODE_INTERRUPT != lineConfig->LineMode )
            {
                eventState = EXTI_FUNCTION_ACTIVE;
            }
            else
            {
                /* Event is not requested */
            }

            /* -3- Enable the requested masks */
            Exti_Line_Set_Masks( lineConf, itState, eventState );
        }
        else
        {
            /* Trigger edge does not match the line type, no register access */
        }
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Deactivates a peripheral event line (lines 16 .. 66).
 *
 * Masks the interrupt and the event of the line; triggers of a configurable
 * line are cleared. A direct line has no triggers in the EXTI, only the masks
 * are cleared. Pending flags are not touched in either case - \ref
 * Exti_Set_LinePendingClear is the only function that clears them.
 *
 * \param lineId [in]: Peripheral event line, value from \ref exti_LineId_t.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Set_LineInactive( exti_LineId_t lineId )
{
    exti_RequestState_t                 retState = EXTI_REQUEST_ERROR;
    const exti_LineConfStruct_t * const lineConf = Exti_Get_LineConf( lineId );

    if( EXTI_NULL_PTR != lineConf )
    {
        Exti_Line_Set_Masks( lineConf, EXTI_FUNCTION_INACTIVE, EXTI_FUNCTION_INACTIVE );

        if( EXTI_LINE_TYPE_CONFIGURABLE == lineConf->LineType )
        {
            Exti_Line_Set_Triggers( lineConf, EXTI_TRIGGER_EDGE_NONE );
        }
        else
        {
            /* A direct line has no triggers */
        }

        retState = EXTI_REQUEST_OK;
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Clears the rising and falling pending flags of a configurable peripheral event line.
 *
 * A direct line has no pending flag in the EXTI (the status is in the
 * peripheral), the request is rejected without a register access.
 *
 * \param lineId [in]: Peripheral event line, value from \ref exti_LineId_t.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Set_LinePendingClear( exti_LineId_t lineId )
{
    exti_RequestState_t                 retState = EXTI_REQUEST_ERROR;
    const exti_LineConfStruct_t * const lineConf = Exti_Get_LineConf( lineId );

    if( EXTI_NULL_PTR != lineConf )
    {
        if( EXTI_LINE_TYPE_CONFIGURABLE == lineConf->LineType )
        {
            Exti_Line_Set_PendingClear( lineConf );

            retState = EXTI_REQUEST_OK;
        }
        else
        {
            /* A direct line has no pending flags */
        }
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reports the type of a peripheral event line.
 *
 * No register is accessed.
 *
 * \param lineId   [in]:  Peripheral event line, value from \ref exti_LineId_t.
 * \param lineType [out]: Pointer to the line type. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Get_LineType( exti_LineId_t lineId, exti_LineType_t * const lineType )
{
    exti_RequestState_t                 retState = EXTI_REQUEST_ERROR;
    const exti_LineConfStruct_t * const lineConf = Exti_Get_LineConf( lineId );

    if( ( EXTI_NULL_PTR != lineConf ) &&
        ( EXTI_NULL_PTR != lineType )    )
    {
        *lineType = lineConf->LineType;

        retState = EXTI_REQUEST_OK;
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}


/**
 * \brief Reads the pending edge(s) of a configurable peripheral event line.
 *
 * Reads the rising / falling pending flags of the line without clearing them
 * (\ref Exti_Set_LinePendingClear clears them). A direct line has no pending
 * flag in the EXTI, the request is rejected without a register access.
 *
 * \param lineId      [in]:  Peripheral event line, value from \ref exti_LineId_t.
 * \param pendingEdge [out]: Pointer to the pending edge: \ref EXTI_TRIGGER_EDGE_RAISING,
 *                           \ref EXTI_TRIGGER_EDGE_FALLING, \ref EXTI_TRIGGER_EDGE_BOTH or
 *                           \ref EXTI_TRIGGER_EDGE_NONE when no edge is pending. Must not be NULL.
 *
 * \return State of request execution. Returns \ref EXTI_REQUEST_OK if request was
 *         success, otherwise returns \ref EXTI_REQUEST_ERROR.
 */
exti_RequestState_t Exti_Get_LinePending( exti_LineId_t lineId, exti_TriggerEdge_t * const pendingEdge )
{
    exti_RequestState_t                 retState = EXTI_REQUEST_ERROR;
    const exti_LineConfStruct_t * const lineConf = Exti_Get_LineConf( lineId );

    if( ( EXTI_NULL_PTR != lineConf    ) &&
        ( EXTI_NULL_PTR != pendingEdge )    )
    {
        if( EXTI_LINE_TYPE_CONFIGURABLE == lineConf->LineType )
        {
            *pendingEdge = Exti_Line_Get_Pending( lineConf );

            retState = EXTI_REQUEST_OK;
        }
        else
        {
            /* A direct line has no pending flags */
        }
    }
    else
    {
        retState = EXTI_REQUEST_ERROR;
    }

    return ( retState );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/**
 * \brief Returns the table row of a peripheral event line.
 *
 * The line identification is checked against the table size and against the
 * LineId stored in the row, so no register of another line can be accessed.
 *
 * \param lineId [in]: Peripheral event line, value from \ref exti_LineId_t.
 *
 * \return Pointer to the table row, NULL for an unknown line.
 */
static const exti_LineConfStruct_t * Exti_Get_LineConf( exti_LineId_t lineId )
{
    const exti_LineConfStruct_t * lineConf = EXTI_NULL_PTR;

    if( EXTI_LINE_CNT > lineId )
    {
        if( lineId == exti_LineConf[ lineId ].LineId )
        {
            lineConf = &exti_LineConf[ lineId ];
        }
        else
        {
            /* Table row does not match the line identification */
        }
    }
    else
    {
        /* Line identification out of range */
    }

    return ( lineConf );
}


/**
 * \brief Sets the interrupt mask (IMRx) and the event mask (EMRx) of a peripheral event line.
 *
 * \param lineConf   [in]: Table row of the line.
 * \param itState    [in]: Required state of the interrupt mask.
 * \param eventState [in]: Required state of the event mask.
 */
static void Exti_Line_Set_Masks( const exti_LineConfStruct_t * const lineConf,
                                 exti_FunctionState_t                itState,
                                 exti_FunctionState_t                eventState )
{
    const uint32_t lineMask = lineConf->LineMask;

    if( EXTI_LINE_REG_GROUP_0_31 == lineConf->RegGroup )
    {
        if( EXTI_FUNCTION_ACTIVE == itState )
        {
            LL_EXTI_EnableIT_0_31( lineMask );
        }
        else
        {
            LL_EXTI_DisableIT_0_31( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == eventState )
        {
            LL_EXTI_EnableEvent_0_31( lineMask );
        }
        else
        {
            LL_EXTI_DisableEvent_0_31( lineMask );
        }
    }
    else if( EXTI_LINE_REG_GROUP_32_63 == lineConf->RegGroup )
    {
        if( EXTI_FUNCTION_ACTIVE == itState )
        {
            LL_EXTI_EnableIT_32_63( lineMask );
        }
        else
        {
            LL_EXTI_DisableIT_32_63( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == eventState )
        {
            LL_EXTI_EnableEvent_32_63( lineMask );
        }
        else
        {
            LL_EXTI_DisableEvent_32_63( lineMask );
        }
    }
    else
    {
#if defined(EXTI_IMR3_IM)
        if( EXTI_FUNCTION_ACTIVE == itState )
        {
            LL_EXTI_EnableIT_64_95( lineMask );
        }
        else
        {
            LL_EXTI_DisableIT_64_95( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == eventState )
        {
            LL_EXTI_EnableEvent_64_95( lineMask );
        }
        else
        {
            LL_EXTI_DisableEvent_64_95( lineMask );
        }
#else
        /* Lines 64 .. 95 are not present on the device */
#endif
    }
}


/**
 * \brief Sets the rising (RTSRx) and falling (FTSRx) triggers of a configurable peripheral event line.
 *
 * \param lineConf    [in]: Table row of the line.
 * \param triggerEdge [in]: Required trigger edge(s), \ref EXTI_TRIGGER_EDGE_NONE disables both.
 */
static void Exti_Line_Set_Triggers( const exti_LineConfStruct_t * const lineConf, exti_TriggerEdge_t triggerEdge )
{
    const uint32_t       lineMask     = lineConf->LineMask;
    exti_FunctionState_t risingState  = EXTI_FUNCTION_INACTIVE;
    exti_FunctionState_t fallingState = EXTI_FUNCTION_INACTIVE;

    if( ( EXTI_TRIGGER_EDGE_RAISING == triggerEdge ) ||
        ( EXTI_TRIGGER_EDGE_BOTH    == triggerEdge )    )
    {
        risingState = EXTI_FUNCTION_ACTIVE;
    }
    else
    {
        /* Rising edge is not used */
    }

    if( ( EXTI_TRIGGER_EDGE_FALLING == triggerEdge ) ||
        ( EXTI_TRIGGER_EDGE_BOTH    == triggerEdge )    )
    {
        fallingState = EXTI_FUNCTION_ACTIVE;
    }
    else
    {
        /* Falling edge is not used */
    }

    if( EXTI_LINE_REG_GROUP_0_31 == lineConf->RegGroup )
    {
        if( EXTI_FUNCTION_ACTIVE == risingState )
        {
            LL_EXTI_EnableRisingTrig_0_31( lineMask );
        }
        else
        {
            LL_EXTI_DisableRisingTrig_0_31( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == fallingState )
        {
            LL_EXTI_EnableFallingTrig_0_31( lineMask );
        }
        else
        {
            LL_EXTI_DisableFallingTrig_0_31( lineMask );
        }
    }
    else if( EXTI_LINE_REG_GROUP_32_63 == lineConf->RegGroup )
    {
        if( EXTI_FUNCTION_ACTIVE == risingState )
        {
            LL_EXTI_EnableRisingTrig_32_63( lineMask );
        }
        else
        {
            LL_EXTI_DisableRisingTrig_32_63( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == fallingState )
        {
            LL_EXTI_EnableFallingTrig_32_63( lineMask );
        }
        else
        {
            LL_EXTI_DisableFallingTrig_32_63( lineMask );
        }
    }
    else
    {
#if defined(EXTI_IMR3_IM)
        if( EXTI_FUNCTION_ACTIVE == risingState )
        {
            LL_EXTI_EnableRisingTrig_64_95( lineMask );
        }
        else
        {
            LL_EXTI_DisableRisingTrig_64_95( lineMask );
        }

        if( EXTI_FUNCTION_ACTIVE == fallingState )
        {
            LL_EXTI_EnableFallingTrig_64_95( lineMask );
        }
        else
        {
            LL_EXTI_DisableFallingTrig_64_95( lineMask );
        }
#else
        /* Lines 64 .. 95 are not present on the device */
#endif
    }
}


/**
 * \brief Clears the rising (RPRx) and falling (FPRx) pending flags of a configurable peripheral event line.
 *
 * \param lineConf [in]: Table row of the line.
 */
static void Exti_Line_Set_PendingClear( const exti_LineConfStruct_t * const lineConf )
{
    const uint32_t lineMask = lineConf->LineMask;

    if( EXTI_LINE_REG_GROUP_0_31 == lineConf->RegGroup )
    {
        LL_EXTI_ClearRisingFlag_0_31( lineMask );
        LL_EXTI_ClearFallingFlag_0_31( lineMask );
    }
    else if( EXTI_LINE_REG_GROUP_32_63 == lineConf->RegGroup )
    {
        LL_EXTI_ClearRisingFlag_32_63( lineMask );
        LL_EXTI_ClearFallingFlag_32_63( lineMask );
    }
    else
    {
#if defined(EXTI_IMR3_IM)
        LL_EXTI_ClearRisingFlag_64_95( lineMask );
        LL_EXTI_ClearFallingFlag_64_95( lineMask );
#else
        /* Lines 64 .. 95 are not present on the device */
#endif
    }
}


/**
 * \brief Reads the pending flags of a configurable peripheral event line.
 *
 * \param lineConf [in]: Table row of the line.
 *
 * \return Pending edge(s), \ref EXTI_TRIGGER_EDGE_NONE when no flag is active.
 */
static exti_TriggerEdge_t Exti_Line_Get_Pending( const exti_LineConfStruct_t * const lineConf )
{
    const uint32_t     lineMask    = lineConf->LineMask;
    exti_TriggerEdge_t pendingEdge = EXTI_TRIGGER_EDGE_NONE;
    uint32_t           risingFlag  = 0u;
    uint32_t           fallingFlag = 0u;

    if( EXTI_LINE_REG_GROUP_0_31 == lineConf->RegGroup )
    {
        risingFlag  = LL_EXTI_IsActiveRisingFlag_0_31( lineMask );
        fallingFlag = LL_EXTI_IsActiveFallingFlag_0_31( lineMask );
    }
    else if( EXTI_LINE_REG_GROUP_32_63 == lineConf->RegGroup )
    {
        risingFlag  = LL_EXTI_IsActiveRisingFlag_32_63( lineMask );
        fallingFlag = LL_EXTI_IsActiveFallingFlag_32_63( lineMask );
    }
    else
    {
#if defined(EXTI_IMR3_IM)
        risingFlag  = LL_EXTI_IsActiveRisingFlag_64_95( lineMask );
        fallingFlag = LL_EXTI_IsActiveFallingFlag_64_95( lineMask );
#else
        /* Lines 64 .. 95 are not present on the device */
#endif
    }

    if( ( 0u != risingFlag  ) &&
        ( 0u != fallingFlag )    )
    {
        pendingEdge = EXTI_TRIGGER_EDGE_BOTH;
    }
    else if( 0u != risingFlag )
    {
        pendingEdge = EXTI_TRIGGER_EDGE_RAISING;
    }
    else if( 0u != fallingFlag )
    {
        pendingEdge = EXTI_TRIGGER_EDGE_FALLING;
    }
    else
    {
        pendingEdge = EXTI_TRIGGER_EDGE_NONE;
    }

    return ( pendingEdge );
}


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
