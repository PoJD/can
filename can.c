
#include <xc.h>
#include "utils.h"
#include "can.h"

/*
 * Internal methods here
 */

/**
 * Translate the can header to an integer (only low 11 bits should be set)
 * 
 * @param header can header to translate
 * @return canID (11 bit integer)
 */
int translateCanHeader(CanHeader *header) {
    // high byte is set to equal to message type - should be 3 bits only, so highest 5 bits will be 0 anyway
    // low byte = the node ID directly
    // high bits are set to message type on purpose to ease filtering - make sure the highest bits are filtered easily
    return (header->messageType << 8) + header->nodeID;
}

/*
 * Public API here
 */

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
    BRGCON1 = BRP & 0b111111;
    
    // phase 2 segment programmable (1), sampled once (0), 3 bits for phase seg 1 (8=111), 3 bits for propagation time (4=011)
    BRGCON2 = 0b10111011;
    // enable can bus wake up (0), line filter not applied to this (0), 3 unused bits (0), phase segment 2 (3=010)
    BRGCON3 = 0b00000010;
}

void can_setupStrictReceiveFilter(CanHeader *header) {
    // setup just 1 acceptance filter to only accept CAN message for the in passed header information
    int canID = translateCanHeader(header);
    RXF0SIDH = canID >> 3; // take highest 8 bits as a byte
    // take 3 bits only and set as high bits inside low byte register 
    // this also sets EXIDEN bit to 0 to only accept standard IDs (no extended ones)
    RXF0SIDL = (canID & 0b111) << 5;
    // now enable only this filter
    RXFCON0bits.RXF0EN = 1;
    
    // now setup the strict mask -- all high 11 bits are the canID, rest is EXIDEN bits and others unused in legacy mode
    RXM0SIDH = 0b11111111;
    // first 3 bits finish the canID, the 5th bit sset EXIDEN to 1 - use the same value of EXIDEN as in the corresponding filter
    RXM0SIDL = 0b11101000;
    
    // ignore buffer overflows here - we really only want to receive this 1 strict message, which is unlikely to happen often
    // now enable the interrupts to receive CAN messages in buffer 0
    PIE5bits.RXB0IE = 1;
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
    
    int canID = translateCanHeader(canMessage->header);
    TXB0SIDH = (canID >> 3) & MAX_8_BITS; // take highest 8 bits as a byte
    // take 3 bits only and set as high bits inside low byte register 
    // this also sets EXIDEN bit to 0 to only accept standard IDs (no extended ones)
    TXB0SIDL = (canID & 0b111) << 5;

    // TXB0DLC - last 4 bits should contain data length - equal to 3 now (1 byte is enough for the 1 bit only so far + few more byte more for error counts)
    TXB0DLC = 3;

    // now populate TXB0DX data registers
    TXB0D0 = canMessage->isSwitchOn << 7; // 1st bit of the 1st byte only
    TXB0D1 = TXERRCNT; // whole 2nd byte = CAN transmit error count read from the register
    TXB0D2 = RXERRCNT; // whole 3nd byte = CAN receive error count read from the register
    
    // request transmitting the message (setting the special bit)
    TXB0CONbits.TXREQ = 1; 
}
