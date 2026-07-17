/*
 * Semihosting hello world - minimal semihosting program.
 *
 * This program uses semihosting and requires a debugger probe to run.
 * This program will hang if there is no debugger probe present.
 */

#include <SemihostingStream.h> /* https://github.com/koendv/STM32duino-Semihosting */

SemihostingStream sh;

void setup() {
  sh.println("Hello from ARM CAN Tool!");
}

void loop() {
}

// not truncated
