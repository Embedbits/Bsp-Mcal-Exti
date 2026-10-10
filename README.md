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

- `exti_ModuleVersion_t Exti_Get_ModuleVersion ( void );`  
  Returns the current version of the EXTI module.

---

### Initialization

- `exti_RequestState_t Exti_Init ( exti_PeriphConfig_t * const extiConfig );`  
  Initializes the EXTI peripheral with the provided configuration.

- `exti_RequestState_t Exti_Deinit ( exti_PeriphConfig_t * const extiConfig );`  
  Deinitializes the EXTI peripheral and resets the configuration.

- `void Exti_Task ( void );`  
  Handles EXTI-related periodic tasks (if required by the implementation).

- `exti_RequestState_t Exti_Get_DefaultConfig ( exti_PeriphConfig_t * const extiConfig );`  
  Retrieves a default configuration structure for EXTI initialization.

---

## Usage Notes

- The EXTI module is hardware dependent and must be configured per STM32 family branch.  
- The **default configuration API** helps to ensure safe initialization.  
- The `Task` function should be periodically called if asynchronous handling or background processing is implemented.  
- STM32H7: the GPIO port of the line is selected in SYSCFG EXTICR, lines 5 - 9 share NVIC vector EXTI9_5 and
  lines 10 - 15 share EXTI15_10 (priority of the last initialized line of the group).  
- STM32H7R / H7S (Ral family STM32H7RS): the port is selected in SBS EXTICR (SBS clock), every line 0 - 15 has its
  own NVIC vector EXTI0 - EXTI15, ports A - H and M - P.  

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