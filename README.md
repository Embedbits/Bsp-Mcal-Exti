# EXTI MCAL Module

The **EXTI (External Interrupt) MCAL module** provides an abstraction layer for managing external interrupt lines on STM32 microcontrollers.  
This module is part of the MCAL layer and ensures a unified API across different STM32 families.  
Different families are maintained in separate branches; users can switch to the appropriate branch for their MCU family.

> **STM32H5 status**: branch `Dev/STM32H5/PeripheralLines`. GPIO lines 0 .. 15 and
> peripheral event lines 16 .. 66 (interrupt / event mask, edge configuration,
> pending flags) are implemented. Unit tests run on 12 STM32H5 device lines,
> integration tests are compiled only (not run on hardware).

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

### Peripheral Event Lines (STM32H5, lines 16 .. 66)

Peripheral event lines (EXTI lines 16 .. 66, named by the connected peripheral in
`exti_LineId_t`) are either configurable (own edge trigger and pending flag in the
EXTI) or direct (status reported by the peripheral itself, no trigger / pending
flag in the EXTI). The NVIC vector of a peripheral event line belongs to the
module of the peripheral, not to Exti.

- `exti_RequestState_t Exti_Set_LineActive ( exti_LineId_t lineId, const exti_LineConfig_t * const lineConfig );`  
  Activates a peripheral event line as wake-up with interrupt, event or both.

- `exti_RequestState_t Exti_Set_LineInactive ( exti_LineId_t lineId );`  
  Deactivates a peripheral event line.

- `exti_RequestState_t Exti_Set_LinePendingClear ( exti_LineId_t lineId );`  
  Clears the pending flags of a configurable peripheral event line.

- `exti_RequestState_t Exti_Get_LineType ( exti_LineId_t lineId, exti_LineType_t * const lineType );`  
  Reports whether a peripheral event line is configurable or direct.

- `exti_RequestState_t Exti_Get_LinePending ( exti_LineId_t lineId, exti_TriggerEdge_t * const pendingEdge );`  
  Reads the pending edge(s) of a configurable peripheral event line.

`Exti_Set_LineActive` and `Exti_Set_LineInactive` never clear pending flags as a
side effect - `Exti_Set_LinePendingClear` is the only function that clears them.
A line pending from before an activation/deactivation stays pending (sticky,
same as other sticky flags of the project).

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