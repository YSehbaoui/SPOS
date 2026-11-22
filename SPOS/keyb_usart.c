#include "keyb_usart.h"

#include "keyb_processor.h"
#include "util.h"
#include "os_core.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*
 *  The default UART configuration is set to receive and not to transmit.
 *  In order to transmit a command to the keyboard, the receiver must be deactivated
 *  to prevent it from receiving the byte just transmitted to the keyboard.
 *  This workaround is required since the keyboard has just one data port for both
 *  receiving and transmitting data.
 *  The clock however is used to actually conduct polarity changes on the data port.
 */

// --------- private -------------------------------

bool keyb_transmit(uint8_t command);

volatile uint16_t poll_loop_watchdog;
uint8_t poll_failures;

#define POLL_EVENT(CONDITION)                                  \
    do {                                                       \
        poll_loop_watchdog = 60000;                            \
        while (!(CONDITION) && --poll_loop_watchdog) continue; \
        poll_failures += !poll_loop_watchdog;                  \
    } while (0)

#define POLL_EVENT_PANIC(CONDITION, PANIC) \
    {                                      \
        poll_failures = 0;                 \
        POLL_EVENT(CONDITION);             \
        if (poll_failures) {               \
            PANIC;                         \
        }                                  \
    }

/*!
 *  The interrupt service routine that handles incoming bytes from the keyboard.
 *  Received bytes are not parsed in any way and directly passed
 *  to keyb_input_process for processing in layer 3.
 */
ISR(USART0_RX_vect) {
	
	// ISR ruft input process auf nach dokument:
	keyb_input_process(UDR0);
	
}

/*!
 *  \private
 *  Configures the built in USART module to receive.\r\n
 *    The actual configuration of the interface properties is not handled here.
 */
void keyb_activateReceiver(void) {

	//set des 4-ten bits in UCSR0B, was den receiver darstellt:
	sbi(UCSR0B, 4);

}

/*!
 *  \private
 *  Configures the built in USART module to deactivate the receiver.\r\n
 *    The actual configuration of the interface properties is not handled here.
 */
void keyb_deactivateReceiver(void){
	
	//clear des 4-ten bits in UCSR0B, was den receiver darstellt:
	cbi(UCSR0B, 4);

}

/*!
 *  Uses polling to receive a byte with the USART interface.
 *	\return The byte sent by the keyboard.
 */
uint8_t keyb_receive(){
	
	// receive mit timeout pollen:
	// weil ihr das auch so geschrieben habt....
	POLL_EVENT_PANIC(gbi(UCSR0A, RXC0), return 0);
	
	// in variable zwischenspeichern um das ganze atomar machen zu k?nnen wenn n?tig...
//	uint8_t buffer_read = UDR0;
	
	// Daten vom Puffer auslesen und zur?ckgeben wenn alles geklappt hat:
	return UDR0;
	
}

uint8_t keyb_getScanCode(){
	
	// Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
		
	// Deaktivieren des GIEB
	SREG &= 0x7F;
		
	//scancode getten, dazu:
	uint8_t retVal = 0x00;
	uint8_t ack = 0;

		
	//gro?e Schleife in der wir wieder und wieder probieren den Scancode zu setzen
			
	// 1. F0 senden:
	if (!keyb_transmit(KEYB_COMMAND_SCANCODESET)){
//		os_error("gScan: 1 Transmit fail");
	}
			
	// 2. FA abwarten:
	ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
			
			
	// wenn 1 und 2 erfolgreich waren:
	if(ack){

		// 3. 03 senden:
		if (!keyb_transmit(0x00)){
//			os_error("gScan: 3 Get fail");
		}
				
		// 4. FA abwarten:
		ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
			
		// 5. Code abwarten:
		if(ack){
			retVal = keyb_receive();
		}

	}
		
	// Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);

	return (1 <= retVal && retVal <= 3) ? retVal : 0;
	
}

/*!
 *  Tries to update the keyboard's scan code set to the given value.
 *  This function is not plug and play safe! If the keyboard
 *  is disconnected or broken, the OS may freeze.
 *  After setting the new scan code set the actual scan code set will be
 *  returned (e.g. for confirmation).
 *
 *  \param scanCodeSet  A value in [0..3]. If 0, the scan code set will
 *	 				    not be changed. Otherwise the keyboard's
 *						scan code set will be changed to this value.
 *	\return The actual scan code set of the keyboard.
 */
uint8_t keyb_setScanCodeSet(uint8_t scanCodeSet) {
	// Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// Deaktivieren des GIEB
	SREG &= 0x7F;
	
	//scancode setzen, dazu:
	uint8_t ack = 0;
	
	//gro?e Schleife in der wir wieder und wieder probieren den Scancode zu setzen
	while(!ack){
		
		// 1. F0 senden:
		if (!keyb_transmit(KEYB_COMMAND_SCANCODESET)){
			//os_error("sScan: 1 Transmit fail");
		}
		
		// 2. FA abwarten:
		ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
		
		// wenn 1 und 2 erfolgreich waren:
		if(ack){

			// 3. 03 senden:
			if (!keyb_transmit(scanCodeSet)){
				//os_error("sScan: 3 Transmit fail");
			}
			
			// 4. FA abwarten:
			ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
			
		}
	}

	// Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
	
	return scanCodeSet;

	
}

/*!
 *  Initializes the processor to communicate with the keyboard.
 *  Correctly sets pin directions and values to power the keyboard.
 *  Initializes the internal USART to receive data from the keyboard.
 *  Notice that this function is not plug and play safe.
 *  If the keyboard is disconnected or broken, SPOS may freeze.
 */
void keyb_usart_init(void) {
	
	//m?glicherweise critical section notwendig, noch unsicher:
	// Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// Deaktivieren des GIEB
	SREG &= 0x7F;
	
	//setzt power auf output:
	KEYB_POWER_DDR |= 0xFE;
	//setze power port auf: erste 4 high, dann 3 low, clock (letztes bit) bleibt erstmal wies ist:
	KEYB_POWER_PORT |= 0xF0;
	KEYB_POWER_PORT &= 0xF1;
	
	//als n?chstes clock:
	//auf output:
	KEYB_CLOCK_DDR |= 0x01;
	//dann zun?chst de clock pin im port register auf low:
	KEYB_CLOCK_PORT &= ~(1<<KEYB_CLOCK_PIN);
	
	//als letztes Data:
	//zuerst auch auf output
	KEYB_DATA_DDR |= 0x01;
	//und output high einstellen
	KEYB_DATA_PORT |= 0x01;
	
	
	// USART control registers:
	// BITS: lsb to msb
	// 	0: 0 rising or falling edge, here transmit on rising edge, read on falling edge - 1 would be other way around
	// 	1: 1 UCSZN 0
	// 	2: 1 UCSZN 1 sind 2 von 3 bits zur konfiguration der character size, das letzte bit ist in register B, diese 2 sind 1, das in register b sollte 0 sein, f?r 8 bit size
	// 	3: 0 1 stop bit
	// 	4: 1 parity bits, mn0 und mn1 f?r parity
	// 	5: 1 4 setzt parity enable, 5 setzt parity auf odd
	// 	6: 1 umsel 0
	// 	7: 0 umsel 1 f?r synchronen modus, kann aber auch anders sein
	// --------------------------- FEHLERQUELLE --------------------------------------------------
	
	UCSR0C = 0b01110110;
	
	// receiver active bit auf 1 setzen:
	UCSR0B |= 0b00010000;
	
	// UCSZN 3 auf 0 setzen: (letztes character size bit)
	UCSR0B &= 0b11111011;
	
	// warten, sodass die tastatur die initielle Data = 1 und Clock = 0 verarbeitet:
	_delay_us(100);
	
	// Data nochmal auf 1 output spezifizieren
	KEYB_DATA_PORT |= 0x01;
	
	// hier auch clock high setzen, um der tastatur mitzuteilen, dass wir jetzt empfangen:
	KEYB_CLOCK_PORT |= 0x01;
	
	// dann warten, sodass die tastatur das auch mitbekommt:
	_delay_us(150);
	
	//Keyboard port als input konfigurieren:
	KEYB_DATA_DDR &= 0xFE;
	//pullup widerstand aktivieren:
	KEYB_DATA_PORT |= 0x01;
	
	// Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);
	
}

/*!
 *  Start the communication with the keyboard.
 *  This first calls keyb_usart_init to setup the low level communication
 *  interface with the keyboard. It then configures the scan code set to use
 *  and finally initializes layer 3, the input processor layer.
 *  At the end, enables the USART interrupt to start passively receiving input
 *  from the keyboard.
 */
void keyb_start(void) {

	// keyboard init aufrufen und 10ms warten:
	keyb_usart_init();
	_delay_ms(10);

	// setze Scancode auf Code 03:
	keyb_setScanCodeSet(0x03);

	// Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// Deaktivieren des GIEB
	SREG &= 0x7F;

	uint8_t ack = 0;

	while(!ack){
	
		// dann noch ein FA schicken um Make/Break codes zu resetten und Tastatur sinnvoll zu resetten nach ?ndern des Scancodes:
		keyb_transmit(0xFA);
		
		// und nochmal ack daf?r, dass das auch so passiert ist:
		ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);

	}

	// Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);

	keyb_init_input_processor();
	
	// Interrupt des USART einschalten:
	// dazu muss in UCSRNB das MSB auf 1 gesetzt werden:
	// wenn es aktiv ist, sowie global aktiv ist und das empfangs-register voll ist wird der interrupt gefeuert:
	sbi(UCSR0B, 7);
	
	
	
}

/*!
 *  This _private_ function is used by the keyb_transmit routine to flush
 *  a single bit and wait for the next clock signal.
 *  NEVER CALL THIS FUNCTION DIRECTLY!
 *
 *  \param bit  The bit to send to the keyboard.
 *  \return     The bit that was sent.
 */
bool keyb_send_andWaitForFallingEdge(bool bit) {
    if (bit) {
        sbi(KEYB_DATA_PORT, KEYB_DATA_BIT);
    } else {
        cbi(KEYB_DATA_PORT, KEYB_DATA_BIT);
    }

    POLL_EVENT(gbi(KEYB_CLOCK_PIN, KEYB_CLOCK_BIT));
    POLL_EVENT(!gbi(KEYB_CLOCK_PIN, KEYB_CLOCK_BIT));

    return bit;
}

/*!
 *  Transmit an unverified 8 bit command, encapsulated in a data format and
 *  wait for an acknowledge from the keyboard. This function will deactivate and
 *  reactivate the receiver on itself.
 *
 *  \param command  The command to send to the keyboard.
 *  \return 		True if the byte was correctly transmitted.
 */
bool keyb_transmit(uint8_t command) {
    // Deactivate all interrupts
    uint8_t ie = SREG & (1 << SREG_I);
    cli();

    // Deactivate the receiver to avoid any sent data being mistakenly interpreted
    // as received data
    keyb_deactivateReceiver();

    do {
        // To tell the keyboard that we are going to send a command, we have to switch
        // both the clock and data pin to output
        sbi(KEYB_CLOCK_DDR, KEYB_CLOCK_BIT);
        sbi(KEYB_DATA_DDR, KEYB_DATA_BIT);

        // Hold the clock signal low for at least 100 us and then switch the date pin to low as well
        cbi(KEYB_CLOCK_PORT, KEYB_CLOCK_BIT);
        _delay_us(150);

        // Send start bit (start bit is low)
        cbi(KEYB_DATA_PORT, KEYB_DATA_BIT);

        // Set the clock signal back to 1, in order to activate the keyboard's clock generator
        sbi(KEYB_CLOCK_PORT, KEYB_CLOCK_BIT);

        // Set clock to input to synchronize to it
        cbi(KEYB_CLOCK_DDR, KEYB_CLOCK_BIT);
        sbi(KEYB_CLOCK_PORT, KEYB_CLOCK_BIT); // Pull-up

        // Send the 8 bits of the command to be sent.
        // Use the walk_bit to step through bits 0 to 7 of the command.
        // Note that the keyboard expects the data beginning with LSB.
        volatile uint8_t walk_bit = 0;

        // Initialize parity with 1 (odd parity)
        uint8_t parity = 1;

        // Wait for falling edge
        POLL_EVENT_PANIC(!gbi(KEYB_CLOCK_PIN, KEYB_CLOCK_BIT), break);

        // Send bits 0 to 7
        while (walk_bit < 8) {
            parity ^= keyb_send_andWaitForFallingEdge((command >> walk_bit++) & 1);
        }

        // Send the odd parity bit
        keyb_send_andWaitForFallingEdge(parity & 1);

        // Send the final stop bit
        sbi(KEYB_DATA_PORT, KEYB_DATA_BIT);

        // Set data to input to receive ACK bit and final rising + falling edge
        cbi(KEYB_DATA_DDR, KEYB_DATA_BIT);
        sbi(KEYB_DATA_PORT, KEYB_DATA_BIT);

        // Wait for acknowledge bit
        POLL_EVENT_PANIC(!gbi(KEYB_DATA_PIN, KEYB_DATA_BIT), break);
        POLL_EVENT_PANIC(!gbi(KEYB_CLOCK_PIN, KEYB_CLOCK_BIT), break);
        POLL_EVENT_PANIC(gbi(KEYB_DATA_PIN, KEYB_DATA_BIT) || gbi(KEYB_CLOCK_PIN, KEYB_CLOCK_BIT), break);
    } while (0);

    // We're done with the whole transmit mambo jumbo, so turn the receiver back on
    // the keyboard response (if any) can then be received with keyb_receive
    keyb_activateReceiver();

    // Restore previous state of global interrupt enable bit
    SREG |= ie;

    // The transmit was successful if no poll failures occurred
    return poll_failures == 0;
}

/*!
 *  This routine uses the keyb_transmit function to change the LED status of the
 *  keyboard. Notice that the parameter list is sorted according to the typical
 *  layout of a standard keyboard.
 *
 *  \param num  true <=> enable NUM LED
 *  \param caps true <=> enable CAPS LED
 *  \param roll true <=> enable ROLL LED
 */
void keyb_updateLEDs(bool num, bool caps, bool roll) {
	
	// Speichern des GIEB
	uint8_t globalInterruptEnableBit = (SREG >> 7);
	
	// Deaktivieren des GIEB
	SREG &= 0x7F;
	
	
	//LEDs setzen, dazu:
	uint8_t ack = 0;
//	uint8_t maxItCounter = 20;
	uint8_t led_byte =  (caps<<2) | (num<<1) | (roll<<0);
	
	
	//gro?e Schleife in der wir wieder und wieder probieren die LEDs zu setzen
	while(!ack){
			
		// 1. ED senden als command f?r LEDs:
		if (!keyb_transmit(KEYB_COMMAND_UPDATE_LEDS)){
//			os_error("uLED: Transmit fail");
		}
			
		// 2. FA abwarten:
		ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
			
			
		// wenn 1 und 2 erfolgreich waren:
		if(ack) {
			// 3. LEDs updaten:
			if(!keyb_transmit(led_byte)){
				//	os_error("uLED: transfer fail");
			}
			// 4. FA abwarten:
			ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
		
		}
		
// 		// und nochmal ack daf?r, dass das auch so passiert ist:
// 		ack = (keyb_receive() == KEYB_RESPONSE_ACKNOWLEDGE);
			
	}
		
	// wenn wir maxItCounter auf 0 runtergez?hlt haben, also 255 mal versucht haben den code zu setzen:
	//if(!maxItCounter) os_error("uLED: Timeout");
	
	
	// Wiederherstellen des GIEB
	SREG |= (globalInterruptEnableBit << 7);	
	
}