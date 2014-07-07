#include "TWI.h"
#include <avr/interrupt.h>

#define DEBUG 0

/// Number of sensors read so far. Increased by a new data packet.
uint8_t sensorRead = 0;
/// Status bit to check for second start (get the data)
uint8_t status = 0;
/// Counter for the log buffer
uint8_t debugCounter = 0;
/// Counter for counting delays
uint8_t delayCounter = 0;

/**
 * twiPoll
 * 
 * Function to read the sensor data from the sensor module via TWI.
 * This functions should be executed as freqently as possible. It checks for 
 * new messages in the TWI message buffer. If there are new messages, it tries to 
 * answer them. The control flow is as follows:
 * 
 * If there are new messages (TWCR & (1 << TWINT)) check for the type of message (TWSR & 0xF8).
 * 1. The main function sends the START signal
 * 2. TW_START: The start signal is acknowledged, so we send the adress to talk to (SLA_W)
 * 3. TW_MT_SLA_ACK: The address answered, so we send the offset (0x02) where we later want to read from
 * 4. TW_MT_DATA_ACK: The offset was acknowledged. So we want to start reading. We want to send the reading address (SLA_R), so we restart (status = 1)
 * 5. TW_REP_START: After restarting, we send the reading address (SLA_R).
 * 6. TW_MR_SLA_ACK: The reading address was acknowledged. We can start reading.
 * 7. TW_MR_DATA_ACK: We read until we have collected 6 data packets.
 * 8. TW_MR_DATA_NACK: We start again.
 */
void twiPoll() {

	delayCounter++;
	messageData[4] = delayCounter;
	// Nur etwas tun, falls auch etwas erhalten wurde.
	if(TWCR & (1 << TWINT)) {
		switch (TWSR & 0xF8) {
		// Start-Befehl angenommen. Nun senden wir die Write-Adresse SLA_W oder
		// Das 2. Start wurde angenommen. Nun können wir die Read-Adresse SLA_R senden
		case TW_REP_START:
		case TW_START:
			if(status == 0) {
				messageData[0] = 1;
				messageData[2] = 0;
				messageData[3] = 0;
				TWDR = SLA_W;
				TWCR = (1 << TWINT) | (1 << TWEN);
			}
			else if(status == 1) {
				TWDR = SLA_R;
				// hier kommt er noch an
				messageData[0] = 4;
				delayCounter = 0;
				messageData[6] = messageData[6] + 1;
				TWCR = (1 << TWINT) | (1 << TWEN);
			}
			break;

		// Write-Adresse angenommen. Nun senden wir den Offset 0x02
		case TW_MT_SLA_ACK:
			messageData[0] = 2;
			TWDR = 0x02;
			TWCR = (1 << TWINT) | (1 << TWEN);
			break;

		// Die Write-Adresse wurde nicht angenommen. Wir versuchen das Senden noch einmal
		// nur diesmal etwas schneller
		case TW_MT_SLA_NACK:
			TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN) | (1 << TWSTO);
			while(!(TWCR & (1 << TWINT))) ;
			TWDR = SLA_W;
			messageData[2] = 1;
			TWCR = (1 << TWINT) | (1 << TWEN);
			break;

		// Das Offset wurde angenommen. Nun können wir die Daten lesen. Also senden wir Start:
		case TW_MT_DATA_ACK:
			status = 1;
			messageData[0] = 3;
			TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
			break;

		// Die Read-Adresse wurde angenommen. Nun können wir Daten abholen:
		case TW_MR_SLA_ACK:
			// hier nicht??
			messageData[0] = 5;
			sensorRead = 0;
			TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
			break;

		// Die Read-Adresse wurde nicht angenommen. Wir senden sie noch einmal,
		// nur diesmal schneller.
		case TW_MR_SLA_NACK:
			// hier auch nicht??
			TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
			while(!(TWCR & (1 << TWINT))) ;
			TWDR = SLA_R;
			messageData[3] = 1;
			TWCR = (1 << TWINT) | (1 << TWEN);
			break;

		// Neue Daten sind angekommen. Wir rufen sie ab und fordern evtl neue an.
		case TW_MR_DATA_ACK:
			if(sensorRead < 7) {
				sensorData[sensorRead++] = TWDR;
				TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
			}
			else {
				messageData[0] = 6;
				TWCR = (1 << TWINT) | (1 << TWEN);
			}
			break;

		// Das letzte Datenpaket ist angekommen, wir
		// fangen neu an!
		case TW_MR_DATA_NACK:
			messageData[0] = 7;
			messageData[1] = messageData[1] + 1;
			sensorRead = 0;
			status = 0;
			TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA) | (1 << TWSTO);
			break;


		// Scheinbar ist etwas falsch gelaufen. Wir brechen ab.
		default:

			sensorRead = 0;
			status = 0;

			messageData[4] = 1;
			messageData[5] = TWSR & 0xF8;
			// Start/Stop senden
			TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWSTA) | (1 << TWSTO);
			break;
		}
	}
}
