/**
 * \author Mr.Nobody
 * \file Exti_Port.h
 * \ingroup Exti
 * \brief Exti module public functionality
 *
 * This file contains all available public functionality, any other files shall 
 * not used outside of the module.
 *
 */

#ifndef EXTI_EXTI_PORT_H
#define EXTI_EXTI_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================== INCLUDES ================================== */
#include "Exti_Types.h"                     /* Module types definition        */
/* ============================== TYPEDEFS ================================== */

/* ========================== SYMBOLIC CONSTANTS ============================ */

/* ========================== EXPORTED MACROS =============================== */

/* ========================== EXPORTED VARIABLES ============================ */

/* ========================= EXPORTED FUNCTIONS ============================= */

exti_ModuleVersion_t        Exti_Get_ModuleVersion      ( void );

exti_RequestState_t         Exti_Init                   ( exti_PeriphConfig_t * const extiConfig );
exti_RequestState_t         Exti_Deinit                 ( exti_PeriphConfig_t * const extiConfig );
void                        Exti_Task                   ( void );

exti_RequestState_t         Exti_Get_DefaultConfig      ( exti_PeriphConfig_t * const extiConfig );

#ifdef __cplusplus
}
#endif

#endif /* EXTI_EXTI_PORT_H */

