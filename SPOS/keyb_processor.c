#include "keyb_processor.h"

#include "defines.h"
#include "keyb_scancode.h"
#include "keyb_usart.h"
#include "lcd.h"
#include "util.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

Key global_key_status;
KeyHandler type_handlers[KEYB_KEY_TYPES];
KeyHandler main_handler;

// Defines:
#define LALT_BSHIFT 0
#define RALT_BSHIFT 1
#define CTRL_BSHIFT 2
#define LSHIFT_BSHIFT 3
#define RSHIFT_BSHIFT 4
#define ROLL_BSHIFT 5
#define NUM_BSHIFT 6
#define CAPS_BSHIFT 7


/*!
 *  Initialization of layer 3, the input processor.
 *	Initializes all layer 3 state, e.g. the status of the various keyboard LEDs
 *  and the currently active modifiers. This is called from keyb_start.
 */
void keyb_init_input_processor() {
	
	// struct initialisieren
	global_key_status.key = 0xF0;
	global_key_status.meta = 0;
	global_key_status.modifier = KEYB_KM_BREAK;
	global_key_status.type = 0;
	
	
	// und LEDS auf: num 1, caps 0, print 0 setzen
	keyb_updateLEDs(1, 0, 0);
	
	global_key_status.modifier |= (1<<6);
	
	// und KeyHandler funktionen initialisieren:
	main_handler = NULL;
	for(uint8_t i = 0; i < KEYB_KEY_TYPES; i++) type_handlers[i] = NULL;
	
}

/*!
 *  Set the main key processor.
 *	This handler will receive all completely processed keys regardless of type.
 *	\param charHandler	The handler that will receive all keys
 */
void keyb_set_main_processor(KeyHandler charHandler) {
	main_handler = charHandler;
}

/*!
 *  This function runs after keyb_char_modifiers.
 *  Calls the both the main handler and the char handlers for specific types.
 */
void keyb_char_process() {
	
	Key single_key_status = global_key_status;
	
	if(main_handler != NULL){
		main_handler(single_key_status);
	}
	
	if(single_key_status.type != KEYB_KT_UNKNOWN && type_handlers[single_key_status.type] != NULL){
		type_handlers[single_key_status.type](single_key_status);
	} else if (single_key_status.type == KEYB_KT_UNKNOWN){
		type_handlers[0](single_key_status);
	}
	
	//wenn calls gemacht wurden, können wir global unbreaken:
	global_key_status.meta = KEYB_KM_MAKE;
	
}


bool keyb_isModifier(uint16_t key){
	return	key == KEYB_KEY_CTRL_CTRL ||
	key == KEYB_KEY_CTRL_SHIFT ||
	key == KEYB_KEY_CTRL_CAPS ||
	key == KEYB_KEY_CTRL_ROLL ||
	key == KEYB_KEY_CTRL_NUM ||
	key == KEYB_KEY_CTRL_ALT;
}

/*!
 *  This function modifies keys detected in keyb_input_process.
 *  It keeps track of the currently active modifiers and
 *  translates keys further (e.g. if the SHIFT modifier is active,
 *  calls keyb_shiftChar).
 *  Also respects the current state of CAPS and NUM.
 */
void keyb_char_modifiers() {
	
	    // Update metadata if needed
	    if(keyb_isModifier(global_key_status.key)){
		    uint8_t shift=255;
		    switch(global_key_status.key){
			    case KEYB_KEY_CTRL_CTRL: shift = CTRL_BSHIFT; break;
			    case KEYB_KEY_CTRL_SHIFT: shift = LSHIFT_BSHIFT + (global_key_status.type == KEYB_KT_RCONTROL); break;
			    case KEYB_KEY_CTRL_CAPS: shift = CAPS_BSHIFT; break;
			    case KEYB_KEY_CTRL_ROLL: shift = ROLL_BSHIFT; break;
			    case KEYB_KEY_CTRL_NUM: shift = NUM_BSHIFT; break;
			    case KEYB_KEY_CTRL_ALT: shift = LALT_BSHIFT + (global_key_status.type == KEYB_KT_RCONTROL); break;
		    }
		    uint8_t is_temp = !(shift == CAPS_BSHIFT || shift == ROLL_BSHIFT || shift == NUM_BSHIFT);
		    
		    if(global_key_status.meta == KEYB_KM_MAKE) { // if press down..
			    if(is_temp) { // and if it's a temp key
				    global_key_status.modifier |= (1 << shift); // then activate modifier
				    } else { // else if it is a permanent key (CAPS, NUM, ROLL)...
				    global_key_status.modifier ^= (1 << shift); // then XOR with previous state
			    }
			    } else { // BREAK temp keys, weil nicht temp auf break nicht reagieren sollen, sind ja locked keys
			    if(is_temp) {
				    global_key_status.modifier &= ~(1 << shift);
			    }
		    }

	    }
	
	// modify the key according to the modifiers

    // make vars for all relevant cases
    uint8_t shift = (global_key_status.modifier>>LSHIFT_BSHIFT & 1) || (global_key_status.modifier>>RSHIFT_BSHIFT & 1);
    uint8_t ralt = (global_key_status.modifier>>RALT_BSHIFT & 1);
    uint8_t caps = (global_key_status.modifier>>CAPS_BSHIFT & 1);
    uint8_t scroll = (global_key_status.modifier>>ROLL_BSHIFT & 1);
    uint8_t ctrl = (global_key_status.modifier>>CTRL_BSHIFT & 1);
    uint8_t alt = (global_key_status.modifier>>LALT_BSHIFT & 1);
    uint8_t num = (global_key_status.modifier>>NUM_BSHIFT & 1);

    if(ralt){
	    ctrl = 1;
	    alt = 1;
    }
	
	// lustig... wenn num an ist, dann funktioniert das numpad als ob es aus wäre, wenn ctrl + shift gedrückt wird: wird aber canceled, weil ctrl + shift eigentlich müll mit tasten macht
// 	if(num){
// 		if(shift && ctrl) num = 0;
// 	}
	
	// Update LEDs
	keyb_updateLEDs(num, caps, scroll);
	
	// wenn wir modifier kombinationen gedrückt haben die so nicht funktionieren, machen wir gar nichts...
// 	if ((alt && shift) || (ctrl && shift)) {
// 		global_key_status.type = KEYB_KT_UNKNOWN;
// 		return;
// 	}
// 	

	// if key on numpad
	if(global_key_status.type == KEYB_KT_NUMPRINTABLE){
		// but numpad not on
		if(!num){
			// un-Num the key
			keyb_unNumChar(&global_key_status);
		}
		// if key on main keyboard
	} else if (global_key_status.type == KEYB_KT_PRINTABLE){
		// if shift XOR caps (exactly one of them) is pressed or ralt is pressed, shift key
		if(((shift ^ caps) & 1) || (ctrl && alt)) global_key_status.key = keyb_shiftChar(global_key_status.key);
		// shift key a second time, if ralt is pressed
		if(ctrl && alt) global_key_status.key = keyb_shiftChar(global_key_status.key);
	}

	keyb_char_process();

}



/*!
 *  This function takes unprocessed bytes from layer 2 and parses them into the layer 3 key buffer.
 *	Uses the translate functions from keyb_scancode.h to get key and key type for an input byte
 *  and updates the key buffer. Also sets the active modifiers.
 *  If the translations yielded a valid result, hands off to keyb_char_modifiers for further processing
 *  of the key.
 *
 * \param input	The unprocessed byte from layer 2.
 */
void keyb_input_process(uint8_t input) {
	

	//wenn wir nen breakcode bekommen swappen wir nur zu Break-State und das wars für diesen aufruf
	//der break-code ist nur für die NÄCHSTE Anweisung wichtig:
	if(input == 0xF0)
	{
		global_key_status.meta = KEYB_KM_BREAK;
		return;
		
	}
	

	uint8_t interpreted_key_type = keyb_translateInputToKeyType(input);
	uint16_t interpreted_key = keyb_translateInputToKey(input);
	
	// Wenn der interpreted_key 0 ist, dann returnen wir sofort, weil nix sinnvolles gelesen wurde
	if(!interpreted_key) return;
	
	if(interpreted_key_type != KEYB_KT_UNKNOWN)	{
		// Sonst schreib key-werte in struct:
		global_key_status.key = interpreted_key;
		global_key_status.type = interpreted_key_type;
		keyb_char_modifiers();
	}
	
	
}



/*!
 *  With this function a simple trigger mechanism can be established. It allows to specify a hander
 *    function for every character class independently.
 *  \param keyType The keyType denotes the class of the key the caller is to be configured for.
 *  \param charHandler  The handler for the layer 3 parsed character. You may set this to NULL to deactivate
 *                        the trigger for the specified key class.
 */
void keyb_set_processor(KeyType keyType, KeyHandler charHandler){
	
	// wenn der keyType sinnvoll ist, aktualisier den Handler
	if(keyType != KEYB_KT_UNKNOWN){
		type_handlers[keyType] = charHandler;
	}else{
		// wenn wir unknown sind, dann schreibs erstmal in 0, aber vielleicht mach auch einfach gar nix - wir callen handlers[0] einfach nicht in process...
		type_handlers[0] = charHandler;
	}
	
}