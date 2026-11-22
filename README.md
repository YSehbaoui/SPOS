# SPOS

A small preemptive operating system for the AVR **ATmega644** (20 MHz), written in C. Built as part of the course *Praktikum Systemprogrammierung* at RWTH Aachen University.

> Educational project. It builds on materials provided by Lehrstuhl Informatik 11 (RWTH Aachen) for this course. Those materials remain the property of their authors.

## Features

- **Process management**: up to 8 concurrent processes, process creation (`os_exec`), killing, priorities, and stack checksums
- **Scheduler** with swappable strategies: Even, Random, Round Robin, Run to Completion, Inactive Aging, Multi-Level Feedback Queue
- **Heap allocator** with per-process ownership and swappable strategies: First Fit, Next Fit, Best Fit, Worst Fit
- **Shared memory** with read and write access control, plus `os_realloc`
- **Task manager** on the LCD: start and kill programs, change priorities, switch scheduling and allocation strategies, inspect and erase heaps
- **User privileges** system that decides which actions the task manager may perform
- **Keyboard input** via USART with a layered scancode parser
- **LCD driver** and button input
- **SPI driver** and a memory driver abstraction
- **Autostart** of programs at boot (`REGISTER_AUTOSTART`), including a speed-typing demo program

## Installation

### Requirements

- `avr-gcc`, `avr-libc`, `avr-binutils`
- `make`
- `avrdude` (only for flashing)

On Arch:

```bash
sudo pacman -S avr-gcc avr-libc avr-binutils make avrdude
```

### Build

```bash
git clone https://github.com/YSehbaoui/SPOS.git
cd SPOS
make          # release build -> SPOS.elf
make debug    # debug build
make clean
```

The build uses `-Werror`, so warnings fail the build.

### Flash

Adjust the programmer (`-c`) to your hardware:

```bash
avr-objcopy -O ihex -R .eeprom SPOS.elf SPOS.hex
avrdude -p m644 -c <programmer> -U flash:w:SPOS.hex
```

Alternatively, open `SPOS/SPOS.cproj` in Microchip Studio (formerly Atmel Studio).

### Hardware

An ATmega644 at 20 MHz with an LCD, buttons, and a keyboard connected via USART.
