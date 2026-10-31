/**
 * \author Mr.Nobody
 * \file Test_Exti.c
 * \ingroup Exti
 * \brief Unit tests of External Interrupt (EXTI) module.
 *
 * Exti.c is compiled unchanged with real LL drivers. EXTI registers are emulated
 * by RegMem, GPIO and NVIC modules are mocked by CMock. Line ISR registered in
 * NVIC is captured by stub and called directly to test interrupt handling.
 *
 * \note Pending registers (RPR1 / FPR1) are write-1-to-clear on HW. Emulated
 *       registers keep written value, tests check that the line bit was written.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "RegMem.h"                         /* Register memory emulation      */
#include "Exti_Port.h"                      /* Module under test              */
#include "MockGpio_Port.h"                  /* GPIO module mock               */
#include "MockNvic_Port.h"                  /* NVIC module mock               */
#include "Stm32_exti.h"                     /* EXTI registers definition      */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static nvic_RequestState_t  Ut_Exti_NvicSetHandlerStub  ( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int callCnt );
static void                 Ut_Exti_UserCallback        ( exti_TriggerEdge_t edge );
static exti_PeriphConfig_t  Ut_Exti_Get_Config          ( exti_PinId_t pinId, exti_TriggerEdge_t edge );
static void                 Ut_Exti_Expect_Init         ( exti_PinId_t pinId, gpio_PortId_t gpioPort, gpio_PinPullCfg_t pull, gpio_PinSpeed_t speed, exti_IrqPrio_t prio );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/** Priority used by test configurations */
#define UT_EXTI_PRIO                        ( 7u )

/** Default priority of Exti_Get_DefaultConfig */
#define UT_EXTI_DEFAULT_PRIO                ( 10u )

/** Maximum count of recorded callback calls */
#define UT_EXTI_CALLBACK_REC_CNT            ( 4u )

/** Width of EXTI source field in EXTICR register (8-bit per line on STM32H5) */
#define UT_EXTI_EXTICR_FIELD_WIDTH          ( 8u )

/** Count of lines configured by one EXTICR register */
#define UT_EXTI_EXTICR_LINES                ( 4u )

/* ============================== MACROS ==================================== */

/** EXTI line bit mask */
#define UT_EXTI_LINE_MASK( pin )            ( 1u << (uint32_t)( pin ) )

/* ========================== LOCAL VARIABLES =============================== */

/** ISR registered in NVIC for every EXTI line */
static nvic_IsrCallback_t   utExti_LineIsr[ EXTI_PIN_CNT ];

/** IRQ identification passed to Nvic_Set_PeriphIrq_Handler */
static nvic_PeriphIrqList_t utExti_LastHandlerIrq;

/** Edges reported to user callback, in order of calls */
static exti_TriggerEdge_t   utExti_CallbackEdge[ UT_EXTI_CALLBACK_REC_CNT ];

/** Count of user callback calls */
static uint32_t             utExti_CallbackCnt;

/* ============================ TEST FIXTURE ================================ */

void setUp( void )
{
    TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

    for( uint32_t pinId = 0u; EXTI_PIN_CNT > pinId; pinId++ )
    {
        utExti_LineIsr[ pinId ] = NULL;
    }

    utExti_LastHandlerIrq = NVIC_PERIPH_IRQ_SIZE;
    utExti_CallbackCnt    = 0u;

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
 * - Version is 1.1.0 (Major 1, Minor 1, Patch 0).
 */
void Ut_Exti_Get_ModuleVersion_ReturnsVersion( void )
{
    exti_ModuleVersion_t version = Exti_Get_ModuleVersion();

    TEST_ASSERT_EQUAL_UINT8( 1u, version.Major );
    TEST_ASSERT_EQUAL_UINT8( 1u, version.Minor );
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
 * - GPIO and NVIC are not called (strict mocks without expectations).
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
 * - GPIO and NVIC are not called, interrupt mask IMR1 is not written.
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
 *          edge). GPIO input initialization and NVIC priority / activation are
 *          expected, ISR registration is captured by stub.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - FTSR1 = line 5 bit, RTSR1 = 0, IMR1 = line 5 bit.
 * - EXTICR source of line 5 = port C (8-bit field of the line).
 * - Pending flags RPR1 / FPR1 of line 5 are cleared (bit written).
 * - ISR is registered for NVIC_PERIPH_IRQ_EXTI5.
 */
void Ut_Exti_Init_FallingEdge_ConfiguresLine( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u,                              EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->IMR1 );
    TEST_ASSERT_EQUAL( LL_EXTI_EXTI_PORTC, LL_EXTI_GetEXTISource( LL_EXTI_EXTI_LINE5 ) );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_EXTI_PORTC << ( UT_EXTI_EXTICR_FIELD_WIDTH * ( EXTI_PIN_5 % UT_EXTI_EXTICR_LINES ) ),
                             EXTI->EXTICR[ EXTI_PIN_5 / UT_EXTI_EXTICR_LINES ] );

    /* Pending flags of previous configuration cleared */
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->RPR1 );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->FPR1 );

    TEST_ASSERT_EQUAL( NVIC_PERIPH_IRQ_EXTI5, utExti_LastHandlerIrq );
    TEST_ASSERT_NOT_NULL( utExti_LineIsr[ EXTI_PIN_5 ] );
}


/**
 * \brief   Exti_Init() with rising edge enables only rising trigger.
 *
 * \details Initializes line 5 with rising edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK, FTSR1 = 0, RTSR1 = line 5 bit.
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
 * - EXTI_REQUEST_OK, FTSR1 = RTSR1 = line 5 bit.
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
 * \brief   Exti_Init() removes trigger of previous configuration of the line.
 *
 * \details Presets rising trigger of lines 5 and 6, then initializes line 5 with
 *          falling edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK.
 * - RTSR1 contains only line 6 bit (other lines untouched).
 */
void Ut_Exti_Init_Reconfiguration_RemovesPreviousTrigger( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    EXTI->RTSR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 ) | UT_EXTI_LINE_MASK( EXTI_PIN_6 );  /* Previous config */

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    /* Only own line changed */
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_6 ), EXTI->RTSR1 );
}


/**
 * \brief   Exti_Init() maps pull and speed configuration to GPIO.
 *
 * \details Initializes line 0 on port A with pull-down and very high speed.
 *
 * \par Expected results
 * - Gpio_Init() is called with PA0, input mode, pull-down, very high speed.
 * - EXTI_REQUEST_OK is returned.
 */
void Ut_Exti_Init_PullAndSpeedMapping_PassedToGpio( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_0, EXTI_TRIGGER_EDGE_FALLING );

    config.ExtiPort     = EXTI_PORT_A;
    config.ExtiPinPull  = EXTI_PIN_PULL_DOWN;
    config.ExtiPinSpeed = EXTI_PIN_SPEED_VERY_HIGH;

    Ut_Exti_Expect_Init( EXTI_PIN_0, GPIO_PORT_A, GPIO_PIN_PULL_DOWN, GPIO_PIN_SPEED_VERY_HIGH, UT_EXTI_PRIO );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );
}


/**
 * \brief   Exti_Init() uses own NVIC IRQ, line bit and EXTICR field for every line.
 *
 * \details For every line 0 - 15: registers are reset and the line is initialized
 *          on port C.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK for every line.
 * - ISR registered for NVIC_PERIPH_IRQ_EXTI0 + line.
 * - IMR1 contains only the line bit, EXTICR field of the line selects port C.
 */
void Ut_Exti_Init_AllLines_UseOwnNvicIrqAndLineBit( void )
{
    for( uint32_t pinId = 0u; EXTI_PIN_CNT > pinId; pinId++ )
    {
        exti_PeriphConfig_t config = Ut_Exti_Get_Config( (exti_PinId_t)pinId, EXTI_TRIGGER_EDGE_FALLING );
        const nvic_PeriphIrqList_t irqId = (nvic_PeriphIrqList_t)( NVIC_PERIPH_IRQ_EXTI0 + pinId );

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );

        Ut_Exti_Expect_Init( (exti_PinId_t)pinId, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );

        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

        TEST_ASSERT_EQUAL( irqId, utExti_LastHandlerIrq );
        TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( pinId ), EXTI->IMR1 );
        TEST_ASSERT_EQUAL_HEX32( LL_EXTI_EXTI_PORTC << ( UT_EXTI_EXTICR_FIELD_WIDTH * ( pinId % UT_EXTI_EXTICR_LINES ) ),
                                 EXTI->EXTICR[ pinId / UT_EXTI_EXTICR_LINES ] );
    }
}


/**
 * \brief   Exti_Init() stops on GPIO initialization error.
 *
 * \details Gpio_Init() mock returns error, line 5 is initialized.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - IMR1 and FTSR1 are not written, no ISR registered, NVIC not called.
 */
void Ut_Exti_Init_GpioError_ReturnsErrorWithoutLineConfig( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_ERROR );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
    TEST_ASSERT_NULL( utExti_LineIsr[ EXTI_PIN_5 ] );
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

    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    Nvic_Set_PeriphIrq_Prio_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI5, UT_EXTI_PRIO, NVIC_REQUEST_ERROR );

    /* No handler registration, no Nvic_Set_PeriphIrq_Active expected */
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &config ) );
    TEST_ASSERT_NULL( utExti_LineIsr[ EXTI_PIN_5 ] );
}

/* ============================ INTERRUPT HANDLING ========================== */

/**
 * \brief   Line ISR reports rising edge.
 *
 * \details Initializes line 5 with both edges, presets rising pending flag of
 *          line 5 and calls captured ISR.
 *
 * \par Expected results
 * - User callback called once with EXTI_TRIGGER_EDGE_RAISING.
 * - Falling pending register FPR1 is not written.
 */
void Ut_Exti_Isr_RisingFlag_CallsCallbackWithRisingEdge( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->RPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );
    EXTI->FPR1 = 0u;

    utExti_LineIsr[ EXTI_PIN_5 ]();

    TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, utExti_CallbackEdge[ 0 ] );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FPR1 );  /* Falling flag not touched */
}


/**
 * \brief   Line ISR reports both pending edges.
 *
 * \details Initializes line 5 with both edges, presets rising and falling pending
 *          flags of line 5 and calls captured ISR.
 *
 * \par Expected results
 * - User callback called twice - rising edge first, then falling edge.
 */
void Ut_Exti_Isr_BothFlags_CallsCallbackForBothEdges( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->RPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );
    EXTI->FPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );

    utExti_LineIsr[ EXTI_PIN_5 ]();

    TEST_ASSERT_EQUAL_UINT32( 2u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, utExti_CallbackEdge[ 0 ] );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, utExti_CallbackEdge[ 1 ] );
}


/**
 * \brief   Line ISR ignores pending flags of other lines.
 *
 * \details Initializes line 5, presets rising flag of line 6 and falling flag of
 *          line 4 and calls ISR of line 5.
 *
 * \par Expected results
 * - User callback is not called.
 * - RPR1 keeps flag of line 6 (not cleared by line 5).
 */
void Ut_Exti_Isr_OtherLineFlag_DoesNotCallCallback( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->RPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_6 );
    EXTI->FPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_4 );

    utExti_LineIsr[ EXTI_PIN_5 ]();

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_6 ), EXTI->RPR1 );  /* Not cleared by line 5 */
}


/**
 * \brief   Line ISR without user callback only clears pending flag.
 *
 * \details Initializes line 5 without callback, presets falling flags of lines 5
 *          and 2 and calls ISR of line 5.
 *
 * \par Expected results
 * - No callback call (no NULL pointer call).
 * - FPR1 is written with line 5 bit only (write-1-to-clear of own line).
 */
void Ut_Exti_Isr_NoCallback_ClearsFlagWithoutCall( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    config.ExtiCallback = NULL;

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    EXTI->RPR1 = 0u;
    EXTI->FPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 ) | UT_EXTI_LINE_MASK( EXTI_PIN_2 );

    utExti_LineIsr[ EXTI_PIN_5 ]();

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_5 ), EXTI->FPR1 );  /* Write-1-to-clear of own line */
}

/**
 * \brief   ISR of every EXTI line reports the edge of its own line.
 *
 * \details For every line 0 - 15: registers are reset, the line is initialized on
 *          port C with both edges, rising pending flag of the line and falling
 *          pending flag of the next line are preset and the ISR registered for
 *          the line is called.
 *
 * \par Expected results
 * - User callback called once with EXTI_TRIGGER_EDGE_RAISING for every line
 *   (the flag of the next line is ignored).
 * - RPR1 keeps only the bit of the line (write-1-to-clear of own line).
 */
void Ut_Exti_Isr_AllLines_CallbackWithOwnLineEdge( void )
{
    for( uint32_t pinId = 0u; EXTI_PIN_CNT > pinId; pinId++ )
    {
        exti_PeriphConfig_t config   = Ut_Exti_Get_Config( (exti_PinId_t)pinId, EXTI_TRIGGER_EDGE_BOTH );
        const uint32_t      nextLine = ( pinId + 1u ) % EXTI_PIN_CNT;

        TEST_ASSERT_EQUAL( REGMEM_REQUEST_OK, RegMem_Reset() );
        utExti_CallbackCnt = 0u;

        Ut_Exti_Expect_Init( (exti_PinId_t)pinId, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

        EXTI->RPR1 = UT_EXTI_LINE_MASK( pinId );
        EXTI->FPR1 = UT_EXTI_LINE_MASK( nextLine );

        TEST_ASSERT_NOT_NULL( utExti_LineIsr[ pinId ] );
        utExti_LineIsr[ pinId ]();

        TEST_ASSERT_EQUAL_UINT32( 1u, utExti_CallbackCnt );
        TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, utExti_CallbackEdge[ 0 ] );
        TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( pinId ), EXTI->RPR1 );
    }
}

/* =========================== DEINITIALIZATION ============================= */

/**
 * \brief   Exti_Deinit() disables the line and returns the pin to analog mode.
 *
 * \details Presets enabled line 5 (and line 1) with both triggers, deinitializes
 *          line 5 on port C.
 *
 * \par Expected results
 * - NVIC IRQ EXTI5 is deactivated, Gpio_Init() called for PC5 analog, no pull,
 *   low speed.
 * - EXTI_REQUEST_OK, IMR1 keeps only line 1, RTSR1 = FTSR1 = 0.
 */
void Ut_Exti_Deinit_DisablesLineAndSetsPinAnalog( void )
{
    exti_PeriphConfig_t config         = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );
    gpio_Config_t       expectedGpio   = { 0 };

    expectedGpio.PortId   = GPIO_PORT_C;
    expectedGpio.PinId    = GPIO_PIN_ID_5;
    expectedGpio.PinMode  = GPIO_PIN_MODE_ANALOG;
    expectedGpio.PinPull  = GPIO_PIN_PULL_NONE;
    expectedGpio.PinSpeed = GPIO_PIN_SPEED_LOW;

    EXTI->IMR1  = UT_EXTI_LINE_MASK( EXTI_PIN_5 ) | UT_EXTI_LINE_MASK( EXTI_PIN_1 );
    EXTI->RTSR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );
    EXTI->FTSR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI5, NVIC_REQUEST_OK );
    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config ) );

    TEST_ASSERT_EQUAL_HEX32( UT_EXTI_LINE_MASK( EXTI_PIN_1 ), EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
}


/**
 * \brief   Exti_Deinit() removes user callback of the line.
 *
 * \details Initializes line 5, deinitializes it, then presets rising pending flag
 *          and calls ISR (late interrupt).
 *
 * \par Expected results
 * - EXTI_REQUEST_OK from deinitialization.
 * - User callback is not called by the late interrupt.
 */
void Ut_Exti_Deinit_RemovesCallback( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_BOTH );

    Ut_Exti_Expect_Init( EXTI_PIN_5, GPIO_PORT_C, GPIO_PIN_PULL_UP, GPIO_PIN_SPEED_HIGH, UT_EXTI_PRIO );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &config ) );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI5, NVIC_REQUEST_OK );
    Gpio_Init_ExpectAnyArgsAndReturn( GPIO_REQUEST_OK );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &config ) );

    EXTI->RPR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );

    utExti_LineIsr[ EXTI_PIN_5 ]();     /* Late interrupt */

    TEST_ASSERT_EQUAL_UINT32( 0u, utExti_CallbackCnt );
}


/**
 * \brief   Exti_Deinit() disables the line even when NVIC fails.
 *
 * \details Presets enabled line 5, Nvic_Set_PeriphIrq_Inactive() returns error.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 * - GPIO is still deinitialized, IMR1 line 5 bit is cleared.
 */
void Ut_Exti_Deinit_NvicError_ReturnsErrorButDisablesLine( void )
{
    exti_PeriphConfig_t config = Ut_Exti_Get_Config( EXTI_PIN_5, EXTI_TRIGGER_EDGE_FALLING );

    EXTI->IMR1 = UT_EXTI_LINE_MASK( EXTI_PIN_5 );

    Nvic_Set_PeriphIrq_Inactive_ExpectAndReturn( NVIC_PERIPH_IRQ_EXTI5, NVIC_REQUEST_ERROR );
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

/* ========================= PERIPHERAL EVENT LINES ========================== */

/**
 * \brief   Exti_Set_LineActive() enables interrupt mask and trigger of a
 *          configurable line.
 *
 * \details Activates line 16 (PVD/AVD, configurable) with interrupt mode and
 *          both trigger edges.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - RTSR1 and FTSR1 contain the line 16 bit, IMR1 contains the line 16 bit,
 *   EMR1 is 0.
 * - Pending flags RPR1 / FPR1 of line 16 are not touched (activation does not
 *   clear pending, only \ref Exti_Set_LinePendingClear does).
 */
void Ut_Exti_Set_LineActive_ConfigurableInterrupt_SetsTriggerAndImr( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_INTERRUPT, .TriggerEdge = EXTI_TRIGGER_EDGE_BOTH };

    EXTI->RPR1 = LL_EXTI_LINE_16;
    EXTI->FPR1 = LL_EXTI_LINE_16;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u,              EXTI->EMR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->RPR1 );  /* Preset pending kept, not cleared */
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->FPR1 );
}


/**
 * \brief   Exti_Set_LineActive() enables only the event mask of a configurable line.
 *
 * \details Activates line 16 with event mode and falling edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - EMR1 contains the line 16 bit, IMR1 is 0.
 * - FTSR1 contains the line 16 bit, RTSR1 is 0.
 */
void Ut_Exti_Set_LineActive_ConfigurableEvent_SetsEmrOnly( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_EVENT, .TriggerEdge = EXTI_TRIGGER_EDGE_FALLING };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->EMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u,              EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u,              EXTI->RTSR1 );
}


/**
 * \brief   Exti_Set_LineActive() enables both masks when mode is both.
 *
 * \details Activates line 16 with both mode and rising edge.
 *
 * \par Expected results
 * - IMR1 and EMR1 both contain the line 16 bit.
 */
void Ut_Exti_Set_LineActive_ConfigurableBothMode_SetsImrAndEmr( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_BOTH, .TriggerEdge = EXTI_TRIGGER_EDGE_RAISING };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->EMR1 );
}


/**
 * \brief   Exti_Set_LineActive() rejects a configurable line without trigger edge.
 *
 * \details Activates line 16 with \ref EXTI_TRIGGER_EDGE_NONE.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned, IMR1 and RTSR1 are not written.
 */
void Ut_Exti_Set_LineActive_ConfigurableNoEdge_ReturnsErrorWithoutAccess( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_INTERRUPT, .TriggerEdge = EXTI_TRIGGER_EDGE_NONE };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RTSR1 );
}


/**
 * \brief   Exti_Set_LineActive() enables masks of a direct line without
 *          touching trigger registers.
 *
 * \details Activates line 17 (RTC, direct) with both mode and
 *          \ref EXTI_TRIGGER_EDGE_NONE.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - IMR1 and EMR1 contain the line 17 bit.
 * - RTSR1, FTSR1, RPR1 and FPR1 stay 0 (direct line has no trigger / pending).
 */
void Ut_Exti_Set_LineActive_DirectLine_SetsMasksOnly( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_BOTH, .TriggerEdge = EXTI_TRIGGER_EDGE_NONE };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineActive( EXTI_LINE_RTC, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_17, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_17, EXTI->EMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RPR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FPR1 );
}


/**
 * \brief   Exti_Set_LineActive() rejects a direct line with a trigger edge.
 *
 * \details Activates line 17 with falling edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned, IMR1 is not written.
 */
void Ut_Exti_Set_LineActive_DirectLineWithEdge_ReturnsErrorWithoutAccess( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_INTERRUPT, .TriggerEdge = EXTI_TRIGGER_EDGE_FALLING };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_RTC, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Set_LineActive() maps lines 32 .. 63 to the second register group.
 *
 * \details Activates line 50 (DTS wake-up, configurable, register group 32 .. 63)
 *          with interrupt mode and rising edge.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - RTSR2 and IMR2 contain the line 50 bit, group 0 .. 31 registers stay 0.
 */
void Ut_Exti_Set_LineActive_RegGroup32_63_SetsSecondGroupRegisters( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_INTERRUPT, .TriggerEdge = EXTI_TRIGGER_EDGE_RAISING };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineActive( EXTI_LINE_DTS_WKUP, &lineConfig ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_50, EXTI->RTSR2 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_50, EXTI->IMR2 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Set_LineActive() rejects NULL configuration.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 */
void Ut_Exti_Set_LineActive_NullConfig_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, NULL ) );
}


/**
 * \brief   Exti_Set_LineActive() rejects an invalid line identification.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned, no register is written.
 */
void Ut_Exti_Set_LineActive_InvalidLineId_ReturnsErrorWithoutAccess( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = EXTI_LINE_MODE_INTERRUPT, .TriggerEdge = EXTI_TRIGGER_EDGE_BOTH };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_CNT, &lineConfig ) );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
}


/**
 * \brief   Exti_Set_LineActive() rejects an invalid line mode or trigger edge value.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in both cases.
 */
void Ut_Exti_Set_LineActive_InvalidModeOrEdge_ReturnsError( void )
{
    exti_LineConfig_t lineConfig = { .LineMode = (exti_LineMode_t)( EXTI_LINE_MODE_BOTH + 1u ),
                                     .TriggerEdge = EXTI_TRIGGER_EDGE_BOTH };

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );

    lineConfig.LineMode    = EXTI_LINE_MODE_INTERRUPT;
    lineConfig.TriggerEdge = (exti_TriggerEdge_t)( EXTI_TRIGGER_EDGE_NONE + 1u );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineActive( EXTI_LINE_PVD_AVD, &lineConfig ) );
}


/**
 * \brief   Exti_Set_LineInactive() disables masks and trigger of a configurable line.
 *
 * \details Presets IMR1, EMR1, RTSR1, FTSR1 bit of line 16, deactivates the line.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - IMR1, EMR1, RTSR1, FTSR1 of line 16 are cleared, pending flags are not
 *   touched (only \ref Exti_Set_LinePendingClear clears them).
 */
void Ut_Exti_Set_LineInactive_ConfigurableLine_ClearsMasksAndTrigger( void )
{
    EXTI->IMR1  = LL_EXTI_LINE_16;
    EXTI->EMR1  = LL_EXTI_LINE_16;
    EXTI->RTSR1 = LL_EXTI_LINE_16;
    EXTI->FTSR1 = LL_EXTI_LINE_16;
    EXTI->RPR1  = LL_EXTI_LINE_16;
    EXTI->FPR1  = LL_EXTI_LINE_16;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineInactive( EXTI_LINE_PVD_AVD ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->EMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RTSR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->FTSR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->RPR1 );  /* Preset pending kept, not cleared */
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->FPR1 );
}


/**
 * \brief   Exti_Set_LineInactive() disables masks of a direct line without
 *          touching pending flags.
 *
 * \details Presets IMR1, EMR1 and RPR1 bit of line 17, deactivates the line.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - IMR1, EMR1 of line 17 are cleared.
 * - RPR1 keeps the preset bit (direct line has no pending flag in the EXTI).
 */
void Ut_Exti_Set_LineInactive_DirectLine_ClearsMasksOnly( void )
{
    EXTI->IMR1 = LL_EXTI_LINE_17;
    EXTI->EMR1 = LL_EXTI_LINE_17;
    EXTI->RPR1 = LL_EXTI_LINE_17;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LineInactive( EXTI_LINE_RTC ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->IMR1 );
    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->EMR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_17, EXTI->RPR1 );
}


/**
 * \brief   Exti_Set_LineInactive() rejects an invalid line identification.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 */
void Ut_Exti_Set_LineInactive_InvalidLineId_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LineInactive( EXTI_LINE_CNT ) );
}


/**
 * \brief   Exti_Set_LinePendingClear() clears pending flags of a configurable line.
 *
 * \details Clears pending flags of line 16.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned, RPR1 and FPR1 of line 16 are written.
 */
void Ut_Exti_Set_LinePendingClear_ConfigurableLine_ClearsFlags( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Set_LinePendingClear( EXTI_LINE_PVD_AVD ) );

    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->RPR1 );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->FPR1 );
}


/**
 * \brief   Exti_Set_LinePendingClear() rejects a direct line.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned, RPR1 is not written.
 */
void Ut_Exti_Set_LinePendingClear_DirectLine_ReturnsErrorWithoutAccess( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LinePendingClear( EXTI_LINE_RTC ) );

    TEST_ASSERT_EQUAL_HEX32( 0u, EXTI->RPR1 );
}


/**
 * \brief   Exti_Set_LinePendingClear() rejects an invalid line identification.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 */
void Ut_Exti_Set_LinePendingClear_InvalidLineId_ReturnsError( void )
{
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Set_LinePendingClear( EXTI_LINE_CNT ) );
}


/**
 * \brief   Exti_Get_LineType() reports a configurable line.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned, lineType is \ref EXTI_LINE_TYPE_CONFIGURABLE.
 */
void Ut_Exti_Get_LineType_ConfigurableLine_ReturnsConfigurable( void )
{
    exti_LineType_t lineType;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LineType( EXTI_LINE_PVD_AVD, &lineType ) );
    TEST_ASSERT_EQUAL( EXTI_LINE_TYPE_CONFIGURABLE, lineType );
}


/**
 * \brief   Exti_Get_LineType() reports a direct line.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned, lineType is \ref EXTI_LINE_TYPE_DIRECT.
 */
void Ut_Exti_Get_LineType_DirectLine_ReturnsDirect( void )
{
    exti_LineType_t lineType;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LineType( EXTI_LINE_RTC, &lineType ) );
    TEST_ASSERT_EQUAL( EXTI_LINE_TYPE_DIRECT, lineType );
}


/**
 * \brief   Exti_Get_LineType() rejects NULL pointer and invalid line identification.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in both cases.
 */
void Ut_Exti_Get_LineType_NullPtrOrInvalidLineId_ReturnsError( void )
{
    exti_LineType_t lineType;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_LineType( EXTI_LINE_PVD_AVD, NULL ) );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_LineType( EXTI_LINE_CNT, &lineType ) );
}


/**
 * \brief   Exti_Get_LineType() reports a type for every line of the device line.
 *
 * \details Reads the type of every line of \ref exti_LineId_t.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned for every line present on the device line. The
 *   compile-time assertion in Exti.c already checks that the table size matches
 *   \ref EXTI_LINE_CNT of the device line, this test proves every row of the
 *   table is reachable by its line identification at run time (presence /
 *   absence of a line is decided by the device line macros guarding the row).
 */
void Ut_Exti_Get_LineType_AllLines_ReturnsOkForEveryLine( void )
{
    for( uint32_t lineId = 0u; EXTI_LINE_CNT > lineId; lineId++ )
    {
        exti_LineType_t lineType;

        TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LineType( (exti_LineId_t)lineId, &lineType ) );
    }
}


/**
 * \brief   Exti_Get_LinePending() reports rising, falling, both and no edge of a
 *          configurable line.
 *
 * \details Presets RPR1 / FPR1 bit of line 16 in every combination.
 *
 * \par Expected results
 * - EXTI_REQUEST_OK is returned.
 * - Reported edge matches the preset combination, flags are not cleared by the read.
 */
void Ut_Exti_Get_LinePending_ConfigurableLine_ReturnsPendingEdge( void )
{
    exti_TriggerEdge_t pendingEdge;

    EXTI->RPR1 = LL_EXTI_LINE_16;
    EXTI->FPR1 = 0u;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LinePending( EXTI_LINE_PVD_AVD, &pendingEdge ) );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, pendingEdge );
    TEST_ASSERT_EQUAL_HEX32( LL_EXTI_LINE_16, EXTI->RPR1 );  /* Not cleared by the read */

    EXTI->RPR1 = 0u;
    EXTI->FPR1 = LL_EXTI_LINE_16;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LinePending( EXTI_LINE_PVD_AVD, &pendingEdge ) );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, pendingEdge );

    EXTI->RPR1 = LL_EXTI_LINE_16;
    EXTI->FPR1 = LL_EXTI_LINE_16;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LinePending( EXTI_LINE_PVD_AVD, &pendingEdge ) );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_BOTH, pendingEdge );

    EXTI->RPR1 = 0u;
    EXTI->FPR1 = 0u;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_LinePending( EXTI_LINE_PVD_AVD, &pendingEdge ) );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_NONE, pendingEdge );
}


/**
 * \brief   Exti_Get_LinePending() rejects a direct line.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned.
 */
void Ut_Exti_Get_LinePending_DirectLine_ReturnsErrorWithoutAccess( void )
{
    exti_TriggerEdge_t pendingEdge = EXTI_TRIGGER_EDGE_NONE;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_LinePending( EXTI_LINE_RTC, &pendingEdge ) );
}


/**
 * \brief   Exti_Get_LinePending() rejects NULL pointer and invalid line identification.
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in both cases.
 */
void Ut_Exti_Get_LinePending_NullPtrOrInvalidLineId_ReturnsError( void )
{
    exti_TriggerEdge_t pendingEdge;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_LinePending( EXTI_LINE_PVD_AVD, NULL ) );
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Get_LinePending( EXTI_LINE_CNT, &pendingEdge ) );
}

/* =========================== LOCAL FUNCTIONS ============================== */

/** Stub of Nvic_Set_PeriphIrq_Handler - stores registered ISR of EXTI line */
static nvic_RequestState_t Ut_Exti_NvicSetHandlerStub( nvic_PeriphIrqList_t irqId, const nvic_IsrCallback_t irqHandler, int callCnt )
{
    const uint32_t lineId = (uint32_t)irqId - (uint32_t)NVIC_PERIPH_IRQ_EXTI0;

    (void)callCnt;

    TEST_ASSERT_LESS_THAN_UINT32( EXTI_PIN_CNT, lineId );

    utExti_LineIsr[ lineId ] = irqHandler;
    utExti_LastHandlerIrq    = irqId;

    return ( NVIC_REQUEST_OK );
}


/** User callback - records reported edges */
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

    utExti_CallbackCnt++;
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


/** Expects successful GPIO and NVIC configuration of the line */
static void Ut_Exti_Expect_Init( exti_PinId_t pinId, gpio_PortId_t gpioPort, gpio_PinPullCfg_t pull, gpio_PinSpeed_t speed, exti_IrqPrio_t prio )
{
    static gpio_Config_t expectedGpio;
    const nvic_PeriphIrqList_t irqId = (nvic_PeriphIrqList_t)( NVIC_PERIPH_IRQ_EXTI0 + (uint32_t)pinId );

    expectedGpio          = (gpio_Config_t){ 0 };
    expectedGpio.PortId   = gpioPort;
    expectedGpio.PinId    = (gpio_PinId_t)pinId;
    expectedGpio.PinMode  = GPIO_PIN_MODE_INPUT;
    expectedGpio.PinPull  = pull;
    expectedGpio.PinSpeed = speed;

    Gpio_Init_ExpectAndReturn( &expectedGpio, GPIO_REQUEST_OK );
    Nvic_Set_PeriphIrq_Prio_ExpectAndReturn( irqId, prio, NVIC_REQUEST_OK );
    Nvic_Set_PeriphIrq_Active_ExpectAndReturn( irqId, NVIC_REQUEST_OK );
}
