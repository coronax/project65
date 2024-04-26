# Project:65

This repo contains source code for the Project:65 homebrew computer project. The P:65 is a 6502-based computer project that was first created in 2013. A full description of the project is available at [CJ's Project Blog].

## Contents

- **OS** - The in-ROM OS and monitor. Includes device drivers for IO and disk. Built with the CA65 assembler.
- **DiskController** - Driver for the ATmega328p-based SD-card controller. Build with the Arduino SDK.
- **ExpBlinkenlights** - Blinking lights demo program for the "expansion card", as demonstrated in my [Adding IO Ports] video.
- **Insitu** - Firmware updater program. Downloads a new ROM image via XModem and writes it to the 28c65 EEPROM.

[//]: # 

   [CJ's Project Blog]: <https://coronax.wordpress.com/projects/project65/>
   [CA65]: <https://github.com/cc65/cc65>
   [Adding IO Ports]: <https://www.youtube.com/watch?v=rfc4Z2hQoig>
   
