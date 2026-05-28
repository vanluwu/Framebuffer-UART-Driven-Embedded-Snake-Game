# Framebuffer-UART-Driven-Embedded-Snake-Game
An embedded Snake Game implemented on the H-EmbedTKU M4 V2.0 dual-core board. Features high-performance graphics rendering via Linux Framebuffer (/dev/fb0) and real-time core-to-core synchronization via UART communication between i.MX6Q (Q6) and STM32 (M4).

---

## 1. Project Overview

The project migrates the classic Snake Game into an embedded system architecture, partitioning tasks between a high-performance application processor and a real-time co-processor:
* **Main Processor (Q6)**: Runs the core game loop logic, handles graphics rendering, and polls asynchronous user inputs.
* **Co-Processor (M4)**: Manages peripheral status indicators and displays live gameplay data.

---

## 2. System Architecture & Tech Stack

### 💾 Q6 Core (NXP i.MX6Q Cortex-A9 @ Embedded Linux 4.9.17)
* **Direct Graphics Framework**: Utilizing Linux Framebuffer (`/dev/fb0`) mapped directly via `mmap()` system calls into the user space for ultra-low latency pixel manipulation.
* **Asynchronous Input Polling**: Non-blocking character reading from the Linux input subsystem (`/dev/input/event1`) using the `O_NONBLOCK` system flag to keep the game loop fluid.
* **Hardware Sound Engine**: Direct control over the onboard buzzer (`/dev/buzzer`) utilizing `ioctl()` commands to translate musical frequencies into nanosecond-period waveforms for dynamic sound effects.
* **Status Signaling**: Driving system status LEDs through the standard Linux `sysfs` filesystem interface (`/sys/class/gpio`).

### 🎛 M4 Core (STM32F446 Cortex-M4)
* **Real-time Scoring**: Driving 3-digit 7-Segment (FND) display hardware to show live scores ranging from `000` to `999`.
* **State Indicator**: Dynamically updates the auxiliary LCD background color and onboard LEDs based on the game's current Finite State Machine (FSM) state (Green = Play, Blue = Pause, Red = Game Over).

### 🔌 Inter-Core Communication
* Synchronized over a dedicated UART serial line (`/dev/ttymxc1`) running at `115200 bps`.
* Employs a custom, lightweight 5-byte protocol frame to ensure data integrity against line noise:
  $$\text{[STX (0x12)]} \rightarrow \text{[CMD]} \rightarrow \text{[DATA 1]} \rightarrow \text{[DATA 2]} \rightarrow \text{[ETX (0x13)]}$$

---

## 3. Repository & Branch Structure

This repository follows a structured workflow to isolate individual hardware testing modules before unifying them into production:

* **`main`**: The deployment branch containing the integrated, production-ready codebase, unified `Makefile`, and full documentation.
* **`game`**: Core development branch where the primary game engine, coordinate grids ($40 \times 20$), snake body tịnh tiến treading, and collision detection algorithms reside.
* **`test-Q6`**: Sandbox branch dedicated to lower-level Linux driver integrations (`bt_event.c`, `buzzer.c`, `gpio_led.c`).
* **`test-M4`**: Firmware development branch for the STM32 microcontroller handling serial packet decoding and register-level FND/LCD controls (`01_who.c` to `05_button.c`).

---

## 📺 Hardware Demonstration

[![Google Drive Video](https://img.shields.io/badge/Google_Drive-Video_Demo-blue?style=for-the-badge&logo=googledrive&logoColor=white)](https://drive.google.com/file/d/1PfGMIHmzChYyWjLd_FRBK10rbW5rwKRU/view?usp=drive_link)

> 💡 **Note:** Click on the badge above to watch the full real-time multi-core hardware execution on Google Drive.




