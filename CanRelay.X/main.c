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

/** 
 * These should be constants really (written and read from EEPROM)
 */

Floor floor = GROUND; // is mandated to be set in EEPROM

/*
 * Setup section
 */

void configureOutputs() {
    // configure everything as output
    TRISA = 0;
    TRISB = 0;
    TRISC = 0;
    
    // disable all analog inputs (set as digital)
    ANCON0 = 0;
    ANCON1 = 0;
    
    // enable global and peripheral interrupts only, disable other interrupts
    INTCON = 0b11000000;
    //INTCON2 and INTCON3 and ODCON are fine by default
    // TODO review all alternative port functions and disable all to allow outputs to work fine
}

void configureCan() {    
    // first move to CONFIG mode (and wait for the switch to finish)
    can_setMode(CONFIG_MODE, TRUE);

    can_setupBaudRate(BAUD_RATE, CPU_SPEED);
    
    // now setup CAN to receive only NORMAL message types for this node's floor
    CanHeader header;
    header.nodeID = floor; // node ID = floor (first bit most important only really)
    header.messageType = NORMAL;
    can_setupFirstBitIdReceiveFilter(&header);

    // switch CAN to loopback for now for testing, don't wait for the switch to finish
    can_setMode(LOOPBACK_MODE, FALSE);
}

void configure() {
    configureOutputs();
    configureCan();
}

/*
 * Can message processing
 */

void switchOutput(byte nodeID) {
    // simply go for 1-8 - PORTA, 9-16 - PORTB, 17-24 - PORTC
    // need to flip the respective bit
    if (nodeID>0 && nodeID<=8) { // PORTA
        PORTA ^= 1 << nodeID;
    } else if (nodeID>8 && nodeID<=16) { // PORTB
        PORTB ^= 1 << (nodeID-8);
    } else if (nodeID>16 && nodeID<=24) { // PORTC
        PORTC ^= 1 << (nodeID-16);
    }
}

void checkCanMessageReceived() {
    // check if CAN receive buffer 0 interrupt is enabled and interrupt flag set
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        // now confirm the buffer 0 is full and take directly 2 bytes of data from there - should be always present
        if (RXB0CONbits.RXFUL) {
            // we can only receive normal messages, so all we need to know is the canID = so the low ID register is enough
            // switch on the respective output directly from here for now
            // simply ignore 4 highest bits (message type and floor) = 4bits from high and 3 bits from low register
            byte nodeID = (RXB0SIDH >> 4) + (RXB0SIDL << 5);
            switchOutput(nodeID);
            RXB0CONbits.RXFUL = 0; // mark the data in buffer as read and no longer needed
        }
        PIR5bits.RXB0IF = 0; // clear the interrupt
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
    while (TRUE) { // nothing to be done here, all processed inside the interrupt routines
    }
    
    return 0;
}
