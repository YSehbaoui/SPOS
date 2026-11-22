/*! \file
 *  \brief Static scan code set lookup tables and auxiliary functions
 *
 *  \author Lehrstuhl Informatik 11 - RWTH Aachen
 *  \ingroup    keyboard_group
 */

#ifndef KEYB_SCANCODE_H
#define KEYB_SCANCODE_H

#include "keyb_usart.h"

#include <stdint.h>

//! Shift the passed key, i.e. return the key as if the SHIFT-key was pressed
uint16_t keyb_shiftChar(uint16_t character);

//! Translate a byte from the keyboard to an actual character
uint16_t keyb_translateInputToKey(uint8_t input);

//! Translate a byte from the keyboard to the respective key type
uint8_t keyb_translateInputToKeyType(uint8_t input);

//! Modify the meaning of a num key.
void keyb_unNumChar(Key *keyBuffer);

#endif
