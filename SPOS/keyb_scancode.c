#include "keyb_scancode.h"

#include "keyb_usart.h"
#include "util.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*!
 *  These char arrays are used as lookup tables for the translation of
 *  unparsed values from the keyboard. Only scan code set 3 is supported.
 *  Notice that the interpretation is completely coded within these tables. All functions
 *  in this module simply apply these lookup tables in an apt way.
 *  Therefore, implementing further scan code sets could be done by defining new lookup tables.
 */


/*!
 *  Decode the scan code set into a pseudo meaningful char set.
 *  This translation only makes sense with the appropriate types.
 */
const uint16_t PROGMEM keyb_tb_scancodeset3[] = {
    [0x07] = KEYB_KEY_FKEY(1),
    [0x08] = KEYB_KEY_CTRL_ESC,
    [0x0D] = KEYB_KEY_CTRL_TAB,
    [0x0E] = '^',
    [0x0F] = KEYB_KEY_FKEY(2),
    [0x11] = KEYB_KEY_CTRL_CTRL,
    [0x12] = KEYB_KEY_CTRL_SHIFT,
    [0x13] = '<',
    [0x14] = KEYB_KEY_CTRL_CAPS,
    [0x15] = 'q',
    [0x16] = '1',
    [0x17] = KEYB_KEY_FKEY(3),
    [0x19] = KEYB_KEY_CTRL_ALT,
    [0x1A] = 'y',
    [0x1B] = 's',
    [0x1C] = 'a',
    [0x1D] = 'w',
    [0x1E] = '2',
    [0x1F] = KEYB_KEY_FKEY(4),
    [0x21] = 'c',
    [0x22] = 'x',
    [0x23] = 'd',
    [0x24] = 'e',
    [0x25] = '4',
    [0x26] = '3',
    [0x27] = KEYB_KEY_FKEY(5),
    [0x29] = ' ',
    [0x2A] = 'v',
    [0x2B] = 'f',
    [0x2C] = 't',
    [0x2D] = 'r',
    [0x2E] = '5',
    [0x2F] = KEYB_KEY_FKEY(6),
    [0x31] = 'n',
    [0x32] = 'b',
    [0x33] = 'h',
    [0x34] = 'g',
    [0x35] = 'z',
    [0x36] = '6',
    [0x37] = KEYB_KEY_FKEY(7),
    [0x39] = KEYB_KEY_CTRL_ALT,
    [0x3A] = 'm',
    [0x3B] = 'j',
    [0x3C] = 'u',
    [0x3D] = '7',
    [0x3E] = '8',
    [0x3F] = KEYB_KEY_FKEY(8),
    [0x41] = ',',
    [0x42] = 'k',
    [0x43] = 'i',
    [0x44] = 'o',
    [0x45] = '0',
    [0x46] = '9',
    [0x47] = KEYB_KEY_FKEY(9),
    [0x49] = '.',
    [0x4A] = '-',
    [0x4B] = 'l',
    [0x4C] = 0xC3B6, // Should be 'ö' or 0xC3B6
    [0x4D] = 'p',
    [0x4E] = 0xC39F, // Should be 'ß' or 0xC39F
    [0x4F] = KEYB_KEY_FKEY(10),
    [0x52] = 0xC3A4, // Should be 'ä' or 0xC3A4
    [0x53] = '#',
    [0x54] = 0xC3BC, // Should be 'ü' or 0xC3BC
    [0x55] = '\'',   // Should be '´' but the LCD can't display it
    [0x56] = KEYB_KEY_FKEY(11),
    [0x57] = KEYB_KEY_CTRL_PRINT,
    [0x58] = KEYB_KEY_CTRL_CTRL,
    [0x59] = KEYB_KEY_CTRL_SHIFT,
    [0x5A] = KEYB_KEY_CTRL_ENTER,
    [0x5B] = '+',
    [0x5C] = '#',
    [0x5E] = KEYB_KEY_FKEY(12),
    [0x5F] = KEYB_KEY_CTRL_ROLL,
    [0x60] = KEYB_KEY_ARROW_DOWN,
    [0x61] = KEYB_KEY_ARROW_LEFT,
    [0x62] = KEYB_KEY_CTRL_PAUSE,
    [0x63] = KEYB_KEY_ARROW_UP,
    [0x64] = KEYB_KEY_CTRL_DELETE,
    [0x65] = KEYB_KEY_CTRL_END,
    [0x66] = KEYB_KEY_CTRL_BACK,
    [0x67] = KEYB_KEY_CTRL_INSERT,
    [0x69] = '1',
    [0x6A] = KEYB_KEY_ARROW_RIGHT,
    [0x6B] = '4',
    [0x6C] = '7',
    [0x6D] = KEYB_KEY_CTRL_PAGEDOWN,
    [0x6E] = KEYB_KEY_CTRL_HOME,
    [0x6F] = KEYB_KEY_CTRL_PAGEUP,
    [0x70] = '0',
    [0x71] = ',',
    [0x72] = '2',
    [0x73] = '5',
    [0x74] = '6',
    [0x75] = '8',
    [0x76] = KEYB_KEY_CTRL_NUM,
    [0x77] = '/',
    [0x79] = KEYB_KEY_CTRL_ENTERPAD,
    [0x7A] = '3',
    [0x7C] = '+',
    [0x7D] = '9',
    [0x7E] = '*',
    [0x84] = '-',
    [0x8B] = KEYB_KEY_CTRL_APPS,
    [0x8C] = KEYB_KEY_CTRL_APPS,
    [0x8D] = KEYB_KEY_CTRL_MENU,
};
#define KEYB_TB_SCANCODESET3_LEN (sizeof(keyb_tb_scancodeset3) / sizeof(keyb_tb_scancodeset3[0]))

/*!
 *  Decode the scan code set into the types appropriate to the pseudo char set.
 */
const char PROGMEM keyb_tb_scancodeset3_types[] = "\
       53    315\
 3313115 3111115\
 1111115 1111115\
 1111115 4111115\
 1111115 1111115\
  11115444411 54\
66464444 2622444\
22222242 422222 \
    2      344  ";
#define KEYB_TB_SCANCODESET3_TYPES_LEN (sizeof(keyb_tb_scancodeset3_types) / sizeof(keyb_tb_scancodeset3_types[0]))

/*!
 *  Shift the keys according to the standard layout.
 *  Applying this lookup multiple times will switch through the different key meanings.
 *  e.g. shift('q') will be 'Q', and shift('Q') == shift(shift('q')) will be '@'.
 *  This translation does not come with an extra type shifter, since the type is not affected
 *  by the shift.
 */
const uint16_t PROGMEM keyb_tb_universalShifter[] = {
    // Shift
    ['^'] = 0xC2B0, // Should be '°' or 0xC2B0
    ['0'] = '=',
    ['1'] = '!',
    ['2'] = '"',
    ['3'] = 0, // Should be '§' but the LCD can't display it
    ['4'] = '$',
    ['5'] = '%',
    ['6'] = '&',
    ['7'] = '/',
    ['8'] = '(',
    ['9'] = ')',
    ['S'] = '?',  // Index should be 'ß' or 0xC39F
    ['\''] = '`', // Index should be '´' but the LCD can't display it
    ['#'] = '\'',
    ['+'] = '*',
    ['a'] = 'A',
    ['b'] = 'B',
    ['c'] = 'C',
    ['d'] = 'D',
    ['e'] = 'E',
    ['f'] = 'F',
    ['g'] = 'G',
    ['h'] = 'H',
    ['i'] = 'I',
    ['j'] = 'J',
    ['k'] = 'K',
    ['l'] = 'L',
    ['m'] = 'M',
    ['n'] = 'N',
    ['o'] = 'O',
    ['p'] = 'P',
    ['q'] = 'Q',
    ['r'] = 'R',
    ['s'] = 'S',
    ['t'] = 'T',
    ['u'] = 'U',
    ['v'] = 'V',
    ['w'] = 'W',
    ['x'] = 'X',
    ['y'] = 'Y',
    ['z'] = 'Z',
    ['<'] = '>',
    [','] = ';',
    ['.'] = ':',
    ['-'] = '_',
    // Alt
    ['/'] = '{',
    ['('] = '[',
    [')'] = ']',
    ['='] = '}',
    ['?'] = '\\',
    ['Q'] = '@',
    ['E'] = 0xC2A5, // Should be '€' but the LCD can't display it. Replaced with '¥'
    ['*'] = '~',
    ['>'] = '|',
};
#define KEYB_TB_UNIVERSALSHIFTER_LEN (sizeof(keyb_tb_universalShifter) / sizeof(keyb_tb_universalShifter[0]))

/*!
 *  Invert the numpad values to pseudo command representations.
 *  This translation is only meaningful with the appropriate type translation.
 */
const uint8_t PROGMEM keyb_tb_universalNumpadInverter[] = {
    ['*'] = '*',
    ['+'] = '+',
    [','] = KEYB_KEY_CTRL_DELETE,
    ['-'] = '-',
    ['/'] = '/',
    ['0'] = KEYB_KEY_CTRL_INSERT,
    ['1'] = KEYB_KEY_CTRL_END,
    ['2'] = KEYB_KEY_ARROW_DOWN,
    ['3'] = KEYB_KEY_CTRL_PAGEDOWN,
    ['4'] = KEYB_KEY_ARROW_LEFT,
    ['5'] = KEYB_KEY_CTRL_NONE,
    ['6'] = KEYB_KEY_ARROW_RIGHT,
    ['7'] = KEYB_KEY_CTRL_HOME,
    ['8'] = KEYB_KEY_ARROW_UP,
    ['9'] = KEYB_KEY_CTRL_PAGEUP,
};
#define KEYB_TB_UNIVERSALNUMPADINVERTER_LEN (sizeof(keyb_tb_universalNumpadInverter) / sizeof(keyb_tb_universalNumpadInverter[0]))

/*!
 *  This translation generates the appropriate types for the universalNumpadInverter.
 */
const char PROGMEM keyb_tb_universalNumpadInverter_types[] = "\
                \
                \
          2242 2\
4464646464      ";
#define KEYB_TB_UNIVERSALNUMPADINVERTER_TYPES_LEN (sizeof(keyb_tb_universalNumpadInverter_types) / sizeof(keyb_tb_universalNumpadInverter_types[0]))


/*!
 *  Takes a byte produced by layer 2 and translates it to the appropriate key.
 *  \param input    The input to be translated.
 *  \returns        The appropriate key.
 */
uint16_t keyb_translateInputToKey(uint8_t input) {
    if (input < KEYB_TB_SCANCODESET3_LEN) {
        return pgm_read_word(&keyb_tb_scancodeset3[input]);
    }
    return 0;
}

/*!
 *  Takes a byte produced by layer 2 and translates it to the appropriate key type.
 *  \param input    The input to be translated.
 *  \returns        The appropriate key type.
 */
uint8_t keyb_translateInputToKeyType(uint8_t input) {
    if (input < KEYB_TB_SCANCODESET3_TYPES_LEN) {
        return pgm_read_byte(&keyb_tb_scancodeset3_types[input]) - '0';
    }
    return 0xFF;
}

/*!
 *  A function that shifts the passed char.
 *  Applying this function once will modify the passed value like the SHIFT-key would.
 *  Applying this function another time will return a value that would be expected if Alt Graph was pressed.
 *
 *  \param character The (class 1) character to shift.
 *  \return the character after shifting it.
 */
uint16_t keyb_shiftChar(uint16_t character) {
    switch (character) {
        case 0xC39F /*'ß'*/: return '?';
        case 0xC3A4 /*'ä'*/: return 0xC384; // 'ä' -> 'ä'
        case 0xC3B6 /*'ö'*/: return 0xC396; // 'ö' -> 'ö'
        case 0xC3BC /*'ü'*/: return 0xC39C; // 'ü' -> 'ü'
    }
    if (character < KEYB_TB_UNIVERSALSHIFTER_LEN) {
        return pgm_read_word(&keyb_tb_universalShifter[character]);
    }
    return 0;
}

/*!
 *  A function to modify the value of the numpad.
 *  The default value from the numpad keys will be the value you would expect if the NUM mode is _ON_.
 *  After applying this function the numpad key will have the meaning as if the NUM mode were _OFF_.
 *
 *  \param keyBuffer  Pointer to the key that should be modified. Make sure type and key are set correctly.
 */
void keyb_unNumChar(Key *keyBuffer) {
    // The translation may only be initiated for proper values that come from the numpad
    if (keyBuffer->key < KEYB_TB_UNIVERSALNUMPADINVERTER_LEN && keyBuffer->type == KEYB_KT_NUMPRINTABLE) {
        keyBuffer->type = pgm_read_byte(&keyb_tb_universalNumpadInverter_types[keyBuffer->key]) - '0';
        keyBuffer->key = pgm_read_byte(&keyb_tb_universalNumpadInverter[keyBuffer->key]);
    }
}
