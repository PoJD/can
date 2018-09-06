/* 
 * Main source file for CanRelay in PIC microcontroller.
 * 
 * File:   main.c
 * Author: pojd
 *
 * Created on November 18, 2015, 3:14 PM
 */

#include <xc.h>
#include "config.h"
#include "utils.h"
#include "can.h"
#include "dao.h"
#include "canSwitches.h"

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz
#define FIRMWARE_VERSION 1

/** 
 * These should be constants really (written and read from EEPROM)
 */

Floor floor = GROUND; // is mandated to be set in EEPROM

/**
 * These are control variables used by the main loop
 */

/** received node ID over CAN */
byte receivedNodeID = 0;
/** received data over CAN. Should never be 0, if it is, this relay would ignore such traffic anyway */
byte receivedDataByte = 0;

/*
 * Setup section
 */

void configureSpeed() {
    // setup external oscillator - use default primary oscillator = as defined by config.h (last 2 bits)
    OSCCON = 0;
}

void configureOutputs() {
    // configure everything as output and set everything as 0 initially
    TRISA = 0;
    TRISB = 0;
    TRISC = 0;
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
    
    // disable all analog inputs (set as digital)
    ANCON0 = 0;
    ANCON1 = 0;
    
    // enable global and peripheral interrupts only, disable other interrupts
    INTCON = 0b11000000;
    //INTCON2 and INTCON3 and ODCON are fine by default
}

void configureCan() {
    can_init();
    
    // first move to CONFIG mode
    can_setMode(CONFIG_MODE);

    can_setupBaudRate(BAUD_RATE, CPU_SPEED);
    
    // now setup CAN to receive only NORMAL message types for this node's floor
    CanHeader header;
    header.nodeID = floor; // node ID = floor (first bit most important only really)
    header.messageType = NORMAL;
    can_setupFirstBitIdReceiveFilter(&header);
    
    // in addition to the above, also setup filter to receive complex message types for the same node's floor
    header.messageType = COMPLEX;
    can_setupFirstBitIdReceiveFilter(&header);

    // switch CAN to normal mode
    can_setMode(NORMAL_MODE);
}

void configure() {
    configureSpeed();    
    configureOutputs();
    configureCan();
}

/*
 * Can message processing
 */

void checkCanMessageReceived() {
    // check if CAN receive buffer 0 interrupt is enabled and interrupt flag set
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        // now confirm the buffer 0 is full
        if (RXB0CONbits.RXFUL) {
            // we can only receive normal and complex messages, so we need to know the canID
            CanHeader header = can_idToHeader(&RXB0SIDH, &RXB0SIDL);
            // also ignore highest bit of node ID = floor and take just 1st byte of data - never more
            receivedNodeID = header.nodeID & MAX_7_BITS;
            receivedDataByte = RXB0D0;
            
            RXB0CONbits.RXFUL = 0; // mark the data in buffer as read and no longer needed
        }
        PIR5bits.RXB0IF = 0; // clear the interrupt flag now
    }    
}

/**
 * Fired when an interrupt occurs. Internally checks for the port b interrupt being enabled and the flag being set and then processes it accordingly
 * Or sends a heartbeat message if the timer occurred otherwise
 */
void interrupt handleInterrupt(void) {
    checkCanMessageReceived();
}

/*
 * Action methods here
 */

void updateConfigData(DataItem *data) {
    if (dao_isValid(data)) {
        switch (data->dataType) {
            case FLOOR:
                floor = data->value;
                break;
        }
    }
}

/**
 * Reads initial values from DAO and sets the config variables accordingly.
 * @return TRUE if all mandatory values were read OK, false otherwise
 */
boolean initConfigData() {
    // try floor, which is mandatory. If it is not set, then return false
    DataItem dataItem = dao_loadDataItem(FLOOR);
    if (!dao_isValid(&dataItem)) {
        return FALSE;
    }
    updateConfigData (&dataItem);
    return TRUE;
}

void performOperation (byte receivedDataByte, volatile byte* port, byte shift) {
    // see http://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit-in-c-c
    switch (receivedDataByte) {
        case COMPLEX_OPERATOR_SWITCH:
            *port ^= shift;
            break;
        case COMPLEX_OPERATOR_SET:
            *port |= shift;
            break;
        case COMPLEX_OPERATOR_CLEAR:
            *port &= ~shift;
            break;
    }
}

void sendCanMessageWithAllPorts() {
    CanHeader header;
    // set node ID to be the very first node ID on the given floor so that we can find out what relay is actually replying
    header.nodeID = floor;
    header.messageType = COMPLEX_REPLY;
    
    CanMessage message;
    message.header = &header;
    
    // data length - equal to 6 since we are sending all PORTA, PORTB and PORTC together with error registers and firmware version
    message.dataLength = 6;
    byte* data = &message.data;
    *data++ = PORTA;
    *data++ = PORTB;
    *data++ = PORTC;
    *data++ = TXERRCNT;
    *data++ = RXERRCNT;
    *data++ = FIRMWARE_VERSION;
    
    can_send(&message);
}

void eraseReceivedCanMessage() {
    receivedNodeID = 0;
    receivedDataByte = 0;
}

void processIncomingTraffic() {
    if (receivedDataByte == COMPLEX_OPERATOR_GET) {
        eraseReceivedCanMessage(); // we no longer need the message and can allow other messages to be received
        sendCanMessageWithAllPorts();
    } else { // other operations are setting things up
        if (receivedNodeID > 0) {
            // simply go for 1-8 - PORTA, 9-16 - PORTB, 17-24 - PORTC
            // (mind that some IDs should not be used - see README)
            // need to flip/set/clear the respective bit in respective PORT - shift by 0 up to 7 bits

            // find out the number in 0..7 - to find out the respective bit to change
            // simply decrease by 1 and take just the 3 bits. Then move it to find out the shift we need below
            byte shift = 1 << ((receivedNodeID-1) & 0b111);

            volatile byte* port;
            if (receivedNodeID>0 && receivedNodeID<=8) {
                port = &PORTA;
            } else if (receivedNodeID>8 && receivedNodeID<=16) {
                port = &PORTB;
            } else if (receivedNodeID>16 && receivedNodeID<=24) {
                port = &PORTC;
            }

            performOperation (receivedDataByte, port, shift);
        } else {
            // node ID 0 - means do the same operations as above, but for all ports
            performOperation (receivedDataByte, &PORTA, 0b11111111);
            performOperation (receivedDataByte, &PORTB, 0b11111111);
            performOperation (receivedDataByte, &PORTC, 0b11111111);
        }
        eraseReceivedCanMessage();
    }
}

/**
 * Main method runs and checks various flags potentially set by various interrupts - invokes action points upon such a condition
 */
int main(void) {
    // find out some initial values to use to configure this node
    // some are mandatory and refuse to startup if these are not set
    if (!initConfigData()) {
        return -1;
    }
    
    // all OK, so start up
    configure();
    
    // main loop
    while (TRUE) {
        // received a message over CAN, so react to that - databyte is never null, if it is, we would just ignore it anyway
        if (receivedDataByte) {
            processIncomingTraffic();
        }
    }
    
    return 0;
}
