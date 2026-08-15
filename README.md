# Security Service STM32

## Overview

This project is a firmware solution for the STM32F411VET6 microcontroller, designed for a fingerprint-based security access control system. The controller identifies users through a fingerprint module, displays system status on a 128x64 OLED screen, communicates with a backend/BBB via UART, and is operated using a user button.

The main objectives of the system are to:

- Scan and match user fingerprints
- Allow enrollment of new fingerprints
- Display operational status clearly on the OLED screen
- Temporarily or permanently block access when multiple invalid attempts are detected
- Synchronize time and member data from the BBB
- Send system status back to the backend for further processing

---

## Basic Project Information

### MCU and Hardware Platform

- Microcontroller: STM32F411VET6
- Chip family: STM32F4
- Clock system: uses PLL from HSI and runs at an effective CPU frequency suitable for real-time embedded control
- Compiler / IDE: supports STM32CubeIDE and MDK-ARM builds

### Main Peripherals

- I2C1: communication with the SSD1306 OLED display
- USART1: UART communication with the BBB or central management module
- USART2: UART communication with the fingerprint sensor module
- GPIO: user button, LEDs, and status flags
- FreeRTOS: runs concurrent tasks in the system

### Development Environment

- STM32CubeMX / STM32CubeIDE
- ARM MDK-ARM
- FreeRTOS v10
- SEGGER RTT / SEGGER SystemView for debugging and tracing
- STMicroelectronics HAL drivers

---

## Software Architecture

The project is organized into clear embedded-system modules:

- Core/Src/main.c
  - Initializes the system
  - Configures the clock
  - Initializes UART, I2C, and GPIO
  - Creates and starts FreeRTOS tasks

- Core/Src/Task_FingerPrint/
  - Manages the fingerprint sensor FSM
  - Sends commands to the fingerprint module
  - Receives and parses response packets
  - Handles enrollment, search, lock, and unlock flows

- Core/Src/Task_ParsingData/
  - Parses data received from the fingerprint module and BBB
  - Verifies checksums
  - Converts timestamps, member names, and security states

- Core/Src/Comm_BBB/
  - Handles UART communication with the BBB
  - Sends system states such as successful verification, failure, new fingerprint registration, and lock events
  - Receives data from the BBB, including timestamps, member names, enrollment ID assignments, and unlock commands

- Core/Src/Task_Display/
  - Manages the OLED display
  - Drives standby, scanning, pass, fail, lock, and enrollment screens

- Core/Src/Task_UserButton/
  - Monitors the user button
  - Triggers fingerprint enrollment when held for more than 3 seconds

---

## Main FreeRTOS Tasks

### 1. Button_Task

- Reads the state of button PA0
- If the button is held for 3 seconds or more, it starts the new fingerprint enrollment process
- Runs periodically with a 50 ms polling interval

### 2. ParsingRXData_Task

- Waits for notifications from the UART data stream
- Extracts data from the fingerprint module
- Validates checksum
- Forward data to the fingerprint processing logic or BBB parsing logic

### 3. Fingerprint_StateMachine_Task

- Main logic engine of the system
- Runs all fingerprint FSM states:
  - image capture
  - feature extraction
  - library search
  - fingerprint confirmation
  - enrollment
  - temporary/permanent lock handling

### 4. Display_Task

- Updates the OLED status based on the FSM state
- Displays:
  - standby
  - scanning
  - pass
  - fail
  - temporary lock
  - infinite lock
  - enrollment process

---

## Main Project Features

### 1. Fingerprint Recognition

- Sends GEN_IMG commands to capture an image
- Converts the image into a template using IMG_2_TZ
- Searches the fingerprint library with a 1:N SEARCH operation
- Extracts ID and match score
- Handles states such as success, no finger detected, incorrect fingerprint, and previously confirmed fingerprint

### 2. New Fingerprint Enrollment

- The user holds the button to start enrollment
- The STM32 requests a new ID from the BBB for the new user
- The process includes:
  - first fingerprint capture
  - first feature extraction
  - remove finger request
  - second fingerprint capture
  - second feature extraction
  - model generation
  - model storage in sensor flash
- On success, the system reports the newly added fingerprint and displays a pass screen

### 3. System Lock Management

The system includes lock logic based on the number of failed recognition attempts:

- 5 failed attempts: 5-minute lock
- 10 failed attempts: 10-minute lock
- 15 failed attempts: infinite lock

When locked:

- The OLED shows a countdown timer or infinite lock message
- Information is sent to the BBB
- The system waits for unlock instructions from the BBB

### 4. Communication with the BBB

- UART1 is used to communicate with the BBB
- Transmitted data includes:
  - system state
  - matched ID
  - confirmation state
  - enrollment ID request
  - enrollment error notification

- Data received from the BBB includes:
  - timestamps in the form #TS=...
  - member name information during user search states
  - IDs available for new enrollment
  - unlock command for infinite lock state

### 5. OLED SSD1306 Display

The OLED screen shows different states, including:

- Standby: time and date, ready-to-scan status
- Scanning: fingerprint scanning animation
- Pass: welcome message with member name
- Fail: retry message
- Temporary lock: remaining time countdown
- Infinite lock: system lock warning
- Enrollment: step-by-step fingerprint registration flow

### 6. Time and User Data Synchronization

- When a timestamp frame is received from the BBB, the system converts Unix timestamp into real-world time and updates the standby display
- When a member name is received, the system stores the corresponding user name for display after successful verification

### 7. Debugging and Tracing

- SEGGER RTT is used to log debug information to the terminal
- SEGGER SystemView is used to trace tasks and real-time system activity
- Logs are generated for command sending, packet reception, timeouts, lock states, enrollment, and pass/fail results

---

## Main Operating Flow

1. The MCU boots and configures clock, UART, I2C, and GPIO
2. FreeRTOS tasks are created
3. Time tracking and communication are initialized
4. The fingerprint sensor enters scanning mode
5. When a finger is placed on the sensor:
   - capture the image
   - create a template
   - perform library search
6. If a matching ID is found:
   - display pass state
   - send status to the BBB
7. If no match is found:
   - display fail state
   - increment failed-attempt count
   - trigger temporary or permanent lock if necessary
8. If the user presses and holds the button:
   - begin a new enrollment flow
9. When enrollment is complete:
   - send the result to the BBB
   - return to standby mode

---

## Main Directory Structure

```text
Security_service_STM32/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       ├── main.c
│       ├── Comm_BBB/
│       ├── Task_Display/
│       ├── Task_FingerPrint/
│       ├── Task_ParsingData/
│       └── Task_UserButton/
├── Drivers/
│   ├── CMSIS/
│   └── STM32F4xx_HAL_Driver/
├── 3rdParty/
│   └── FreeRTOS/
├── MDK-ARM/
├── STM32CubeIDE/
├── Security_service_STM32.ioc
├── STM32F411VETX_FLASH.ld
├── STM32F411VETX_RAM.ld
└── README_EN.md
```

---

## Build and Run Guide

### With STM32CubeIDE

1. Open the project in STM32CubeIDE
2. Select the correct toolchain and STM32F411VETx target definition
3. Build the project
4. Flash the firmware to the board via ST-Link or J-Link

### With Keil MDK

1. Open the .uvprojx file in the MDK-ARM folder
2. Build the project
3. Flash the firmware to the board

> Note: the project already includes J-Link / ST-Link configuration; ensure the debugger and boot mode match the hardware setup.

---

## Technical Notes

- The source code is based on STM32 HAL programming
- The system follows an event-driven, task-based model with a real-time operating system
- Fingerprint communication uses a packet protocol with header, address, packet ID, length, payload, and checksum
- Each task has a distinct responsibility: data reception, parsing, FSM handling, and display management
- This project focuses on a fingerprint-based access control system for gate, warehouse, or security-zone environments

---

## Conclusion

This project is a complete embedded application for implementing a fingerprint recognition access control system, monitoring status on an OLED display, communicating with a BBB, and controlling access using lock logic. It demonstrates the integration of STM32 microcontroller control, FreeRTOS, UART/I2C communication, and complex fingerprint FSM logic.
