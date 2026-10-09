/**
 * \author Mr.Nobody
 * \file Test_Exti.c
 * \ingroup Exti
 * \brief Unit tests of External Interrupt (EXTI) module.
 *
 * Exti.c is compiled unchanged with real LL drivers. EXTI and SYSCFG registers
 * are emulated by RegMem, RCC, GPIO and NVIC modules are mocked by CMock. Line
 * ISR registered in NVIC is captured by stub and called directly to test
 * interrupt handling.
 *
 * \note Pending register PR is write-1-to-clear on HW. Emulated register keeps
 *       written value, tests check that only the line bit was written.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "CmsisHost.h"                      /* Core intrinsics emulation      */
#include "Exti_Port.h"                      /* Module under test              */
#include "MockRcc_Port.h"                   /* RCC module mock                */
#include "MockGpio_Port.h"                  /* GPIO module mock               */
#include "MockNvic_Port.h"                  /* NVIC module mock               */
#include "Stm32_exti.h"                     /* EXTI registers definition      */
#include "Stm32_system.h"                   /* SYSCFG registers definition    */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static nvic_RequestState_t  Ut_Exti_NvicSetHandlerStub  ( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int callCnt );
static void                 Ut_Exti_UserCallback        ( exti_TriggerEdge_t edge );
static void                 Ut_Exti_OtherCallback       ( exti_TriggerEdge_t edge );
static exti_PeriphConfig_t  Ut_Exti_Get_Config          ( exti_PinId_t pinId, exti_TriggerEdge_t edge );
static nvic_PeriphIrqList_t Ut_Exti_Get_IrqId           ( exti_PinId_t pinId );
static uint32_t             Ut_Exti_Get_ExtiCrField     ( exti_PinId_t pinId );
static void                 Ut_Exti_Call_Isr            ( exti_PinId_t pinId );
static void                 Ut_Exti_Expect_Init         ( exti_PinId_t pinId, gpio_PortId_t gpioPort, gpio_PinPullCfg_t pull, gpio_PinSpeed_t speed, exti_IrqPrio_t prio );
static void                 Ut_Exti_Expect_PinLevel     ( gpio_PortId_t gpioPort, exti_PinId_t pinId, gpio_PinLevel_t pinLevel );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Priority used by test configurations */
#define UT_EXTI_PRIO                        ( 7u )

/** Default priority of Exti_Get_DefaultConfig */
#define UT_EXTI_DEFAULT_PRIO                ( 10u )

/** Maximum count of recorded callback calls */
#define UT_EXTI_CALLBACK_REC_CNT            ( 4u )

/** Width of EXTI source field in SYSCFG EXTICR register (4-bit per line on STM32L4 / L4+) */
#define UT_EXTI_EXTICR_FIELD_WIDTH          ( 4u )

/** Count of lines configured by one EXTICR register */
#define UT_EXTI_EXTICR_LINES                ( 4u )

/** Mask of EXTI source field */
#define UT_EXTI_EXTICR_FIELD_MASK           ( 0xFu )

/* ============================== MACROS ==================================== */

/** EXTI line bit mask */
#define UT_EXTI_LINE_MASK( pin )            ( 1u << (uint32_t)( pin ) )

/* ========================== LOCAL VARIABLES =============================== */

/** ISR registered in NVIC for every vector */
static nvic_IsrCallback_t   utExti_IrqIsr[ NVIC_PERIPH_IRQ_SIZE ];

/** IRQ identification passed to Nvic_Set_PeriphIrq_Handler */
static nvic_PeriphIrqList_t utExti_LastHandlerIrq;

/** Edges reported to user callback, in order of calls */
static exti_TriggerEdge_t   utExti_CallbackEdge[ UT_EXTI_CALLBACK_REC_CNT ];

/** Count of user callback calls */
static uint32_t             utExti_CallbackCnt;

/** Count of other user callback calls */
static uint32_t             utExti_OtherCallbackCnt;

/** Value of PR register seen by user callback */
static uint32_t             utExti_PrInCallback;

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    for( uint32_t irqId = 0u; NVIC_PERIPH_IRQ_SIZE > irqId; irqId++ )
    {
        utExti_IrqIsr[ irqId ] = NULL;
    }

    utExti_LastHandlerIrq   = NVIC_PERIPH_IRQ_SIZE;
    utExti_CallbackCnt      = 0u;
    utExti_OtherCallbackCnt = 0u;
    utExti_PrInCallback     = 0u;

    Nvic_Set_PeriphIrq_Handler_Stub( Ut_Exti_NvicSetHandlerStub );
}


void tearDown( void )
{
    /* Mocks are verified by generated runner */
}

/* ========================== MODULE VERSION ================================ */

/**
 * \brief   Exti_Get_ModuleVersion() returns version of the module.
 *
 * \details Reads the module version structure.
 *
 * \par Expected results
 * - Version is 1.0.0 (Major 1, Minor 0, Patch 0).
 */
void Ut_Exti_Get_ModuleVersion_ReturnsVersion( void )
{
    exti_ModuleVersion_t version = Exti_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Minor );
    TEST_ASSERT_EQUAL_UINT8( 0u, version.Patch );
}

/* =========================== DEFAULT CONFIG =============================== */

/**
 * \brief   Exti_Get_DefaultConfig() fills default line configuration.
 *
 * \details Reads default configuration into local structure.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - Pin 0, port A, no pull, low speed, priority 10, falling edge, no callback.
 */
void Ut_Exti_Get_DefaultConfig_ReturnsDefaults( void )
{
    exti_PeriphConfig_t config;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_DefaultConfig( &config ) );

    TEST_ASSERT_EQUAL( EXTI_PIN_0,                config.ExtiPin );
    TEST_ASSERT_EQUAL( EXTI_PORT_A,               config.ExtiPort );
    TEST_ASSERT_EQUAL( EXTI_PIN_PULL_NONE,        config.ExtiPinPull );
    TEST_ASSERT_EQUAL( EXTI_PIN_SPEED_LOW,        config.ExtiPinSpeed );
    TEST_ASSERT_EQUAL( UT_EXTI_DEFAULT_PRIO,      config.ExtiPriority );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, config.ExtiTriggerEdge );
    TEST_ASSERT_NULL( config.ExtiCallback );
}


/**
 * \brief   Exti_Get_DefaultConfig() rejects NULL pointer.
 *
 * \details Calls Exti_Get_DefaultConfig() with NULL pointer.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 */
void Ut_Exti_Get_DefaultConfig_NullPtr_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_DefaultConfig( NULL ) );
}

/* ============================ INITIALIZATION ============================== */

/**
 * \brief   Exti_Init() rejects NULL configuration.
 *
 * \details Calls Exti_Init() with NULL pointer.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - RCC, GPIO and NVIC are not called (strict mocks without expectations).
 */
void Ut_Exti_Init_NullConfig_ReturnsErrorWithoutAccess( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( NULL ) );
}


/**
 * \brief   Exti_Init() rejects configuration with value out of range.
 *
 * \details Calls Exti_Init() with valid configuration of line 5 where one item
 *          is invalid: pin, port, pull, speed and trigger edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in all cases.
 * - RCC, GPIO and NVIC are not called, interrupt mask IMR is not written.
 */
void Ut_Exti_Init_InvalidConfig_ReturnsErrorWithoutAccess( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    config.ExtiPin = EXTI_PIN_CNT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    config.ExtiPort = EXTI_PORT_CNT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    config.ExtiPinPull = (exti_PinPullCfg_t)( EXTI_PIN_PULL_DOWN + 1u );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    config.ExtiPinSpeed = (exti_PinSpeed_t)( EXTI_PIN_SPEED_VERY_HIGH + 1u );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    config.ExtiTriggerEdge = (exti_TriggerEdge_t)( EXTI_TRIGGER_EDGE_BOTH + 1u );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Init() configures EXTI line with falling edge trigger.
 *
 * \details Initializes line 5 on port C (pull-up, high speed, priority 7, falling
 *          edge). SYSCFG clock activation, GPIO input initialization and NVIC
 *          priority / activation are expected, ISR registration is captured by
 *          stub.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - FTSR = line 5 bit, RTSR = 0, IMR = line 5 bit.
 * - SYSCFG EXTICR source of line 5 = port C (4-bit field of the line).
 * - Pending flag of line 5 is cleared (PR bit written).
 * - ISR is registered for shared vector NVIC_PERIPH_IRQ_EXTI9_5.
 */
void Ut_Exti_Init_FallingEdge_ConfiguresLine( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u,                              EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->IMR1 );
    TEST_ASSERT_EQUAL( LL_SYSCFG_EXTI_PORTC, LL_SYSCFG_GetEXTISource( LL_SYSCFG_EXTI_LINE5 ) );
    TEST_ASSERT_EQUAL_HEX32( LL_SYSCFG_EXTI_PORTC << ( UT_EXTI_EXTICR_FIELD_WIDTH * ( EXTI_PIN_5 % UT_EXTI_EXTICR_LINES ) ),
                             SYSCFG->EXTICR[ EXTI_PIN_5 / UT_EXTI_EXTICR_LINES ] );

    /* Pending flag of previous configuration cleared */
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->PR1 );

    TEST_ASSERT_EQUAL( NVIC_PERIPH_IRQ_EXTI9_5, utExti_LastHandlerIrq );
    TEST_ASSERT_NOT_NULL( utExti_IrqIsr[ NVIC_PERIPH_IRQ_EXTI9_5 ] );
}


/**
 * \brief   Exti_Init() with rising edge enables only rising trigger.
 *
 * \details Initializes line 5 with rising edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK, FTSR = 0, RTSR = line 5 bit.
 */
void Ut_Exti_Init_RisingEdge_ConfiguresRisingTriggerOnly( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_RAISING );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u,                              EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->RTSR1 );
}


/**
 * \brief   Exti_Init() with both edges enables rising and falling trigger.
 *
 * \details Initializes line 5 with both edges.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK, FTSR = RTSR = line 5 bit.
 */
void Ut_Exti_Init_BothEdges_ConfiguresBothTriggers( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->RTSR1 );
}


/**
 * \brief   Exti_Init() removes trigger and port of previous configuration of the line.
 *
 * \details Presets rising trigger of lines 5 and 6 and port D as source of lines
 *          4 - 7, then initializes line 5 on port C with falling edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK.
 * - RTSR contains only line 6 bit (other lines untouched).
 * - EXTICR field of line 5 = port C, fields of lines 4, 6, 7 keep port D.
 */
void Ut_Exti_Init_Reconfiguration_RemovesPreviousConfig( void )
{
    exti_PeriphConfig_t config        = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    const uint32_t      portDAllLines = 0x3333u;   /* Port D in all 4 fields */
    const uint32_t      line5Field    = UT_EXTI_EXTICR_FIELD_MASK << Ut_Exti_Get_ExtiCrField( EXTI_PIN_5 );

    EXTI->RTSR1        = UT_EXTI_LINE_MASK( EXTI_PIN_5 ) | UT_EXTI_LINE_MASK( EXTI_PIN_6 );  /* Previous config */
    SYSCFG->EXTICR[1] = portDAllLines;

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    /* Only own line changed */
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_6 ), EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( ( portDAllLines & ~line5Field ) | ( LL_SYSCFG_EXTI_PORTC << Ut_Exti_Get_ExtiCrField( EXTI_PIN_5 ) ),
                             SYSCFG->EXTICR[1] );
}


/**
 * \brief   Exti_Init() maps pull and speed configuration to GPIO.
 *
 * \details Initializes line 0 on port A with every pull configuration (none,
 *          up, down) and every speed (low, medium, high, very high).
 *
 * \par Expected results
 * - Gpio_Init() is called with PA0, input mode and the GPIO pull / speed of
 *   the same meaning.
 * - EXTI_REQUEST_OK is returned for every configuration.
 */
void Ut_Exti_Init_PullAndSpeedMapping_PassedToGpio( void )
{
    const struct
    {
        exti_PinPullCfg_t ExtiPull;
        exti_PinSpeed_t   ExtiSpeed;
        gpio_PinPullCfg_t GpioPull;
        gpio_PinSpeed_t   GpioSpeed;
    }   mappingLut[] =
    {
        { EXTI_PIN_PULL_NONE, EXTI_PIN_SPEED_LOW,       GPIO_PIN_PULL_NONE, GPIO_PIN_SPEED_LOW       },
        { EXTI_PIN_PULL_UP,   EXTI_PIN_SPEED_MEDIUM,    GPIO_PIN_PULL_UP,   GPIO_PIN_SPEED_MEDIUM    },
        { EXTI_PIN_PULL_DOWN, EXTI_PIN_SPEED_HIGH,      GPIO_PIN_PULL_DOWN, GPIO_PIN_SPEED_HIGH      },
        { EXTI_PIN_PULL_DOWN, EXTI_PIN_SPEED_VERY_HIGH, GPIO_PIN_PULL_DOWN, GPIO_PIN_SPEED_VERY_HIGH },
    };
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_0, EXTI_TRIGGER_EDGE_FALLING );

    config.ExtiPort = EXTI_PORT_A;

    for( uint32_t idx = 0u; ( sizeof( mappingLut ) / sizeof( mappingLut[ 0u ] ) ) > idx; idx++ )
    {
        config.ExtiPinPull  = mappingLut[ idx ].ExtiPull;
        config.ExtiPinSpeed = mappingLut[ idx ].ExtiSpeed;

        Ut_Exti_Expect_Init( EXTI_PIN_0, GPIO_PORT_A, mappingLut[ idx ].GpioPull, mappingLut[ idx ].GpioSpeed, UT_EXTI_PRIO );

        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );
    }
}


/**
 * \brief   Exti_Init() maps every port to GPIO port and SYSCFG EXTICR value.
 *
 * \details Initializes line 3 on every available port (EXTI_PORT_A ..
 *          EXTI_PORT_CNT - 1), registers are reset before every initialization.
 *
 * \par Expected results
 * - Gpio_Init() is called with the GPIO port of the same order.
 * - EXTICR field of line 3 = SYSCFG value of the port (port letter index, ports
 *   not available on the device are skipped in both enumerations).
 */
void Ut_Exti_Init_AllPorts_MappedToGpioAndSyscfg( void )
{
    static const uint32_t expectedSysPort[] =
    {
#if defined(GPIOA)
        LL_SYSCFG_EXTI_PORTA,
#endif
#if defined(GPIOB)
        LL_SYSCFG_EXTI_PORTB,
#endif
#if defined(GPIOC)
        LL_SYSCFG_EXTI_PORTC,
#endif
#if defined(GPIOD)
        LL_SYSCFG_EXTI_PORTD,
#endif
#if defined(GPIOE)
        LL_SYSCFG_EXTI_PORTE,
#endif
#if defined(GPIOF)
        LL_SYSCFG_EXTI_PORTF,
#endif
#if defined(GPIOG)
        LL_SYSCFG_EXTI_PORTG,
#endif
#if defined(GPIOH)
        LL_SYSCFG_EXTI_PORTH,
#endif
#if defined(GPIOI)
        LL_SYSCFG_EXTI_PORTI,
#endif
#if defined(GPIOJ)
        LL_SYSCFG_EXTI_PORTJ,
#endif
#if defined(GPIOK)
        LL_SYSCFG_EXTI_PORTK,
#endif
    };

    TEST_ASSERT_EQUAL_UINT32( EXTI_PORT_CNT, sizeof( expectedSysPort ) / sizeof( expectedSysPort[ 0 ] ) );
    TEST_ASSERT_EQUAL_UINT32( GPIO_PORT_CNT, EXTI_PORT_CNT );

    for( uint32_t portId = 0u; EXTI_PORT_CNT > portId; portId++ )
    {
        exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_3, EXTI_TRIGGER_EDGE_FALLING );

        config.ExtiPort = (exti_PortId_t)portId;

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

        Ut_Exti_Expect_Init( EXTI_PIN_3, (gpio_PortId_t)portId, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );
        TEST_ASSERT_EQUAL_HEX32( expectedSysPort[ portId ] << Ut_Exti_Get_ExtiCrField( EXTI_PIN_3 ), SYSCFG->EXTICR[0] );
    }
}


/**
 * \brief   Exti_Init() uses NVIC vector, line bit and EXTICR field of every line.
 *
 * \details For every line 0 - 15: registers are reset and the line is initialized
 *          on port C.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK for every line.
 * - ISR registered for EXTI0 - EXTI4 (lines 0 - 4), shared EXTI9_5 (lines 5 - 9)
 *   and shared EXTI15_10 (lines 10 - 15).
 * - IMR contains only the line bit, EXTICR field of the line selects port C.
 */
void Ut_Exti_Init_AllLines_UseLineNvicIrqAndLineBit( void )
{
    for( uint32_t pinId = 0u; EXTI_PIN_CNT > pinId; pinId++ )
    {
        exti_PeriphConfig_t config = Ut_Exti_Get_Config( (exti_PinId_t)pinId, EXTI_TRIGGER_EDGE_FALLING );

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

        Ut_Exti_Expect_Init( (exti_PinId_t)pinId, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

        TEST_ASSERT_EQUAL( Ut_Exti_Get_IrqId( (exti_PinId_t)pinId ), utExti_LastHandlerIrq );
        TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( pinId ), EXTI->IMR1 );
        TEST_ASSERT_EQUAL_HEX32( LL_SYSCFG_EXTI_PORTC << Ut_Exti_Get_ExtiCrField( (exti_PinId_t)pinId ),
                                 SYSCFG->EXTICR[ pinId / UT_EXTI_EXTICR_LINES ] );
    }
}


/**
 * \brief   Exti_Init() stops on SYSCFG clock activation error.
 *
 * \details Rcc_Set_PeriphActive() returns error, line 5 is initialized.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - GPIO and NVIC are not called, IMR is not written, no ISR registered.
 */
void Ut_Exti_Init_RccError_ReturnsErrorWithoutLineConfig( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_SYSCFG, RCC_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_NULL( utExti_IrqIsr[ NVIC_PERIPH_IRQ_EXTI9_5 ] );
}


/**
 * \brief   Exti_Init() stops on GPIO initialization error.
 *
 * \details Gpio_Init() mock returns error, line 5 is initialized.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - IMR, FTSR and EXTICR are not written, no ISR registered, NVIC not called.
 */
void Ut_Exti_Init_GpioError_ReturnsErrorWithoutLineConfig( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_SYSCFG, RCC_REQUEST_OK );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, SYSCFG->EXTICR[1] );
    TEST_ASSERT_NULL( utExti_IrqIsr[ NVIC_PERIPH_IRQ_EXTI9_5 ] );
}


/**
 * \brief   Exti_Init() stops on NVIC priority error.
 *
 * \details Gpio_Init() succeeds, Nvic_Set_PeriphIrq_Prio() returns error.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - No ISR registered, IRQ is not activated (no further NVIC call expected).
 */
void Ut_Exti_Init_NvicPrioError_ReturnsErrorWithoutIrqActivation( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_SYSCFG, RCC_REQUEST_OK );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    Nvic_Set_PeriphIrq_Prio_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI9_5, UT_EXTI_PRIO, NVIC_REQUEST_ERROR );

    /* No handler registration, no Nvic_Set_PeriphIrq_Active expected */
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );
    TEST_ASSERT_NULL( utExti_IrqIsr[ NVIC_PERIPH_IRQ_EXTI9_5 ] );
}

/* ============================ INTERRUPT HANDLING ========================== */

/**
 * \brief   Line ISR of falling edge line reports falling edge.
 *
 * \details Initializes line 2 (own vector EXTI2) with falling edge, presets
 *          pending flag of line 2 and calls captured ISR.
 *
 * \par Expected results
 * - User callback called once with EXTI_TRIGGER_EDGE_FALLING.
 * - Pin level is not read (GPIO mock without expectation).
 * - PR is written with line 2 bit (write-1-to-clear).
 */
void Ut_Exti_Isr_FallingEdgeLine_CallsCallbackWithFallingEdge( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_2, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_2 );

    Ut_Exti_Call_Isr( EXTI_PIN_2 );

    TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, utExti_CallbackEdge[ 0 ] );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_2 ), EXTI->PR1 );
}


/**
 * \brief   Line ISR of rising edge line reports rising edge.
 *
 * \details Initializes line 12 (shared vector EXTI15_10) with rising edge,
 *          presets pending flag of line 12 and calls captured ISR.
 *
 * \par Expected results
 * - User callback called once with EXTI_TRIGGER_EDGE_RAISING.
 */
void Ut_Exti_Isr_RisingEdgeLine_CallsCallbackWithRisingEdge( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_12, EXTI_TRIGGER_EDGE_RAISING );

    Ut_Exti_Expect_Init( EXTI_PIN_12, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_12 );

    Ut_Exti_Call_Isr( EXTI_PIN_12 );

    TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, utExti_CallbackEdge[ 0 ] );
}


/**
 * \brief   Line ISR of both edges line derives edge from pin level.
 *
 * \details Initializes line 5 on port C with both edges, then twice presets
 *          pending flag of line 5 and calls captured ISR: pin level HIGH, then
 *          pin level LOW (Gpio_Get_PinLevel mock).
 *
 * \par Expected results
 * - Pin level of PC5 is read in every ISR.
 * - User callback called twice - rising edge (HIGH), then falling edge (LOW).
 */
void Ut_Exti_Isr_BothEdgesLine_EdgeFromPinLevel( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );
    Ut_Exti_Expect_PinLevel( GPIO_PORT_C, EXTI_PIN_5, GPIO_PIN_LEVEL_HIGH );
    Ut_Exti_Call_Isr( EXTI_PIN_5 );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );
    Ut_Exti_Expect_PinLevel( GPIO_PORT_C, EXTI_PIN_5, GPIO_PIN_LEVEL_LOW );
    Ut_Exti_Call_Isr( EXTI_PIN_5 );

    TEST_ASSERT_EQUAL_UINT32( 2u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, utExti_CallbackEdge[ 0 ] );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, utExti_CallbackEdge[ 1 ] );
}


/**
 * \brief   Pending flag is cleared before the user callback is called.
 *
 * \details Initializes line 2, presets pending flags of lines 2 and 3 and calls
 *          ISR of line 2. Callback records PR value.
 *
 * \par Expected results
 * - Callback sees PR written with line 2 bit only (flag already cleared, a new
 *   edge during the callback sets the flag again and is not lost).
 */
void Ut_Exti_Isr_FlagClearedBeforeCallback( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_2, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_2 ) | UT_EXTI_LINE_MASK( EXTI_PIN_3 );

    Ut_Exti_Call_Isr( EXTI_PIN_2 );

    TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_2 ), utExti_PrInCallback );
}


/**
 * \brief   Line ISR ignores pending flags of other lines.
 *
 * \details Initializes line 2, presets pending flags of lines 1 and 3 and calls
 *          ISR of line 2.
 *
 * \par Expected results
 * - User callback is not called.
 * - PR keeps flags of lines 1 and 3 (not written by line 2).
 */
void Ut_Exti_Isr_OtherLineFlag_DoesNotCallCallback( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_2, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_1 ) | UT_EXTI_LINE_MASK( EXTI_PIN_3 );

    Ut_Exti_Call_Isr( EXTI_PIN_2 );

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_1 ) | UT_EXTI_LINE_MASK( EXTI_PIN_3 ), EXTI->PR1 );
}


/**
 * \brief   Shared vector ISR dispatches the edge to the line with pending flag.
 *
 * \details Initializes lines 5 and 7 (shared vector EXTI9_5) with falling edge
 *          and different callbacks, presets pending flag of line 7 and calls
 *          the shared ISR.
 *
 * \par Expected results
 * - Callback of line 7 is called once, callback of line 5 is not called.
 */
void Ut_Exti_Isr_SharedVector_DispatchesToPendingLine( void )
{
    exti_PeriphConfig_t config5 = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    exti_PeriphConfig_t config7 = Ut_Exti_Get_Config( EXTI_PIN_7, EXTI_TRIGGER_EDGE_FALLING );

    config7.ExtiCallback = Ut_Exti_OtherCallback;

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config5 ) );
    Ut_Exti_Expect_Init( EXTI_PIN_7, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config7 ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_7 );

    Ut_Exti_Call_Isr( EXTI_PIN_5 );     /* Same vector as line 7 */

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_UINT32( 1u, utExti_OtherCallbackCnt );
}


/**
 * \brief   Line ISR without user callback only clears pending flag.
 *
 * \details Initializes line 2 without callback, presets pending flag of line 2
 *          and calls ISR.
 *
 * \par Expected results
 * - No callback call (no NULL pointer call).
 * - PR is written with line 2 bit (flag cleared, no endless interrupt).
 */
void Ut_Exti_Isr_NoCallback_ClearsFlagWithoutCall( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    config.ExtiCallback = NULL;

    Ut_Exti_Expect_Init( EXTI_PIN_2, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_2 ) | UT_EXTI_LINE_MASK( EXTI_PIN_1 );

    Ut_Exti_Call_Isr( EXTI_PIN_2 );

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_2 ), EXTI->PR1 );  /* Write-1-to-clear of own line */
}

/* =========================== DEINITIALIZATION ============================= */

/**
 * \brief   Exti_Deinit() disables the line and returns the pin to analog mode.
 *
 * \details Presets enabled line 2 (and line 1) with both triggers on port C,
 *          deinitializes line 2 on port C.
 *
 * \par Expected results
 * - NVIC vector EXTI2 is deactivated, Gpio_Init() called for PC2 analog, no
 *   pull, low speed.
 * - EXTI_REQUEST_OK, IMR keeps only line 1, RTSR = FTSR = 0, PR line 2 bit
 *   written, EXTICR field of line 2 = port A (reset value).
 */
void Ut_Exti_Deinit_DisablesLineAndSetsPinAnalog( void )
{
    exti_PeriphConfig_t config         = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_BOTH );
    gpio_Config_t       expectedGpio   = { 0 };

    expectedGpio.PortId   = GPIO_PORT_C;
    expectedGpio.PinId    = GPIO_PIN_ID_2;
    expectedGpio.PinMode  = GPIO_PIN_MODE_ANALOG;
    expectedGpio.PinPull  = GPIO_PIN_PULL_NONE;
    expectedGpio.PinSpeed = GPIO_PIN_SPEED_LOW;

    EXTI->IMR1         = UT_EXTI_LINE_MASK( EXTI_PIN_2 ) | UT_EXTI_LINE_MASK( EXTI_PIN_1 );
    EXTI->RTSR1        = UT_EXTI_LINE_MASK( EXTI_PIN_2 );
    EXTI->FTSR1        = UT_EXTI_LINE_MASK( EXTI_PIN_2 );
    SYSCFG->EXTICR[0] = LL_SYSCFG_EXTI_PORTC << Ut_Exti_Get_ExtiCrField( EXTI_PIN_2 );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI2, NVIC_REQUEST_OK );
    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_1 ), EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_2 ), EXTI->PR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, SYSCFG->EXTICR[0] );
}


/**
 * \brief   Exti_Deinit() keeps shared NVIC vector active for other enabled line.
 *
 * \details Presets enabled lines 5 and 9 (shared vector EXTI9_5), deinitializes
 *          line 5, then deinitializes line 9.
 *
 * \par Expected results
 * - Line 5: vector EXTI9_5 is not deactivated (strict mock without expectation),
 *   IMR keeps line 9.
 * - Line 9 (last line of the group): vector EXTI9_5 is deactivated.
 */
void Ut_Exti_Deinit_SharedVector_KeptForOtherEnabledLine( void )
{
    exti_PeriphConfig_t config5 = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    exti_PeriphConfig_t config9 = Ut_Exti_Get_Config( EXTI_PIN_9, EXTI_TRIGGER_EDGE_FALLING );

    EXTI->IMR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 ) | UT_EXTI_LINE_MASK( EXTI_PIN_9 );

    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config5 ) );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_9 ), EXTI->IMR1 );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI9_5, NVIC_REQUEST_OK );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config9 ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Deinit() removes user callback of the line.
 *
 * \details Initializes line 2, deinitializes it, then presets pending flag and
 *          calls ISR (late interrupt).
 *
 * \par Expected results
 * - EXTI_REQUEST_OK from deinitialization.
 * - User callback is not called by the late interrupt.
 */
void Ut_Exti_Deinit_RemovesCallback( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_2, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI2, NVIC_REQUEST_OK );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config ) );

    EXTI->PR1 = UT_EXTI_LINE_MASK( EXTI_PIN_2 );

    Ut_Exti_Call_Isr( EXTI_PIN_2 );     /* Late interrupt */

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
}


/**
 * \brief   Exti_Deinit() disables the line even when NVIC fails.
 *
 * \details Presets enabled line 2, Nvic_Set_PeriphIrq_Inactive() returns error.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - GPIO is still deinitialized, IMR line 2 bit is cleared.
 */
void Ut_Exti_Deinit_NvicError_ReturnsErrorButDisablesLine( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_2, EXTI_TRIGGER_EDGE_FALLING );

    EXTI->IMR1 = UT_EXTI_LINE_MASK( EXTI_PIN_2 );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI2, NVIC_REQUEST_ERROR );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Deinit( &config ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Deinit() rejects invalid configuration.
 *
 * \details Calls Exti_Deinit() with NULL, invalid pin and invalid port.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in all cases.
 * - GPIO and NVIC are not called.
 */
void Ut_Exti_Deinit_InvalidConfig_ReturnsErrorWithoutAccess( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Deinit( NULL ) );

    config.ExtiPin = EXTI_PIN_CNT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Deinit( &config ) );

    config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );
    config.ExtiPort = EXTI_PORT_CNT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Deinit( &config ) );
}


/**
 * \brief   Interrupt handler of every line with own vector reports its own line.
 *
 * \details Lines 0 - 4 (vectors EXTI0 - EXTI4) are initialized with falling edge one by one,
 *          pending flag of the line is preset and the captured ISR of the line vector is
 *          called. Exti_Task() is called at the end.
 *
 * \par Expected results
 * - User callback called once per line with EXTI_TRIGGER_EDGE_FALLING.
 * - PR is written with the bit of the line (write-1-to-clear).
 * - Exti_Task() does not access EXTI registers and mocks (strict mocks).
 */
void Ut_Exti_Isr_OwnVectorLines_EachLineReported( void )
{
    const exti_PinId_t lineLut[] = { EXTI_PIN_0, EXTI_PIN_1, EXTI_PIN_2, EXTI_PIN_3, EXTI_PIN_4 };

    for( uint32_t idx = 0u; ( sizeof( lineLut ) / sizeof( lineLut[ 0u ] ) ) > idx; idx++ )
    {
        exti_PeriphConfig_t config = Ut_Exti_Get_Config( lineLut[ idx ], EXTI_TRIGGER_EDGE_FALLING );

        Ut_Exti_Expect_Init( lineLut[ idx ], GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

        EXTI->PR1                  = UT_EXTI_LINE_MASK( lineLut[ idx ] );
        utExti_CallbackCnt        = 0u;
        utExti_CallbackEdge[ 0u ] = EXTI_TRIGGER_EDGE_RAISING;

        Ut_Exti_Call_Isr( lineLut[ idx ] );

        TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
        TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, utExti_CallbackEdge[ 0u ] );
        TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( lineLut[ idx ] ), EXTI->PR1 );
    }

    const uint32_t imrValue = EXTI->IMR1;

    Exti_Task();

    TEST_ASSERT_EQUAL_HEX32( imrValue, EXTI->IMR1 );
}


/**
 * \brief   Every EXTI interrupt service routine ends with DSB.
 *
 * \details Lines 0 - 4, 5 (shared vector EXTI9_5) and 10 (shared vector EXTI15_10) are
 *          initialized on port C one by one, the captured ISR of the line vector is called once
 *          without pending flag.
 *
 * \note    Cortex-M4 r0p1 core erratum (Arm ID 838869 "Store immediate overlapping exception
 *          return operation might vector to incorrect interrupt"), protection taken over from
 *          STM32G4 device errata bug AB#1104: a buffered store with immediate offset still
 *          pending at the exception return may vector to an incorrect interrupt. Workaround -
 *          DSB before the exception return of every handler.
 *
 * \par Expected results
 * - Every ISR executes exactly one DSB.
 */
void Ut_Exti_Isr_AllVectors_EndWithDsb( void )
{
    const exti_PinId_t lineLut[] = { EXTI_PIN_0, EXTI_PIN_1, EXTI_PIN_2, EXTI_PIN_3, EXTI_PIN_4, EXTI_PIN_5, EXTI_PIN_10 };

    for( uint32_t idx = 0u; ( sizeof( lineLut ) / sizeof( lineLut[ 0u ] ) ) > idx; idx++ )
    {
        exti_PeriphConfig_t config = Ut_Exti_Get_Config( lineLut[ idx ], EXTI_TRIGGER_EDGE_FALLING );

        Ut_Exti_Expect_Init( lineLut[ idx ], GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

        EXTI->PR1 = 0u;

        const uint32_t dsbCnt = CmsisHost_Get_InstrCnt( CMSISHOST_INSTR_DSB );

        Ut_Exti_Call_Isr( lineLut[ idx ] );

        TEST_ASSERT_EQUAL_UINT32( dsbCnt + 1u, CmsisHost_Get_InstrCnt( CMSISHOST_INSTR_DSB ) );
    }
}

/* =========================== LOCAL FUNCTIONS ============================== */

/** Stub of Nvic_Set_PeriphIrq_Handler - stores registered ISR of the vector */
static nvic_RequestState_t Ut_Exti_NvicSetHandlerStub( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int callCnt )
{
    (void)callCnt;

    TEST_ASSERT_LESS_THAN_UINT32( NVIC_PERIPH_IRQ_SIZE, irqId );

    utExti_IrqIsr[ irqId ] = irqHandler;
    utExti_LastHandlerIrq  = irqId;

    return ( NVIC_REQUEST_OK );
}


/** User callback - records reported edges and PR value */
static void Ut_Exti_UserCallback( exti_TriggerEdge_t edge )
{
    if( UT_EXTI_CALLBACK_REC_CNT > utExti_CallbackCnt )
    {
        utExti_CallbackEdge[ utExti_CallbackCnt ] = edge;
    }
    else
    {
        /* Record buffer full */
    }

    utExti_PrInCallback = EXTI->PR1;
    utExti_CallbackCnt++;
}


/** Other user callback - counts calls */
static void Ut_Exti_OtherCallback( exti_TriggerEdge_t edge )
{
    (void)edge;

    utExti_OtherCallbackCnt++;
}


/** Returns valid configuration of the line on port C */
static exti_PeriphConfig_t Ut_Exti_Get_Config( exti_PinId_t pinId, exti_TriggerEdge_t edge )
{
    exti_PeriphConfig_t config;

    config.ExtiPin         = pinId;
    config.ExtiPort        = EXTI_PORT_C;
    config.ExtiPinPull     = EXTI_PIN_PULL_UP;
    config.ExtiPinSpeed    = EXTI_PIN_SPEED_HIGH;
    config.ExtiPriority    = UT_EXTI_PRIO;
    config.ExtiTriggerEdge = edge;
    config.ExtiCallback    = Ut_Exti_UserCallback;

    return ( config );
}


/** Returns NVIC vector of the line (EXTI0 - EXTI4, shared EXTI9_5 and EXTI15_10) */
static nvic_PeriphIrqList_t Ut_Exti_Get_IrqId( exti_PinId_t pinId )
{
    nvic_PeriphIrqList_t irqId;

    if( EXTI_PIN_4 >= pinId )
    {
        irqId = (nvic_PeriphIrqList_t)( NVIC_PERIPH_IRQ_EXTI0 + (uint32_t)pinId );
    }
    else if( EXTI_PIN_9 >= pinId )
    {
        irqId = NVIC_PERIPH_IRQ_EXTI9_5;
    }
    else
    {
        irqId = NVIC_PERIPH_IRQ_EXTI15_10;
    }

    return ( irqId );
}


/** Returns bit position of EXTI source field of the line in EXTICR register */
static uint32_t Ut_Exti_Get_ExtiCrField( exti_PinId_t pinId )
{
    return ( UT_EXTI_EXTICR_FIELD_WIDTH * ( (uint32_t)pinId % UT_EXTI_EXTICR_LINES ) );
}


/** Calls ISR registered for NVIC vector of the line */
static void Ut_Exti_Call_Isr( exti_PinId_t pinId )
{
    const nvic_IsrCallback_t isr = utExti_IrqIsr[ Ut_Exti_Get_IrqId( pinId ) ];

    TEST_ASSERT_NOT_NULL( isr );

    isr();
}


/** Expects successful SYSCFG clock, GPIO and NVIC configuration of the line */
static void Ut_Exti_Expect_Init( exti_PinId_t pinId, gpio_PortId_t gpioPort, gpio_PinPullCfg_t pull, gpio_PinSpeed_t speed, exti_IrqPrio_t prio )
{
    static gpio_Config_t       expectedGpio[ 2 ];   /* Two lines initialized by one test */
    static uint32_t            expectedIdx = 0u;
    const nvic_PeriphIrqList_t irqId       = Ut_Exti_Get_IrqId( pinId );
    gpio_Config_t * const      gpioConfig  = &expectedGpio[ expectedIdx ];

    expectedIdx = ( expectedIdx + 1u ) % 2u;

    *gpioConfig          = (gpio_Config_t){ 0 };
    gpioConfig->PortId   = gpioPort;
    gpioConfig->PinId    = (gpio_PinId_t)pinId;
    gpioConfig->PinMode  = GPIO_PIN_MODE_INPUT;
    gpioConfig->PinPull  = pull;
    gpioConfig->PinSpeed = speed;

    Rcc_Set_PeriphActive_ExpectAndReturn( RCC_PERIPH_SYSCFG, RCC_REQUEST_OK );
    Gpio_Init_ExpectAndReturn( gpioConfig, GPIO_REQUEST_OK );
    Nvic_Set_PeriphIrq_Prio_ExpectAndReturn( irqId, prio, NVIC_REQUEST_OK );
    Nvic_Set_PeriphIrq_Active_ExpectAndReturn( irqId, NVIC_REQUEST_OK );
}


/** Expects read of the pin level in ISR and returns required level */
static void Ut_Exti_Expect_PinLevel( gpio_PortId_t gpioPort, exti_PinId_t pinId, gpio_PinLevel_t pinLevel )
{
    static gpio_PinLevel_t returnedLevel[ 2 ];      /* Two ISR calls prepared by one test */
    static uint32_t        returnedIdx = 0u;

    returnedLevel[ returnedIdx ] = pinLevel;

    Gpio_Get_PinLevel_ExpectAndReturn( gpioPort, (gpio_PinId_t)pinId, NULL, GPIO_REQUEST_OK );
    Gpio_Get_PinLevel_IgnoreArg_pinLevel();
    Gpio_Get_PinLevel_ReturnThruPtr_pinLevel( &returnedLevel[ returnedIdx ] );

    returnedIdx = ( returnedIdx + 1u ) % 2u;
}
