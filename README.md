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

## STM32L4 / STM32L4+ specifics

- GPIO lines 0 - 15 of ports A - I (ports guarded by device header). The port is selected in
  SYSCFG EXTICR - SYSCFG clock is activated by `Exti_Init()`.
- One pending register (PR1) for both edges. With both edges configured the reported edge is
  derived from the pin level in the ISR (high - rising, low - falling).
- Lines 5 - 9 share NVIC vector EXTI9_5, lines 10 - 15 share EXTI15_10. The priority of a shared
  vector is the priority of the last initialized line, the vector is disabled by `Exti_Deinit()`
  only when no other line of the group is enabled.
- Every ISR ends with DSB (Cortex-M4 r0p1 erratum 838869).

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

# 🛠 CMake Integration

```cmake
target_link_libraries(User_Lib PRIVATE Exti_Lib)
```

---

## License

This project is licensed under the **Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**.

You are free to use, modify, and share this work for **non-commercial purposes**, provided appropriate credit is given.

See [LICENSE.md](LICENSE.md) for full terms or visit [creativecommons.org/licenses/by-nc/4.0](https://creativecommons.org/licenses/by-nc/4.0/).

---

## Authors

- **Mr.Nobody** — [embedbits.com](https://embedbits.com)

Contributions are welcome! Please open a pull request.

---

## 🌐 Useful Links

- [STM32CubeIDE](https://www.st.com/en/development-tools/stm32cubeide.html)
- [Azure DevOps](https://azure.microsoft.com/en-us/services/devops/)
- [Embedbits Github](https://github.com/Embedbits)
- [CC BY-NC 4.0 License](https://creativecommons.org/licenses/by-nc/4.0/)