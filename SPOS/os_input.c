#include "os_input.h"

#include <avr/io.h>
#include <stdint.h>

/*! \file

Everything that is necessary to get the input from the Buttons in a clean format.

*/

/*!
 *  A simple "Getter"-Function for the Buttons on the evaluation board.\n
 *
 *  \returns The state of the button(s) in the lower bits of the return value.\n
 *  example: 1 Button:  -pushed:   00000001
 *                      -released: 00000000
 *           4 Buttons: 1,3,4 -pushed: 000001101
 *
 */
uint8_t os_getInput(void) {
	uint8_t rawInput = ~PINC & 0b11000011; // So, hier ist wirklich der richtige Punkt fuer die generelle Input-Funktion.
	uint8_t cookedInput = ((rawInput & 0b00000011) | (rawInput >> 4));
	return cookedInput;
}

/*!
 *  Initializes DDR and PORT for input
 */
void os_initInput() {
	DDRC &= 0b00111100; //Da tut sich ja nix dran
	PORTC |= 0b11000011;
}

/*!
 *  Endless loop as long as at least one button is pressed.
 */
void os_waitForNoInput() {
	while(os_getInput());
}

/*!
 *  Endless loop until at least one button is pressed.
 */
void os_waitForInput() {
	while(!os_getInput());
}
