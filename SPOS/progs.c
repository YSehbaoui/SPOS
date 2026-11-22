#include "keyb_processor.h"
#include "lcd.h"
#include "os_scheduler.h"
#include "os_memory.h"
#include "os_core.h"
#include "time.h"

#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <avr/io.h>

// Defines
#define MAX_WORD_LENGTH 10
#define WORD_LIST_LEN 25
#define GAME_DURATION 30
#define STD_WAIT_MS 1000

// Variables
MemAddr typed_word_address;				// Address of shared Memory for the typed word
uint8_t current_offset;					// Index where we are in the word currently
volatile uint8_t current_phase = 0;				// Variable to distingush between intro sections
volatile uint8_t seconds = 0;			// Time of the game played
uint8_t score = 0;						// Score (number of words typed correctly)
const char* word_list[WORD_LIST_LEN];	// List of Words that can be asked from

/*!
 * Init the seconds counter
  * @params percentage: A percentage of a second (0xFF is an entire second, 0x7F is half a second and so on)
*/
void timer1_init(uint8_t percentage) {
	// Setze Timer1 im CTC Modus
	TCCR1B |= (1 << WGM12);

	// Setze Compare-Match-Wert für 1 Sekunde
	// F_CPU ist die CPU-Frequenz, hier 20 MHz
	// Mit Prescaler 1024: 20 MHz / 1024 = 19531.25 Ticks pro Sekunde
	// Wir brauchen 19531.25 Ticks, um 1 Sekunde zu erreichen
	OCR1A = 19530 * ((percentage+(percentage == 0))/0xff);

	// Setze den Prescaler auf 1024
	TCCR1B |= (1 << CS12) | (1 << CS10);

	// Aktiviere Timer1 Compare Match A Interrupt
	TIMSK1 |= (1 << OCIE1A);

	// Setze den Timer-Zähler auf 0
	TCNT1 = 0;

	// Aktivieren Sie globale Interrupts
	sei();
}

/*!
 * ISR for the second counter
 * Increments global `seconds` by one, every second
*/
ISR(TIMER1_COMPA_vect) {
	seconds++;
}

/*!
 * Init for the game
  * Allocs Shared Memory, inits the timer to one second, fills the word list
*/
void init_speed_typer() {
	// Try Malloc Sh Memory
	typed_word_address = os_sh_malloc(intHeap, MAX_WORD_LENGTH+1);
	if (!typed_word_address) {
		os_error("initST: could not shmalloc");
	}
	timer1_init(0xff);
	//                            ;
	word_list[0]  = "Praktikum\0";
	word_list[1]  = "System\0";
	word_list[2]  = "Programm\0";
	word_list[3]  = "Team7\0";
	word_list[4]  = "RWTHAachen\0";
	word_list[5]  = "Nobis\0";
	word_list[6]  = "Bestanden\0";
	word_list[7]  = "AtMega\0";
	word_list[8]  = "Bitshift\0";
	word_list[9]  = "Polling\0";
	word_list[10] = "Scheduler\0";
	word_list[11] = "Leerlauf\0";
	word_list[12] = "Git\0";
	word_list[13] = "Symbiose\0";
	word_list[14] = "Tutor\0";
	word_list[15] = "HIWI\0";
	word_list[16] = "Krakatoa\0";
	word_list[17] = "Deadlock\0";
	word_list[18] = "Yield\0";
	word_list[19] = "SPOS\0";
	word_list[20] = "Git Gud\0";
	word_list[21] = "Pepe\0";
	word_list[22] = "Pikachu\0";
	word_list[23] = "VM Pool\0";
	word_list[24] = "Numlock\0";

}

/*!
 * KeyHandler for all Right Controll inputs
*/
void RightControlHandler(Key keyBuffer) {
	uint8_t deletion = 0;

	// If Backspace is pressed, remove one character from the sh memory
	switch (keyBuffer.key) {
		case KEYB_KEY_CTRL_BACK:
			if (current_offset > 0 && keyBuffer.meta == KEYB_KM_MAKE) {
				os_sh_write(intHeap, &typed_word_address, --current_offset, &deletion, 1);
			}
		break;
		case KEYB_KEY_CTRL_ENTER:
			if (keyBuffer.meta == KEYB_KM_MAKE) {
				current_phase = 1;
			}
		break;
	} 
}

/*!
 * KeyHandler for all inputs
*/
void AllKeyHandler(Key keyBuffer) {
	// We don't care about key lifts
	if (keyBuffer.meta == KEYB_KM_BREAK) {
		return;
	}
	
	// Safe the currently pressed key as MemValue (cast from uint16 to uint8 ...)
	MemValue pressed_key = keyBuffer.key;

	switch (keyBuffer.type) {
		case KEYB_KT_PRINTABLE: // if the key is printable
		if (current_offset < MAX_WORD_LENGTH) { // and we did not fill the entire memory space yet
			os_sh_write(intHeap, &typed_word_address, current_offset++, &pressed_key, 1); // write the key value to the sh memory
		}
		break;
		default: break;
	}
}

/*!
 * Displaying intro-sequence on LCD
*/
void intro_sequence() {
	// Introduction
	lcd_clear();
	lcd_writeProgString(PSTR("SPEED TYPER"));
	lcd_line2();
	lcd_writeProgString(PSTR("Press Enter"));
	while(!current_phase){}
	_delay_ms(200);
	current_phase = 0; // Reset Phase barrier
	
	// Rules
	lcd_clear();
	lcd_line1();
	lcd_writeProgString(PSTR("Type all words!"));
	lcd_line2();
	lcd_writeProgString(PSTR("Press Enter"));
	while(!current_phase){}

	// Countdown
	lcd_clear();
	lcd_line1();
	lcd_writeProgString(PSTR("Ready?"));
	lcd_line2();
	uint8_t countdown = 3;
	_delay_ms(STD_WAIT_MS);
	do {
		lcd_erase(2);
		lcd_goto(2,1);
		lcd_writeDec(countdown);
		for(int i = 0; i<15; i++){
			_delay_ms((uint8_t) (STD_WAIT_MS/16));
			lcd_writeChar('.'); // Little Animation
		}
	} while (--countdown);
	
	lcd_clear();
	lcd_line1();
}

//! Automatically Start the Game as a process
REGISTER_AUTOSTART(speedTyper)

/*!
 * The Game itself
*/
void speedTyper(void) {
	// Start the keyboard
	keyb_start();

	// Set Processors
	keyb_set_processor(KEYB_KT_RCONTROL, RightControlHandler);
	keyb_set_main_processor(AllKeyHandler);
	
	// every 255th second increment on globals "seconds" to create a random seed
	timer1_init(0x01);

	// Play intro
	intro_sequence();

	// Enable display and cursor
	lcd_command(0b1110);
	
	// Set the Random Seed
	srand(seconds);
	
	// Start game time
	init_speed_typer();

	// Init current string
	char current_string[MAX_WORD_LENGTH] = {0};

	// Reset the timer
	seconds = 0;
	
	// While the game is played...
	while (seconds < GAME_DURATION) {
		// pick a word
		volatile uint8_t index = rand() % WORD_LIST_LEN;
		const char* selected_word = word_list[index];
		
		// Reset Memory and Current String
		current_offset = 0;
		MemValue deletion = 0;
		for (uint8_t i = 0; i < MAX_WORD_LENGTH; i++) {
			os_sh_write(intHeap, &typed_word_address, i, &deletion, 1);
			current_string[i] = 0; // wont get displayed on LCD
		}

		do {
			// Lese den eingegebenen Text
			for (int i = 0; i < MAX_WORD_LENGTH; i++) {
				current_string[i] = intHeap->driver->read(typed_word_address + i);
			}

			// Display Selected Word
			lcd_clear();
			lcd_line1();
			lcd_writeString(selected_word);
			
			// Display Seconds next to it (t:xx) (just for Game time <100)
			lcd_goto(1, 16-4);
			lcd_writeString("t:");
			if(seconds < 10){
				lcd_writeChar(' ');
			}
			lcd_writeDec(seconds);
			
			// Display Current Word
			lcd_line2();
			lcd_writeString(current_string);
			
			// Display Score next to it
			lcd_goto(2, 16-4);
			lcd_writeString("s:");
			if(seconds < 10){
				lcd_writeChar(' ');
			}
			lcd_writeDec(score);
			
			lcd_goto(2, current_offset+1); // for the cursor
			
			// Delay against flicker
			_delay_ms(50);
			
		// repeat until the strings match or the time runs out
		} while (strcmp(selected_word, current_string) != 0 && seconds < GAME_DURATION);

		// increment score if the strings match (not nessessarily the case if the timer runs out)
		if(strcmp(selected_word, current_string) == 0) score++;

	}
	// Display final Score
	lcd_command(LCD_DISPLAY_ON | LCD_HIDE_CURSOR); // Hide Cursor
	lcd_clear();
	lcd_line1();
	lcd_writeString("~~~GAME~OVER~~~");
	lcd_line2();
	lcd_writeString("Your Score: ");
	lcd_writeDec(score);
	while (1);

	HALT;
}

