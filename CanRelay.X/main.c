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
#include "relayMappings.h"

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz
#define FIRMWARE_VERSION 2 // for 44pin packages, first version

/** bucket of the floor of this node in DAO */
#define FLOOR_DAO_BUCKET 0

/** 
 * These should be constants really (written and read from EEPROM)
 */

Floor floor = GROUND; // is mandated to be set in EEPROM

/**
 * These are control variables used by the main loop
 */

/** received node ID over CAN */
volatile byte receivedNodeID = 0;
/** received data over CAN in the case of operations */
volatile byte receivedDataByte = 0;

/** config data - if a config CAN message was sent */
volatile byte receivedMappingNumber = 0;
volatile byte receivedMappingNodeID = 0;
volatile byte receivedMappingOutputNumber = 0;

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
    TRISD = 0;
    TRISE = 0;
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;
    PORTD = 0;
    PORTE = 0;
    
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

    // last piece is also config messages that canRelay now supports (only changing the mappings). It has to be a strict filter though as opposed to more vague filters above
    // since CONFIG messages can be also targeted to the individual CanSwitches, so we need to assure the nodeID is equal the floor
    header.messageType = CONFIG;
    can_setupStrictReceiveFilter(&header);

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
            // see setupCan above for more details, but we can either get NORMAL or COMPLEX or CONFIG messages
            CanHeader header = can_idToHeader(&RXB0SIDH, &RXB0SIDL);

            // NORMAL and COMPLEX messages are the same processing vice
            if (header.messageType == NORMAL || header.messageType == COMPLEX) {
                // we need to know the nodeID (if it is equal to floor that the operation is for all lights)
                receivedNodeID = header.nodeID;
                // and we need just 1 byte of data then
                receivedDataByte = RXB0D0;
            } else if (header.messageType == CONFIG) {
                // in the case of CONFIG messages, we do not need the nodeID since we know it is equal to floor as per setupCan where we use strict filter
                // in this case we expect 3 bytes of data
                // first byte = number of the mapping - will drive address to store this at in EEPROM
                // second byte = nodeID of the mapping
                // last byte = output to set by this mapping (should be only up to 30 anyway)
                receivedMappingNumber = RXB0D0;
                receivedMappingNodeID = RXB0D1;
                receivedMappingOutputNumber = RXB0D2;

            } // should never received other message types
            
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

/**
 * Reads initial values from DAO and sets the config variables accordingly.
 * @return TRUE if all mandatory values were read OK, false otherwise
 */
boolean initConfigData() {
    // floor is mandatory. If it is not set, then return false (but sleep first)
    DataItem dataItem = dao_loadDataItem(FLOOR_DAO_BUCKET);
    if (!dao_isValid(&dataItem)) {
        Sleep();
        return FALSE;
    }
    floor = dataItem.value;
    
    // now init relay mapping
    initMapping();
    
    return TRUE;
}

void performOperation (Operation operation, Output *output) {    
    volatile byte* port = output->port;
    byte shift = 1 << output->portBit;

    // see http://stackoverflow.com/questions/47981/how-do-you-set-clear-and-toggle-a-single-bit-in-c-c
    switch (operation) {
        case TOGGLE:
            *port ^= shift;
            break;
        case ON:
            *port |= shift;
            break;
        case OFF:
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
    
    // data length - equal to 8 since we are sending all outputs together with actual switch count used (5 bytes), error registers and firmware version
    message.dataLength = 8;
    byte* data = &message.data;
    
    retrieveOutputStatus(data);
    data[5] = TXERRCNT;
    data[6] = RXERRCNT;
    data[7] = FIRMWARE_VERSION;
    
    can_send(&message);
}

void eraseReceivedOperationData() {
    receivedNodeID = 0;
    receivedDataByte = 0;
}

void processIncomingOperation() {
    // first take the operation from the data byte
    Operation operation = can_extractOperationFromDataByte(receivedDataByte);
    
    if (operation == GET) {
        eraseReceivedOperationData(); // we no longer need the message and can allow other messages to be received
        sendCanMessageWithAllPorts();
    } else { // other operations are setting things up
        // we should only receive nodeIDs >= floor
        if (receivedNodeID > floor) {
            // use the mapping routine to get output (port, bit) to change using the received nodeID
            // for example for nodeID 5 we need to change say PORTB, bit 2
            Output* output = nodeIDToOutput(receivedNodeID);
            if (output) { // we may have received unknown or not mapped nodeID, imn that case do nothing
                performOperation (operation, output);
            }
        } else if (receivedNodeID == floor) {
            // in this case do the same operations as above, but for all really used outputs
            UsedOutputs* outputs = getUsedOutputs();
            Output *o = outputs->array;
            Output *oEnd = outputs->array + outputs->size;
            for (; o < oEnd; o++) {
                performOperation (operation, o);                
            }
        } // < floor should never happen, in case it does, do nothing, probably missconfigured CAN filters
        eraseReceivedOperationData();
    }
}

void eraseReceivedConfigData() {
    receivedMappingNumber = 0;
    receivedMappingNodeID = 0;
    receivedMappingOutputNumber = 0;
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
            processIncomingOperation();
        }

        if (receivedMappingNumber) {
            // let the mapping do the magic
            updateMapping(receivedMappingNumber, receivedMappingNodeID, receivedMappingOutputNumber);
            eraseReceivedConfigData();
        }        
    }
    
    return 0;
}
