/* 
 * File:   can.h
 * Author: pojd
 *
 * Created on October 31, 2015, 10:19 PM
 */

#ifndef CAN_H
#define	CAN_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "utils.h"

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz

void switchCanMode(int mode, boolean wait) {
    // now switch CAN BUS to configuration mode
    CANCONbits.REQOP = mode;
    // if wait was required, then wait until we are in configuration mode (it may take a few cycles according to datasheet)
    while (wait && CANSTATbits.OPMODE != mode);    
}

void setupBaudRate() {
    // SJW to be 1
    // will use 1 TQ for SYNC, 4 for PROP SEG, 8 for phase 1 and 3 for phase 2 = 16TQ (recommendation in datasheeet to place sample point at 80% of bit time)
    // TBIT = 1000 / BAUD_RATE = 16TQ => TQ = 1000 / (BAUD_RATE)
    // TQ (micros) = (2 * (BRP + 1))/CPU_SPEED (MHz) => BRP = (1000*CPU_SPEED)/(32*BAUD_RATE) -1, BRP = 9 = 0b1001 (for our speed and setting)

    // BRGCON1 = first 2 bits as 0 = SJW 1, last 6 bits are BRP, so mask first two bits of BRP (to make SJW 00)
    int BRP = (int)(1000*CPU_SPEED)/(int)(32*BAUD_RATE) -1;
    BRGCON1 = BRP & 0b00111111;
    
    // phase 2 segment programmable (1), sampled once (0), 3 bits for phase seg 1 (8=111), 3 bits for propagation time (4=011)
    BRGCON2 = 0b10111011;
    // enable can bus wake up (0), line filter not applied to this (0), 3 unused bits (0), phase segment 2 (3=010)
    BRGCON3 = 0b00000010;
}

/**
 * Configures CAN according to the data sheet, see below:
 * 
 * Initial LAT and TRIS bits for RX and TX CAN.
 * Ensure that the ECAN module is in Configuration mode.
 * Select ECAN Operational mode.
 * Set up the Baud Rate registers.
 * Set up the Filter and Mask registers.
 * Set the ECAN module to normal mode or any
 * other mode required by the application logic.
 * 
 * In normal mode, the CAN module automatically over-
 * rides the appropriate TRIS bit for CANTX. The user
 * must ensure that the appropriate TRIS bit for CANRX
 * is set.
 */
void configureCan() {
    // TRIS3 = CAN BUS RX = has to be set as INPUT, all others as outputs (we assume we are the first to setup whole TRISB here)
    TRISB = 0b00001000;
    // now switch CAN BUS to configuration mode
    switchCanMode(0b100, TRUE);

    // no need to configure ECAN registers - mode legacy by default
    // CIOCON is probably OK too (CAN IO register)
    setupBaudRate();
    // TODO Set up the Filter and Mask registers.

    // now switch CAN BUS required mode (loopback for now, will be normal - 000 in the future)
    // do not wait for the mode to actually switch here (loopback does not seem to honor that)
    switchCanMode(0b010, FALSE);
}

/**
 * Attempts to send the message using TXB0 register (not using any others right now)
 * 
 * @param canID: the can ID (standard, ie 11 bits) of the message to be sent
 * @param data: data to be sent over CAN (takes all 8 bytes)
 */
void sendCanMessage(int canID, unsigned char* data) {
    // first check the register is not in error (previous send failed) - if so, then clear the flag and start sending this message
    // the error sent counter will be increased, so we can read this information later anyway
    if (TXB0CONbits.TXERR) {
        TXB0CONbits.TXERR = 0;
        TXB0CONbits.TXREQ = 0; // this will make the register RW again (and will pass the below loop as a result)
    }
    // confirm nothing is in the transmit register yet (previous can message sent)    
    while (TXB0CONbits.TXREQ);
    // we have an int, which should be 16bits, so take bits 5-12 to set TXB0SIDH (first 8 bits of standard ID) - then move it by 2 to get the proper 8 bit number
    TXB0SIDH = (0b11111111000 & canID) >> 3;
    // and now take the last 2 bits to the TXB0SIDL register (remainder of the ID)
    TXB0SIDL = (0b111 & canID) << 5;

    // now find out how much data to send - should be 8 bytes top
    int dataSize = data ? sizeof(data) : 0; 
    if (dataSize > 8) {
        dataSize = 8; // otherwise simply skip further data and only take first 8 bytes
        
    }
    // TXB0DLC - last 4 bits should contain data length - so take those from data size
    TXB0DLC = dataSize & 0b1111;
    // now populate TXB0Dm data registers - m in 0..7
    TXB0D0 = data;
    // TODO introduce custom structure to contain CAN message - both can ID and data - and add some methods to be able to extend the data somehow...
    
    // request transmitting the message (setting the special bit)
    TXB0CONbits.TXREQ = 1; 
}


#ifdef	__cplusplus
}
#endif

#endif	/* CAN_H */

