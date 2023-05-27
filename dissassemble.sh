#!/bin/bash
avr-objdump -S -z -m avr5 /home/james/Arduino/USBTesting/build/arduino.avr.nano/USBTesting.ino.elf > asm_dump.avr

