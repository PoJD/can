
/* 
 * Main CAN interface source file. 
 * Implemented from PIC18F2XKXX datasheet, so could work for other chips too (assuming xc.h is included in your main file prior to this one)
 * 
 * File:   can.c
 * Author: pojd
 *
 * Created on October 31, 2015, 10:19 PM
 */

#include <xc.h>
#include "utils.h"
#include "can.h"

void can_setMode(Mode mode, boolean waitForSwitch) {
    CANCONbits.REQOP = mode;
    // if wait was required, then wait until we are in configuration mode (it may take a few cycles according to datasheet)
    while (waitForSwitch && CANSTATbits.OPMODE != mode);    
}

void can_setupBaudRate(int baudRate, int cpuSpeed) {
    // SJW to be 1
    // will use 1 TQ for SYNC, 4 for PROP SEG, 8 for phase 1 and 3 for phase 2 = 16TQ (recommendation in datasheeet to place sample point at 80% of bit time)
    // TBIT = 1000 / BAUD_RATE = 16TQ => TQ = 1000 / (BAUD_RATE)
    // TQ (micros) = (2 * (BRP + 1))/CPU_SPEED (MHz) => BRP = (1000*CPU_SPEED)/(32*BAUD_RATE) -1, BRP = 9 = 0b1001 (for our speed and setting)

    // BRGCON1 = first 2 bits as 0 = SJW 1, last 6 bits are BRP, so mask first two bits of BRP (to make SJW 00)
    int BRP = (int)(1000*cpuSpeed)/(int)(32*baudRate) -1;
    BRGCON1 = BRP & 0b00111111;
    
    // phase 2 segment programmable (1), sampled once (0), 3 bits for phase seg 1 (8=111), 3 bits for propagation time (4=011)
    BRGCON2 = 0b10111011;
    // enable can bus wake up (0), line filter not applied to this (0), 3 unused bits (0), phase segment 2 (3=010)
    BRGCON3 = 0b00000010;
}

/*
 * Public API goes here
 */

void can_init(int baudRate, int cpuSpeed, Mode mode) {
    can_setMode(CONFIG_MODE, TRUE);

    // no need to configure ECAN registers - mode legacy by default
    // CIOCON is probably OK too (CAN IO register)
    can_setupBaudRate(baudRate, cpuSpeed);
    // TODO Set up the Filter and Mask registers (not needed for now)

    // now switch CAN BUS required mode
    // do not wait for the mode to actually switch here (only config mode is probably required to follow that)
    can_setMode(mode, FALSE);
}

void can_send(CanMessage *canMessage) {
    // first check the register is not in error (previous send failed) - if so, then clear the flag and start sending this message
    // the error sent counter will be increased, so we can read this information later anyway
    if (TXB0CONbits.TXERR) {
        TXB0CONbits.TXERR = 0;
        TXB0CONbits.TXREQ = 0; // this will make the register RW again (and will pass the below loop as a result)
    }
    // confirm nothing is in the transmit register yet (previous can message sent)    
    while (TXB0CONbits.TXREQ);
    // set TXB0SIDH (first 8 bits of standard ID) by simply using the message ID itself
    TXB0SIDH = canMessage->messageID;
    // and now take the message type as 3 bits and set first 3 high bits in the TXB0SIDL register (remainder of the canID)
    TXB0SIDL = canMessage->messageType << 5;

    // TXB0DLC - last 4 bits should contain data length - equal to 2 now (1 byte is enough for the 1 bit only so far + 1 byte more for error count)
    TXB0DLC = 2;

    // now populate TXB0DX data registers
    TXB0D0 = canMessage->isSwitchOn << 7; // 1st bit of the 1st byte only
    TXB0D1 = TXERRCNT; // whole 2nd byte = CAN error count read from the register
    
    // request transmitting the message (setting the special bit)
    TXB0CONbits.TXREQ = 1; 
}
