/**
 * \author Mr.Nobody
 * \file ItTest_Exti.c
 * \ingroup Exti
 * \brief Integration tests of External interrupt (EXTI) module on target.
 *
 * Exti module runs on the MCU together with real GPIO, NVIC and RCC modules
 * and hardware. Tests verify behavior which cannot be verified by unit tests
 * (emulated registers): connection of the EXTI line to the selected GPIO port,
 * edge detection, interrupt with user callback, pending flag handling and
 * deinitialization.
 *
 * Edges are generated without external wiring - the EXTI pin is an input and
 * its level is changed by the internal pull-up / pull-down resistor
 * (\ref Gpio_Set_PinPull).
 *
 * Used pins (see board configuration below):
 * - IT_EXTI_* - pin not connected on the board (no LED, pull resistor or
 *               solder bridge) and on connectors (floating, pull resistor
 *               defines the level).
 * - IT_EXTI_OTHER_* - pin with the same EXTI line on other port, not connected.
 */

/* ============================= INCLUDES =================================== */
#include "unity.h"                          /* Unity testing framework        */
#include "IntegrationTesting.h"             /* Integration testing on target  */
#include "Exti_Port.h"                      /* Module under test              */
#include "Gpio_Port.h"                      /* Pin level control (pull)       */
/* ============================= TYPEDEFS =================================== */

/* ======================= FORWARD DECLARATIONS ============================= */

static void It_Exti_Init_Line       ( exti_PortId_t portId, exti_TriggerEdge_t triggerEdge, exti_PinPullCfg_t pinPull, exti_ExtiIsrCallback_t *callback );
static void It_Exti_Set_PinPull     ( gpio_PortId_t portId, gpio_PinPullCfg_t pinPull );
static void It_Exti_Wait_Callback   ( uint32_t expectedCnt );
static void It_Exti_Callback        ( exti_TriggerEdge_t triggerEdge );

/* ========================= SYMBOLIC CONSTANTS ============================= */

/*----------------------------- Board configuration --------------------------*/
#if defined(IT_BOARD_NUCLEO_H503RB) || defined(IT_BOARD_NUCLEO_H533RE)

    /** Arduino D7 (PA8) - not connected on the board */
    #define IT_EXTI_PORT                    ( EXTI_PORT_A )
    #define IT_EXTI_PIN                     ( EXTI_PIN_8 )
    #define IT_EXTI_GPIO_PORT               ( GPIO_PORT_A )
    #define IT_EXTI_GPIO_PIN                ( GPIO_PIN_ID_8 )

    /** Arduino D15 (PB8) - same EXTI line on other port, not connected on the board */
    #define IT_EXTI_OTHER_PORT              ( EXTI_PORT_B )
    #define IT_EXTI_OTHER_GPIO_PORT         ( GPIO_PORT_B )

#elif defined(IT_BOARD_NUCLEO_H563ZI) || defined(IT_BOARD_NUCLEO_H5E5ZJ)

    /** Arduino D6 (PE9) - not connected on the board */
    #define IT_EXTI_PORT                    ( EXTI_PORT_E )
    #define IT_EXTI_PIN                     ( EXTI_PIN_9 )
    #define IT_EXTI_GPIO_PORT               ( GPIO_PORT_E )
    #define IT_EXTI_GPIO_PIN                ( GPIO_PIN_ID_9 )

    /** Arduino D14 (PB9) - same EXTI line on other port, not connected on the board */
    #define IT_EXTI_OTHER_PORT              ( EXTI_PORT_B )
    #define IT_EXTI_OTHER_GPIO_PORT         ( GPIO_PORT_B )

#else
    #error "Board of Exti integration tests is not defined (INTEGRATION_TEST_BOARD)."
#endif

/** Count of wait loop iterations until pin level is settled (pull resistor charges pin capacity) */
#define IT_EXTI_SETTLE_LOOPS                ( 2000u )

/** Maximal count of wait loop iterations for callback (timeout) */
#define IT_EXTI_WAIT_LOOPS                  ( 200000u )

/** Size of recorded callback edges */
#define IT_EXTI_EDGE_LOG_SIZE               ( 8u )

/* ============================== MACROS ==================================== */

/* ========================== LOCAL VARIABLES =============================== */

/** Count of user callback calls */
static volatile uint32_t            itExti_CallbackCnt;

/** Edges reported by user callback in order of calls */
static volatile exti_TriggerEdge_t  itExti_EdgeLog[ IT_EXTI_EDGE_LOG_SIZE ];

/* ============================= TEST SETUP ================================= */

void setUp( void )
{
    itExti_CallbackCnt = 0u;

    for( uint32_t logIdx = 0u; IT_EXTI_EDGE_LOG_SIZE > logIdx; logIdx++ )
    {
        itExti_EdgeLog[ logIdx ] = EXTI_TRIGGER_EDGE_BOTH;
    }
}


void tearDown( void )
{
    /* Every test case runs after system reset */
}

/* =============================== TESTS ==================================== */

/*------------------------------ Edge detection ------------------------------*/

/**
 * \brief   Rising edge on the EXTI pin calls user callback.
 *
 * \details Initializes the free board pin as EXTI line with rising edge and
 *          pull-down (low level), then switches pull-up to generate rising edge
 *          and waits for the callback.
 *
 * \par Expected results
 * - User callback is called once with EXTI_TRIGGER_EDGE_RAISING.
 */
void It_Exti_Init_RisingEdge_CallbackWithRisingEdge( void )
{
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_RAISING, EXTI_PIN_PULL_DOWN, It_Exti_Callback );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );

    TEST_ASSERT_EQUAL_UINT32( 1u, itExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, itExti_EdgeLog[ 0u ] );
}


/**
 * \brief   Falling edge on the EXTI pin calls user callback.
 *
 * \details Initializes the line with falling edge and pull-up (high level), then
 *          switches pull-down to generate falling edge.
 *
 * \par Expected results
 * - User callback is called once with EXTI_TRIGGER_EDGE_FALLING.
 */
void It_Exti_Init_FallingEdge_CallbackWithFallingEdge( void )
{
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_FALLING, EXTI_PIN_PULL_UP, It_Exti_Callback );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_DOWN );
    It_Exti_Wait_Callback( 1u );

    TEST_ASSERT_EQUAL_UINT32( 1u, itExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, itExti_EdgeLog[ 0u ] );
}


/**
 * \brief   Line configured for rising edge ignores falling edge.
 *
 * \details Initializes the line with rising edge and pull-up (high level):
 * 1. switches pull-down (falling edge),
 * 2. switches pull-up (rising edge).
 *
 * \par Expected results
 * 1. Callback is not called.
 * 2. Callback is called once with EXTI_TRIGGER_EDGE_RAISING.
 */
void It_Exti_Init_RisingEdge_FallingEdgeIgnored( void )
{
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_RAISING, EXTI_PIN_PULL_UP, It_Exti_Callback );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_DOWN );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 0u, itExti_CallbackCnt );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 1u, itExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, itExti_EdgeLog[ 0u ] );
}


/**
 * \brief   Line configured for both edges reports every edge.
 *
 * \details Initializes the line with both edges and pull-down, then generates
 *          rising, falling and rising edge by pull resistor changes.
 *
 * \par Expected results
 * - Callback is called 3x with edges rising, falling, rising (in this order).
 */
void It_Exti_Init_BothEdges_CallbackForEachEdge( void )
{
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_BOTH, EXTI_PIN_PULL_DOWN, It_Exti_Callback );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_DOWN );
    It_Exti_Wait_Callback( 2u );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 3u );

    TEST_ASSERT_EQUAL_UINT32( 3u, itExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, itExti_EdgeLog[ 0u ] );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_FALLING, itExti_EdgeLog[ 1u ] );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, itExti_EdgeLog[ 2u ] );
}


/**
 * \brief   Exti_Init() configures the GPIO pin as input with selected pull.
 *
 * \details Initializes the line with pull-up and reads the pin configuration and
 *          level by GPIO module after settling time.
 *
 * \par Expected results
 * - Pin mode is input, pull is pull-up.
 * - Pin level is HIGH (floating pin pulled up).
 */
void It_Exti_Init_PinPullUp_PinConfiguredAsInputWithPull( void )
{
    gpio_PinMode_t    pinMode  = GPIO_PIN_MODE_ANALOG;
    gpio_PinPullCfg_t pinPull  = GPIO_PIN_PULL_NONE;
    gpio_PinLevel_t   pinLevel = GPIO_PIN_LEVEL_LOW;

    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_RAISING, EXTI_PIN_PULL_UP, It_Exti_Callback );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( IT_EXTI_GPIO_PORT, IT_EXTI_GPIO_PIN, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_INPUT, pinMode );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinPull( IT_EXTI_GPIO_PORT, IT_EXTI_GPIO_PIN, &pinPull ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_PULL_UP, pinPull );

    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinLevel( IT_EXTI_GPIO_PORT, IT_EXTI_GPIO_PIN, &pinLevel ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_LEVEL_HIGH, pinLevel );
}

/*---------------------- Port selection and pending flags --------------------*/

/**
 * \brief   EXTI line is connected only to the selected port.
 *
 * \details Initializes the line on the other port (same line number), configures
 *          the pin of the same number on the not selected port as input and
 *          generates rising and falling edge on it. Then generates rising edge on
 *          the pin of the selected port.
 *
 * \par Expected results
 * - Edges on the not selected port do not call the callback.
 * - Edge on the selected port calls the callback once with rising edge.
 */
void It_Exti_Init_OtherPortSameLine_EdgeOnPinIgnored( void )
{
    gpio_Config_t gpioConfig = { 0u };

    /* Line connected to other port */
    It_Exti_Init_Line( IT_EXTI_OTHER_PORT, EXTI_TRIGGER_EDGE_BOTH, EXTI_PIN_PULL_DOWN, It_Exti_Callback );

    /* Edges on the pin of the same line number on the not selected port */
    gpioConfig.PortId   = IT_EXTI_GPIO_PORT;
    gpioConfig.PinId    = IT_EXTI_GPIO_PIN;
    gpioConfig.PinMode  = GPIO_PIN_MODE_INPUT;
    gpioConfig.PinPull  = GPIO_PIN_PULL_DOWN;
    gpioConfig.PinSpeed = GPIO_PIN_SPEED_LOW;
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Init( &gpioConfig ) );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_DOWN );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 0u, itExti_CallbackCnt );

    /* Edge on the selected port is detected */
    It_Exti_Set_PinPull( IT_EXTI_OTHER_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 1u, itExti_CallbackCnt );
    TEST_ASSERT_EQUAL( EXTI_TRIGGER_EDGE_RAISING, itExti_EdgeLog[ 0u ] );
}


/**
 * \brief   Edge without user callback is handled and its pending flag cleared.
 *
 * \details
 * 1. Initializes the line without callback and generates rising edge.
 * 2. Initializes the line again with callback (pull-down) and waits.
 * 3. Generates rising edge.
 *
 * \par Expected results
 * - No crash in step 1 (NULL callback is not called).
 * - Step 2: callback not called - no pending edge from previous configuration.
 * - Step 3: callback called once.
 */
void It_Exti_Init_WithoutCallback_EdgeHandledWithoutCallback( void )
{
    /* Edge without callback - flag is cleared by the handler */
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_RAISING, EXTI_PIN_PULL_DOWN, EXTI_NULL_PTR );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );

    /* Re-initialization with callback - no pending edge from previous configuration */
    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_RAISING, EXTI_PIN_PULL_DOWN, It_Exti_Callback );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 0u, itExti_CallbackCnt );

    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    TEST_ASSERT_EQUAL_UINT32( 1u, itExti_CallbackCnt );
}

/*---------------------------- Deinitialization ------------------------------*/

/**
 * \brief   Exti_Deinit() disables the line and returns the pin to analog mode.
 *
 * \details Initializes the line with both edges, deinitializes it, reads pin mode,
 *          then sets the pin as input again and generates rising and falling edge.
 *
 * \par Expected results
 * - Exti_Deinit() returns EXTI_REQUEST_OK, pin mode is analog.
 * - Edges after deinitialization do not call the callback.
 */
void It_Exti_Deinit_InitializedLine_EdgesIgnoredPinAnalog( void )
{
    exti_PeriphConfig_t extiConfig;
    gpio_PinMode_t      pinMode = GPIO_PIN_MODE_INPUT;

    It_Exti_Init_Line( IT_EXTI_PORT, EXTI_TRIGGER_EDGE_BOTH, EXTI_PIN_PULL_DOWN, It_Exti_Callback );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_DefaultConfig( &extiConfig ) );
    extiConfig.ExtiPin  = IT_EXTI_PIN;
    extiConfig.ExtiPort = IT_EXTI_PORT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Deinit( &extiConfig ) );

    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Get_PinMode( IT_EXTI_GPIO_PORT, IT_EXTI_GPIO_PIN, &pinMode ) );
    TEST_ASSERT_EQUAL( GPIO_PIN_MODE_ANALOG, pinMode );

    /* Input buffer enabled again, edges must not be reported */
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Set_PinMode( IT_EXTI_GPIO_PORT, IT_EXTI_GPIO_PIN, GPIO_PIN_MODE_INPUT ) );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_UP );
    It_Exti_Wait_Callback( 1u );
    It_Exti_Set_PinPull( IT_EXTI_GPIO_PORT, GPIO_PIN_PULL_DOWN );
    It_Exti_Wait_Callback( 1u );

    TEST_ASSERT_EQUAL_UINT32( 0u, itExti_CallbackCnt );
}


/**
 * \brief   Exti_Init() on target rejects invalid configurations.
 *
 * \details Calls Exti_Init() with NULL configuration, invalid pin and priority
 *          0xFFFF (out of NVIC range).
 *
 * \par Expected results
 * - EXTI_REQUEST_ERROR is returned in all cases (priority rejected by NVIC module).
 */
void It_Exti_Init_InvalidConfig_ReturnsError( void )
{
    exti_PeriphConfig_t extiConfig;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( EXTI_NULL_PTR ) );

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_DefaultConfig( &extiConfig ) );
    extiConfig.ExtiPin = EXTI_PIN_CNT;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &extiConfig ) );

    /* Priority out of range is rejected by NVIC module */
    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_DefaultConfig( &extiConfig ) );
    extiConfig.ExtiPin      = IT_EXTI_PIN;
    extiConfig.ExtiPort     = IT_EXTI_PORT;
    extiConfig.ExtiPriority = 0xFFFFu;
    TEST_ASSERT_EQUAL( EXTI_REQUEST_ERROR, Exti_Init( &extiConfig ) );
}

/* ========================== LOCAL FUNCTIONS =============================== */

/**
 * \brief Initializes EXTI line IT_EXTI_PIN on required port and waits until pin level is settled.
 *
 * \param portId      [in]: EXTI port
 * \param triggerEdge [in]: Trigger edge(s)
 * \param pinPull     [in]: Pull resistor (initial pin level)
 * \param callback    [in]: User callback, can be NULL
 */
static void It_Exti_Init_Line( exti_PortId_t portId, exti_TriggerEdge_t triggerEdge, exti_PinPullCfg_t pinPull, exti_ExtiIsrCallback_t *callback )
{
    exti_PeriphConfig_t extiConfig;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Get_DefaultConfig( &extiConfig ) );
    extiConfig.ExtiPin         = IT_EXTI_PIN;
    extiConfig.ExtiPort        = portId;
    extiConfig.ExtiPinPull     = pinPull;
    extiConfig.ExtiTriggerEdge = triggerEdge;
    extiConfig.ExtiCallback    = callback;

    TEST_ASSERT_EQUAL( EXTI_REQUEST_OK, Exti_Init( &extiConfig ) );

    /* Edge caused by the pin configuration itself is not part of the tests */
    It_Exti_Wait_Callback( IT_EXTI_EDGE_LOG_SIZE );
    itExti_CallbackCnt = 0u;
}


/**
 * \brief Changes pin level by pull resistor (pin is input).
 *
 * \param portId  [in]: GPIO port of the pin with IT_EXTI_GPIO_PIN number
 * \param pinPull [in]: Pull resistor
 */
static void It_Exti_Set_PinPull( gpio_PortId_t portId, gpio_PinPullCfg_t pinPull )
{
    TEST_ASSERT_EQUAL( GPIO_REQUEST_OK, Gpio_Set_PinPull( portId, IT_EXTI_GPIO_PIN, pinPull ) );
}


/**
 * \brief Waits until callback is called required count of times or timeout
 *        elapses (also used as settling time of pin level).
 *
 * \param expectedCnt [in]: Required count of callback calls
 */
static void It_Exti_Wait_Callback( uint32_t expectedCnt )
{
    for( volatile uint32_t loopIdx = 0u; ( IT_EXTI_WAIT_LOOPS > loopIdx ) && ( expectedCnt > itExti_CallbackCnt ); loopIdx++ )
    {
        /* Busy wait */
    }

    for( volatile uint32_t loopIdx = 0u; IT_EXTI_SETTLE_LOOPS > loopIdx; loopIdx++ )
    {
        /* Settling of pin level, no further edge expected */
    }
}


/**
 * \brief User callback - records reported edges.
 *
 * \param triggerEdge [in]: Detected edge
 */
static void It_Exti_Callback( exti_TriggerEdge_t triggerEdge )
{
    if( IT_EXTI_EDGE_LOG_SIZE > itExti_CallbackCnt )
    {
        itExti_EdgeLog[ itExti_CallbackCnt ] = triggerEdge;
    }
    else
    {
        /* Log is full */
    }

    itExti_CallbackCnt++;
}
