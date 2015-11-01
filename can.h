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

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz

void switchCanMode(int mode) {
    // now switch CAN BUS to configuration mode
    CANCONbits.REQOP = mode;
    // wait until we are in configuration mode (it may take a few cycles according to datasheet)
    while (CANSTATbits.OPMODE != mode);    
}

void setupBaudRate() {
    // SJW to be 1
    // will use 1 TQ for SYNC, 4 for PROP SEG, 8 for phase 1 and 3 for phase 2 = 16TQ (recommendation in datasheeet to place sample point at 80% of bit time)
    // TBIT = 1000 / BAUD_RATE = 16TQ => TQ = 1000 / (BAUD_RATE)
    // TQ (micros) = (2 * (BRP + 1))/CPU_SPEED (MHz) => BRP = (1000*CPU_SPEED)/(32*BAUD_RATE) -1, BRP = 9 = 0b1001 (for our speed and setting)

    // BRGCON1 = first 2 bits as 0 = SJW 1, last 6 bits are BRP, but BRP is small enough as it is, no need to adjust by bit shifting
    // maybe just to make sure, we null out the first 2 bits anyway to make sure we use 0 SJW
    int BRP = (long)(1000*CPU_SPEED)/(long)(32*BAUD_RATE) -1;
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
    switchCanMode(0b100);

    // no need to configure ECAN registers - mode legacy by default
    // CIOCON is probably OK too (CAN IO register)
    
    setupBaudRate();
    // TODO Set up the Filter and Mask registers.

    // now switch CAN BUS required mode (loopback for now, will be normal - 000 in the future)
    switchCanMode(0b010);
}


#ifdef	__cplusplus
}
#endif

#endif	/* CAN_H */

