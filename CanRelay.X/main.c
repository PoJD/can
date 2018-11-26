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
#define FIRMWARE_VERSION 1

/** bucket of the floor of this node in DAO */
#define FLOOR_DAO_BUCKET 0

/** marker for last mapping message */
#define MAPPINGS_END_MARKER 0xFF

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
volatile boolean receivedOperation = FALSE;

/** config data - if a config CAN message was sent */
volatile byte receivedMappingNumber = 0;
volatile byte receivedMappingNodeID = 0;
volatile byte receivedMappingOutputNumber = 0;

/** request for mappings received? */
volatile boolean receivedMappingsRequest = FALSE;

/**
 * Timer data
 */
volatile unsigned long tQuarterSecSinceStart = 0;

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
    // this filter would be associated with another receive buffer with lower priority, so we need to remember checking that in can interrupt too
    header.messageType = CONFIG;
    can_setupStrictReceiveFilter(&header);

    // now also register to listen to MAPPINGS requests, also strict filters
    header.messageType = MAPPINGS;
    can_setupStrictReceiveFilter(&header);

    // switch CAN to normal mode
    can_setMode(NORMAL_MODE);
}


void configureTimer() {
    // enable timer, internal clock, use prescaler 1:16 (last 3 bits below)
    // so we have 2^16 * 16 = 1mil oscillator cycles. Since 4 oscillator cycles made up 1 instruction cycle,
    // we have 4mil oscillator cycles each second, so we should see an interrupt 4 times a second
    T0CON = 0b10000011;
    // enable timer interrupts
    INTCONbits.TMR0IE = 1;
}

void configure() {
    configureSpeed();    
    configureOutputs();
    configureCan();
    configureTimer();
}

/*
 * Can message processing
 */

void checkCanMessageReceived() {
    // check if CAN receive buffer 0 interrupt is enabled and interrupt flag set
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        // now confirm the buffer 0 is full
        if (RXB0CONbits.RXFUL) {
            if (RXB0DLCbits.DLC >= 1) { // make sure we received at least one byte in the CAN data frame
                // see setupCan above for more details, but we can either get NORMAL or COMPLEX messages in buffer 0, but we process them the same way actually
                CanHeader header = can_idToHeader(&RXB0SIDH, &RXB0SIDL);
                // we need to know the nodeID (if it is equal to floor that the operation is for all lights)
                receivedNodeID = header.nodeID;
                // and we need just 1 byte of data then
                receivedDataByte = RXB0D0;
                // just set a flag since both of the above could actually be 0 (nodeID 0 could potentially be sent and databyte too)
                receivedOperation = TRUE;
            }
            
            RXB0CONbits.RXFUL = 0; // mark the data in buffer as read and no longer needed
        }
        PIR5bits.RXB0IF = 0; // clear the interrupt flag now
    }
    
    // check if CAN receive buffer 1 interrupt is enabled and interrupt flag set
    if (PIE5bits.RXB1IE && PIR5bits.RXB1IF) {
        // now confirm the buffer 1 is full
        if (RXB1CONbits.RXFUL) {
            // see setupCan above for more details, but we can only get CONFIG or MAPPINGS messages in this buffer
            // which makes it a bit more robust since these shall be really lower priority unlike buffer 0 anyway
            // this time we do not need to know the nodeID since we know it is equal to floor as per setupCan where we use strict filter
            CanHeader header = can_idToHeader(&RXB1SIDH, &RXB1SIDL);
            if (CONFIG == header.messageType && RXB1DLCbits.DLC == 3) {
                // in this case we expect 3 bytes of data
                // first byte = number of the mapping - will drive address to store this at in EEPROM
                // second byte = nodeID of the mapping
                // last byte = output to set by this mapping (should be only up to 30 anyway)
                receivedMappingNumber = RXB1D0;
                receivedMappingNodeID = RXB1D1;
                receivedMappingOutputNumber = RXB1D2;
            } else if (MAPPINGS == header.messageType) { // in this case we do not care about data frame length
                receivedMappingsRequest = TRUE;
            }

            RXB1CONbits.RXFUL = 0; // mark the data in buffer as read and no longer needed
        }
        PIR5bits.RXB1IF = 0; // clear the interrupt flag now
    }
}

/**
 * Timer processing
 */

void checkTimerExpired() {
    // check if timer 0 interrupt is enabled and interrupt flag set, if so, just increase the counter of quarters of second
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {
        tQuarterSecSinceStart++;
        INTCONbits.TMR0IF = 0; // clear the interrupt
    }
}

/**
 * Fired when an interrupt occurs.
 */
void interrupt handleInterrupt(void) {
    checkTimerExpired();
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
    // floor is mandatory. If it is not set, then put the device to sleep
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
    receivedOperation = FALSE;
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
            // pass in also the current time since the method internally also checks for too fast operations for a given output
            Output* output = nodeIDToOutput(receivedNodeID, tQuarterSecSinceStart);
            // we may have received unknown or not mapped nodeID, in that case do nothing
            // or it may be too soon after last message for this output
            if (output) { 
                performOperation (operation, output);
            }
        } else if (receivedNodeID == floor) {
            // in this case do the same operations as above, but for all really used outputs
            // in this case the internal time for the outputs is not used, so this operation can be invoked very fast via CAN API
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

void sendOneCanMessageWithMappings(CanMessage* message, byte dataLength) {
    message->dataLength = dataLength;
    can_sendSynchronous(message);
}

void sendCanMessagesWithAllMappings() {
    CanHeader header;
    header.nodeID = floor;
    header.messageType = MAPPINGS_REPLY;
    
    CanMessage message;
    message.header = &header;
    byte* data = &message.data;
    
    // now loop through all the mappings, split it to chunks of 8 bytes top per messages and send all the messages with mappings, no size now
    Mappings* mappings = getRuntimeMappings();
    Mapping *m = mappings->array;
    Mapping *mEnd = mappings->array + mappings->size;
    
    byte dataLength = 0;
    for (; m < mEnd; m++) {
        *data++ = m->nodeID;
        *data++ = m->outputNumber;
        dataLength += 2;
        if (dataLength == 8) {
            // we have enough data to send can message, so send it now and reset data pointer to start from data 0 again
            sendOneCanMessageWithMappings(&message, dataLength);
            dataLength = 0;
            data = &message.data;
        }
    }
    
    // append 2 markers (so that we always see pairs of numbers)
    // and any clients listening to this traffic would know that this is the last message
    // we may have some residual data already from the above loop, but 6 bytes top, so we can add 2 more
    *data++ = MAPPINGS_END_MARKER;
    *data++= MAPPINGS_END_MARKER;
    dataLength+=2;    
    sendOneCanMessageWithMappings(&message, dataLength);
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
        // received a message over CAN, so react to that - use the flag to detect am operation was received
        if (receivedOperation) {
            processIncomingOperation();
        }

        if (receivedMappingNumber) {
            // let the mapping do the magic
            updateMapping(receivedMappingNumber, receivedMappingNodeID, receivedMappingOutputNumber);
            eraseReceivedConfigData();
        }
        
        if (receivedMappingsRequest) {
            receivedMappingsRequest = FALSE;
            sendCanMessagesWithAllMappings();
        }
    }
    
    return 0;
}
