# EXTI MCAL Module

The **EXTI (External Interrupt) MCAL module** provides an abstraction layer for managing external interrupt lines on STM32 microcontrollers.  
This module is part of the MCAL layer and ensures a unified API across different STM32 families.  
Different families are maintained in separate branches; users can switch to the appropriate branch for their MCU family.

---

## Features

- Initialization and deinitialization of EXTI peripheral
- Task handler for periodic servicing
- Support for default configuration retrieval
- Standardized request state return values
- Versioning support for module management

---

## Public API

### Module Information

- ```c
  exti_ModuleVersion_t Exti_Get_ModuleVersion ( void );
  ```  
  Returns the current version of the EXTI module.

---

### Initialization

  ```c
- exti_RequestState_t Exti_Init ( exti_PeriphConfig_t * const extiConfig );
  ```  
  Initializes the EXTI peripheral with the provided configuration.

  ```c
- exti_RequestState_t Exti_Deinit ( exti_PeriphConfig_t * const extiConfig );
  ```  
  Deinitializes the EXTI peripheral and resets the configuration.

  ```c
- void Exti_Task ( void );
  ```  
  Handles EXTI-related periodic tasks (if required by the implementation).

  ```c
- exti_RequestState_t Exti_Get_DefaultConfig ( exti_PeriphConfig_t * const extiConfig );
  ```  
  Retrieves a default configuration structure for EXTI initialization.

---

## Usage Notes

- The EXTI module is hardware dependent and must be configured per STM32 family branch.  
- The **default configuration API** helps to ensure safe initialization.  
- The `Task` function should be periodically called if asynchronous handling or background processing is implemented.  

---
