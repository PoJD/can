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
#define CPU_SPEED 16 // clock speed in MHz (4 clocks made up 1 instruction)

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
/** last time (in quarter of seconds) when a switch was pressed */
unsigned long lastSwitchPressed = 0;

/** timer data */
boolean timerElapsed = FALSE;
unsigned long quarterSecond = 0;
unsigned long seconds = 0;

/** config data - if a config CAN message was sent */
int configData = 0;


/*
 * Setup section
 */

void configureSpeed() {
    // setup external oscillator - use default primary oscillator = as defined by config.h (last 2 bits)
    OSCCON = 0;
}

void configureInput() {
    // only configure B ports - since only B3:B0 have the external interrupt change feature, we need that...
    // so we configure B0 only 
    // all other shared functionality on the pin is disabled by default, so no need to override anything
    TRISB = 0b00000001;
    
    // configure other ports as outputs as set to 0
    TRISA = 0;
    TRISC = 0;
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;

    // disable all analog inputs (set as digital)
    ANCON0 = 0;
    ANCON1 = 0;
    
    // enable weak pull ups (only for the enabled input)
    INTCON2bits.RBPU = 0;
    WPUBbits.WPUB0 = 1;
    
    // now wait some time for the above false triggers to sink in before enabling the interrupt below
    for (byte i=0; i<5; i++) {
        NOP();
    }
    
    // now clear the interrupt flag (could be set on startup)
    INTCONbits.INT0IF = 0;
    
    // interrupt on falling change (pull up keep it high, only interrupt on "key down")
    INTCON2bits.INTEDG0 = 0;
    // till now enable external interrupt 0 (PORTB0 change)
    INTCONbits.INT0IE = 1;
}

void configureTimer() {
    // enable timer, internal clock, use prescaler 1:16 (last 3 bits below)
    // so we have 2^16 * 16 = 1mil oscillator cycles. Since 4 oscillator cycles made up 1 instruction cycle,
    // we have 4mil oscillator cycles each second, so we should see an interrupt 4 times a second
    T0CON = 0b10000011;
    // enable timer interrupts
    INTCONbits.TMR0IE = 1;
}
/**
 * Configure all the interrupts on this chip as needed by this application.
 */
void configureInterrupts() {
    RCONbits.IPEN = 0; // disable priority interrupts
    INTCONbits.GIE = 1; // enable all global interrupts
    INTCONbits.PEIE = 1; // enable peripheral interrupts

    configureInput();
    configureTimer();
}

void configureCan() {
    can_init();
    
    // first move to CONFIG mode
    can_setMode(CONFIG_MODE);

    can_setupBaudRate(BAUD_RATE, CPU_SPEED);
    
    // now setup CAN to receive only CONFIG message types for this node ID
    CanHeader header;
    header.nodeID = nodeID;
    header.messageType = CONFIG;
    can_setupStrictReceiveFilter(&header);

    // switch CAN to normal mode (using real underlying CAN)
    can_setMode(NORMAL_MODE);
    
    // now enable CAN interrupts    
    PIE5bits.ERRIE = 1;  // Enable CAN BUS error interrupt
    PIE5bits.TXB0IE = 1; // Enable Transmit Buffer 0 Interrupt
}

void configure() {
    configureSpeed();
    configureInterrupts();
    configureCan();
}

/*
 * Input and timer processing
 */

void checkInputChanged() {
    // check if external input change is enabled and interrupt flag set (we know we will only be notified on falling edge)
    if (INTCONbits.INT0IE && INTCONbits.INT0IF) {
        switchPressed = TRUE; // and change the flag to let the main thread handle this message
        INTCONbits.INT0IF = 0; // clear the interrupt
    }
}

void updateTick() {
    if ( (++quarterSecond) % 4 == 0) { // we can safely let this overflow
        seconds ++;
    }
}

void checkHeartBeat() {
    if (seconds >= heartbeatTimeout) {
        seconds = 0;
        timerElapsed = TRUE;
    }    
}

void switchStatusLedOn() {
    PORTCbits.RC1 = 1;
}

void switchStatusLedOff() {
    PORTCbits.RC1 = 0;
}

void checkStatus() {
    int timeDiff = quarterSecond - messageStatus.timestamp;
    if (timeDiff > 8) { //2 secs passed, nothing else to be done here
        switchStatusLedOff();
        // and erase the message status, so that we cannot be triggered again
        messageStatus.timestamp = 0;
        messageStatus.statusCode = NOTHING_SENT;
        return;
    }

    // otherwise we are within 2 secs after message sent
    switch (messageStatus.statusCode) {
        // if message is just sending, switch off the status (was switched on earlier when sending CAN message)
        // it could be that the status already changed, but we have to reflect the blink by switching it off here first
        case SENDING:
            if (timeDiff > 1) {
                switchStatusLedOff();
            }
            break;
        case OK:
            if (timeDiff > 0) { // switch on all the time
                switchStatusLedOn();
            }
            break;
        case ERROR:
            // alternate the status here
            if (timeDiff % 2 == 0) {
                switchStatusLedOn();
            } else {
                switchStatusLedOff();
            }
            break;
    }
}

void checkTimerExpired() {
    // check if timer 0 interrupt is enabled and interrupt flag set
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {
        updateTick();
        checkHeartBeat();
        checkStatus();
        INTCONbits.TMR0IF = 0; // clear the interrupt
    }
}

void checkCan() {
    if (PIE5bits.RXB0IE && PIR5bits.RXB0IF) {
        // CAN receive buffer 0 interrupt
        // now confirm the buffer 0 is full and take directly 2 bytes of data from there - should be always present
        if (RXB0CONbits.RXFUL) {
            // we can only receive config messages, so no need to check what canID did we get
            // form a 16bit int from the 2 incoming bytes and let the main thread process this message then (let the interrupt finish quickly)
            configData = (RXB0D0 << 8) + RXB0D1;
            RXB0CONbits.RXFUL = 0; // mark the data in buffer as read and no longer needed
        }
        PIR5bits.RXB0IF = 0;
    } else if (PIE5bits.TXB0IE && PIR5bits.TXB0IF) {
        // Transmit Buffer 0 interrupt
        // means the CAN send just finished OK
        messageStatus.statusCode = OK;
        PIR5bits.TXB0IF = 0;
    } else if (PIE5bits.ERRIE && PIR5bits.ERRIF) {
        // Can Module Error Interrupt
        // means some error in CAN happened
        messageStatus.statusCode = ERROR;
        PIR5bits.ERRIF = 0;
    }
}

/**
 * Fired when an interrupt occurs. Internally checks for the port b interrupt being enabled and the flag being set and then processes it accordingly
 * Or sends a heartbeat message if the timer occurred otherwise
 */
void interrupt handleInterrupt(void) {
    checkTimerExpired();
    checkInputChanged();
    checkCan();
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
    
    // now also use the nodeID as the number of seconds already passed from the timer
    // helps avoiding burst of heartbeat messages from all can switches on the network at the same time
    // they should evenly split by a second as a result of this
    seconds = nodeID;
    
    // other attributes are not mandatory
    
    dataItem = dao_loadDataItem(HEARTBEAT_TIMEOUT);
    updateConfigData (&dataItem);

    // even if not written before, reading as 0 is fine and we will treat it as disabled by default
    dataItem = dao_loadDataItem(SUPPRESS_SWITCH);
    updateConfigData (&dataItem);
    
    return TRUE;
}

void sendCanMessage(MessageType messageType) {
    // just 1 blink until next timer interrupt
    switchStatusLedOn();
    messageStatus.timestamp = quarterSecond;
    
    CanHeader header;
    header.nodeID = nodeID;
    header.messageType = messageType;
    
    CanMessage message;
    message.header = &header;
    
    // data length - equal to 3 for heartbeat and 1 for normal messages
    // (1 byte is enough for the 1 bit of data only so far (is it switched on) + few more byte more for error counts)
    message.dataLength = (messageType == HEARTBEAT) ? 3:1;
    // 1st byte 1st bit = is the switch on?
    message.data[0] = switchPressed << 7;
    
    if (messageType == HEARTBEAT) {
        // whole 2nd byte = CAN transmit error count read from the register
        message.data[1] = TXERRCNT;
        // whole 3rd byte = CAN receive error count read from the register
        message.data[2] = RXERRCNT;
    }
    
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
            // only react to the switch press if at least half of a second elapsed
            // otherwise it could be faulty switch sending signals more often
            // avoid using message timestamp since that is also set by heartbeat messages 
            // so we would get some sporadic no reaction to switch press if done right after a hearbeat
            if (!suppressSwitch && (quarterSecond - lastSwitchPressed) > 1) {
                lastSwitchPressed = quarterSecond;
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
