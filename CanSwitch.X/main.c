/* 
 * 
 * Main source file for CanSwitch in PIC microcontroller.
 * 
 * File:   main.c
 * Author: pojd
 *
 * Created on October 28, 2015, 6:07 PM
 */

#include <xc.h>
#include "config.h"
#include "utils.h"
#include "can.h"
#include "dao.h"

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz

/** 
 * These should be constants really (written and read from EEPROM)
 */

boolean suppressSwitch = FALSE;
int heartbeatTimeout = 300; // 5 * 60 sec = 5 minutes
byte nodeID = 0; // is mandated to be non-zero, checked in initConfigData()

/**
 * These are control variables used by the main loop
 */

/** was the switch pressed? */
boolean switchPressed = FALSE;

/** timer data */
boolean timerElapsed = FALSE;
int seconds = 0;

/** config data - if a config CAN message was sent */
int configData = 0;


/*
 * Setup section
 */

void configureInput() {
    // only configure B ports - since only B7:B4 have the interrupt on change feature, we need that...
    // so we configure B5 only (since B6 and B7 also have PGC and PGD that we need for debugging)
    // all other shared functionality on the pin is disabled by default, so no need to override anything
    TRISBbits.TRISB5 = 1; // avoid setting other TRIS bits on B since configure can was already called before and sets that
    // enable port change interrupt in B
    INTCONbits.RBIE = 1;
    // enable weak pull ups (only for the enabled input)
    INTCON2bits.RBPU = 0;
}

void configureTimer() {
    // enable timer, internal clock, use prescaler 1:256 (last 3 bits below)
    // as a result we have 2*16 * 256 = 16mil cycles = the speed of the CPU, so we should see an interrupt each second
    T0CON = 0b10000111;
    // enable timer interrupts
    INTCONbits.TMR0IE = 1;
}
/**
 * Configure all the interrupts on this chip as needed by this application.
 */
void configureInterrupts() {
    RCONbits.IPEN = 0; // disable priority interrupts
    INTCONbits.GIE = 1; // enable all global interrupts

    configureInput();
    configureTimer();
}

void configureCan() {
    // TRIS3 = CAN BUS RX = has to be set as INPUT, all others as outputs (we assume we are the first to setup whole TRISB here)
    TRISB = 0b00001000;

    // first move to CONFIG mode (and wait for the switch to finish)
    can_setMode(CONFIG_MODE, TRUE);

    can_setupBaudRate(BAUD_RATE, CPU_SPEED);
    
    // now setup CAN to receive only CONFIG message types for this node ID
    CanHeader header;
    header.nodeID = nodeID;
    header.messageType = CONFIG;
    can_setupStrictReceiveFilter(&header);

    // switch CAN to loopback for now for testing, don't wait for the switch to finish
    can_setMode(LOOPBACK_MODE, FALSE);
}

void configure() {
    configureCan();
    configureInterrupts();
}

/*
 * Input and timer processing
 */

void checkInputChanged() {
    // check if port B change interrupt is enabled and interrupt flag set
    if (INTCONbits.RBIE && INTCONbits.RBIF) {
        // now confirm the PORT B is low (so we only notify on change from high to low)
        if (PORTBbits.RB5 == 0) {
            switchPressed = TRUE; // and change the flag to let the main thread handle this message
        }
        INTCONbits.RBIF = 0; // clear the interrupt
    }
}

void checkTimerExpired() {
    // check if timer 0 interrupt is enabled and interrupt flag set
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {    
        if (++seconds >= heartbeatTimeout) {
            seconds = 0;
            timerElapsed = TRUE;
        }
        INTCONbits.TMR0IF = 0; // clear the interrupt
    }
}

void checkCanMessageReceived() {
    // check if CAN receive buffer 0 interrupt is enabled and interrupt flag set
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        // now confirm the buffer 0 is full and take directly 2 bytes of data from there - should be always present
        if (RXB0CONbits.RXFUL) {
            // we can only receive config messages, so no need to check what canID did we get
            // form a 16bit int from the 2 incoming bytes and let the main thread process this message then (let the interrupt finish quickly)
            configData = (RXB0D0 << 8) + RXB0D1;
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
    checkTimerExpired();
    checkInputChanged();
    checkCanMessageReceived();
}

/*
 * Action methods here
 */

void updateConfigData(DataItem *data) {
    if (dao_isValid(data)) {
        switch (data->dataType) {
            case HEARTBEAT_TIMEOUT:
                heartbeatTimeout = data->value;
                break;
            case SUPPRESS_SWITCH:
                suppressSwitch = data->value;
                break;
            case NODE_ID:
                nodeID = data->value;
                // in this case we also need to configure again to change CAN acceptance filters, etc
                configure();
                break;            
        }
    }
}

/**
 * Reads initial values from DAO and sets the config variables accordingly.
 * @return TRUE if all mandatory values were read OK, false otherwise
 */
boolean initConfigData() {
    // try nodeID, which is mandatory. If it is not set, then return false
    DataItem dataItem = dao_loadDataItem(NODE_ID);
    if (!dao_isValid(&dataItem)) {
        return FALSE;
    }
    updateConfigData (&dataItem);
    
    // other attributes are not mandatory
    
    dataItem = dao_loadDataItem(HEARTBEAT_TIMEOUT);
    updateConfigData (&dataItem);

    // even if not written before, reading as 0 is fine and we will treat it as disabled by default
    dataItem = dao_loadDataItem(SUPPRESS_SWITCH);
    updateConfigData (&dataItem);
    
    return TRUE;
}

void sendCanMessage(MessageType messageType) {
    CanHeader header;
    header.nodeID = nodeID;
    header.messageType = messageType;
    
    CanMessage message;
    message.header = &header;
    message.isSwitchOn = switchPressed;
    can_send(&message);
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
        if (switchPressed) {
            if (!suppressSwitch) {
                sendCanMessage(NORMAL);
            }
            switchPressed = FALSE;
        }
        if (timerElapsed) {
            sendCanMessage(HEARTBEAT);
            timerElapsed = FALSE;
        }
        if (configData) {
            DataItem data = dao_saveData(configData);
            updateConfigData(&data);
            configData = 0;
        }
    }
    
    return 0;
}
