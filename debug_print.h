#ifndef __DEBUG_PRINT_H__
#define __DEBUG_PRINT_H__

#include <stdint.h>

#define DEBUG     1

void printHex(uint8_t value);
void printBinary(uint8_t value);
void debugPrintBuffer(uint8_t* data, uint8_t bytes);

#endif