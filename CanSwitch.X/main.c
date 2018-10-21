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
#include "canSwitches.h"

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // clock speed in MHz (4 clocks made up 1 instruction)
#define FIRMWARE_VERSION 2 // in sync with CanRelay - latest changes for 8 inputs, etc

/** 
 * These should be constants really (written and read from EEPROM)
 */

/** 
 * debug mode utilizes timer, LED status, hearbeats, etc, otherwise low power mode most of the time
 */
boolean DEBUG = FALSE;
boolean suppressSwitch = FALSE;
int tHeartbeatTimeout = 10; // 10seconds default
byte nodeID = 0; // is mandated to be non-zero, checked in initConfigData()

/**
 * These are control variables used by the main loop
 */

/** was the switch pressed? */
volatile boolean switchPressed = FALSE;

/** counter for pressing the switch */
volatile unsigned long switchCounter = 0;

/** current status of PORTB pins detected during the interrupt routine */
volatile byte portbStatus = 0;

/** 
 * The unsigned longs below would by definition turn to 0 when they reach max. 
 * So the only issue to cover is the check of the tLastSwitchQuarterSec against tQuarterSecSinceStart
 * which is covered in the main loop. tQuarterSecSinceStart being left the only long not reset in the code to avoid reaching max anywhere */

/** last time (in quarter of seconds) when a switch was pressed */
volatile unsigned long tLastSwitchQuarterSec = 0;

/** timer data */
volatile boolean timerElapsed = FALSE;
volatile unsigned long tQuarterSecSinceStart = 0;
volatile unsigned long tSecsSinceHeartbeat = 0;

/** config data - if a config CAN message was sent */
volatile int configData = 0;


/*
 * Setup section
 */

void configureSpeed() {
    // setup external oscillator - use default primary oscillator = as defined by config.h (last 2 bits)
    OSCCON = 0;
    
    // enable ultra low power regulator in sleep, see also RETEN in config.h
    WDTCONbits.SRETEN = 1; 
}

void clearInterruptFlags() {
    INTCONbits.INT0IF = 0;
    INTCON3bits.INT1IF = 0;
    INTCON3bits.INT2IF = 0;
    INTCON3bits.INT3IF = 0;
    INTCONbits.RBIF = 0;    
}

void enableInputInterrupts(boolean enable) {
    // clear the interrupt flags (could be set before)
    clearInterruptFlags();
    
    // till now enable/disable external interrupt (change on portb0:3)
    INTCONbits.INT0IE = enable;
    INTCON3bits.INT1IE = enable;
    INTCON3bits.INT2IE = enable;
    INTCON3bits.INT3IE = enable;
    
    // the same for interrupt on change (change on portb4:7)
    INTCONbits.RBIE = enable;
}

void configureInput() {
    // only configure B ports - all as inputs
    // all other shared functionality on the pin is disabled by default, so no need to override anything
    TRISB = 0b11111111;
    
    // configure other ports as outputs as set to 0 (C ports would get set properly later for CAN)
    TRISA = 0;
    TRISC = 0;
    PORTA = 0;
    PORTB = 0;
    PORTC = 0;

    // disable all analog inputs (set as digital)
    ANCON0 = 0;
    ANCON1 = 0;
    
    // enable weak pull ups for all B ports
    INTCON2bits.RBPU = 0;
    WPUB = 0b11111111;
        
    // interrupt on falling change on b0-b3 (pull up keep it high, only interrupt on "value down")
    INTCON2bits.INTEDG0 = 0;
    INTCON2bits.INTEDG1 = 0;
    INTCON2bits.INTEDG2 = 0;
    INTCON2bits.INTEDG3 = 0;
    
    // enable all interrupts on change on b4:7
    IOCB = 0b11110000;

    // now wait some time for potential false triggers to sink in before enabling the interrupt below
    for (byte i=0; i<5; i++) {
        NOP();
    }
    
    enableInputInterrupts(TRUE);
}

void configureTimer() {
    if (DEBUG) {
        // enable timer, internal clock, use prescaler 1:16 (last 3 bits below)
        // so we have 2^16 * 16 = 1mil oscillator cycles. Since 4 oscillator cycles made up 1 instruction cycle,
        // we have 4mil oscillator cycles each second, so we should see an interrupt 4 times a second
        T0CON = 0b10000011;
        // enable timer interrupts
        INTCONbits.TMR0IE = 1;
    }
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
    can_initRcPortsForCan();
    
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
    
    // now enable CAN interrupts (only in debug mode, otherwise not needed and not used, just unnecessary load)
    if (DEBUG) {
        PIE5bits.ERRIE = 1;  // Enable CAN BUS error interrupt
        PIE5bits.TXB0IE = 1; // Enable Transmit Buffer 0 Interrupt        
    }
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
    // check if external input change is enabled and interrupt flag set (B0:B3)
    // check if input on change is enabled and interrupt flag set (B4:B7)
    if ( (INTCONbits.INT0IE && INTCONbits.INT0IF) || (INTCON3bits.INT1IE && INTCON3bits.INT1IF) ||
         (INTCON3bits.INT2IE && INTCON3bits.INT2IF) || (INTCON3bits.INT3IE && INTCON3bits.INT3IF) || 
         (INTCONbits.RBIE && INTCONbits.RBIF) ) {
        // read PORTB as mandated in datasheet to clear the input change mismatch - this would also be used by main thread to send respective CAN message
        // reverse it first so that the main routine can rely on 1 meaning the respective input is ON
        portbStatus = ~PORTB;

        // at least s1 instruction cycle after read is mandated in datasheet to properly clear the interrupt on input!

        // change the flag to let the main thread handle input change
        switchPressed = TRUE;
        switchCounter++;
        clearInterruptFlags();
    }
}

void updateTick() {
    if ( (++tQuarterSecSinceStart) % 4 == 0) { // we can safely let this go infinitely - would turn to 0 after max unsigned long
        tSecsSinceHeartbeat ++;
    }
}

void checkHeartBeat() {
    if (tSecsSinceHeartbeat >= tHeartbeatTimeout) {
        tSecsSinceHeartbeat = 0;
        timerElapsed = TRUE;
    }    
}

void switchStatusLedOn() {
    if (DEBUG) {
        PORTCbits.RC1 = 1;
    }
}

void switchStatusLedOff() {
    if (DEBUG) {
        PORTCbits.RC1 = 0;
    }
}

void checkStatus() {
    int timeDiff = tQuarterSecSinceStart - messageStatus.timestamp;
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

/**
 * Sleep and wake up routines
 */

void switchTransceiverOn() {
    PORTCbits.RC3 = 0;
}

void switchTransceiverOff() {
    PORTCbits.RC3 = 1;
}

void sleepDevice() {
    if (!DEBUG) {
        // setup SLEEP mode of the CAN module to save power
        can_setMode(SLEEP_MODE);
        // apply high on standby pin of transceiver to put it into sleep and low power mode (15uA top from datasheet)
        switchTransceiverOff();
        // enter sleep mode now to be waken up by interrupt later, some other power saving settings also kick in now, for example ultra low power voltage regulator
        Sleep();
    }
}

void wakeUpDevice() {
    if (!DEBUG) {
        switchTransceiverOn();
        can_setMode(NORMAL_MODE);

        // we need to wait 50us now since the transceiver could just be going back from standby
        // datasheet says 40us max, we will be conservative and do 50us instead
        for (int i=0; i<CPU_SPEED * 50; i++) {
            NOP();
        }
    }
}


void updateConfigData(DataItem *data) {
    if (dao_isValid(data)) {
        switch (data->dataType) {
            case HEARTBEAT_TIMEOUT:
                tHeartbeatTimeout = data->value;
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
    // try nodeID, which is mandatory. If it is not set, then nothing to be done here, so just sleep the device
    DataItem dataItem = dao_loadDataItem(NODE_ID);
    if (!dao_isValid(&dataItem)) {
        sleepDevice();
    }
    updateConfigData (&dataItem);
    
    // now also use the nodeID as the number of seconds already passed from the timer
    // helps avoiding burst of heartbeat messages from all can switches on the network at the same time
    // they should evenly split by a second as a result of this
    tSecsSinceHeartbeat = nodeID;
    
    // other attributes are not mandatory
    
    dataItem = dao_loadDataItem(HEARTBEAT_TIMEOUT);
    updateConfigData (&dataItem);

    // even if not written before, reading as 0 is fine and we will treat it as disabled by default
    dataItem = dao_loadDataItem(SUPPRESS_SWITCH);
    updateConfigData (&dataItem);
    
    return TRUE;
}

void sendCanMessage(MessageType messageType, byte portBPin) {
    // just 1 blink until next timer interrupt
    //switchStatusLedOn();
    messageStatus.timestamp = tQuarterSecSinceStart;
    
    CanHeader header;
    header.nodeID = nodeID + portBPin; // for B0 directly nodeID is sent, up to nodeId+7 depending on which pin was set
    header.messageType = messageType;
    
    CanMessage message;
    message.header = &header;
    
    // data length - equal to 6 for heartbeat and 1 for normal messages
    message.dataLength = (messageType == HEARTBEAT) ? 6:1;

    // data - set operation to be toggle, pass in the other args too to combine first byte all the time
    byte* data = &message.data;
    
    // the actual encoding/decoding has to match between relay and switch, wrapped inside can method below
    *data++ = can_combineCanDataByte(TOGGLE, TXERRCNT+RXERRCNT, FIRMWARE_VERSION, switchCounter);
    unsigned long timeSinceStart = tQuarterSecSinceStart / 4;
    
    if (messageType == HEARTBEAT) {
        // whole 2nd byte = CAN transmit error count read from the register
        *data++ = TXERRCNT;
        // whole 3rd byte = CAN receive error count read from the register
        *data++ = RXERRCNT;
        // another byte = firmware version
        *data++ = FIRMWARE_VERSION;
        // last 2 bytes = time since start. Ignore any values higher, only used in debugging anyway
        *data++ = (timeSinceStart >> 8) & MAX_8_BITS;
        *data++ = timeSinceStart & MAX_8_BITS;
    }
    // use the synchronous version to make sure the message is really sent before moving on
    can_sendSynchronous(&message);
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
    
    while (TRUE) {
        wakeUpDevice();
        if (switchPressed) {
            // dropped condition for time check since that would require a timer
            if (!suppressSwitch) {
                // now loop through all PORTB pins as were set in interrupt routine and for all low (could be multiple), send a CAN message out
                // we have 1s there where the input is on right now (weak pull ups but reverted in portbChanged function)
                for (int i=0; i<8; i++) {
                    if (portbStatus & (1 << i)) {
                        sendCanMessage(NORMAL, i);
                    }
                }
            }
            switchPressed = FALSE;
        }
        if (timerElapsed) {
            sendCanMessage(HEARTBEAT, 0);
            timerElapsed = FALSE;
        }
        if (configData) {
            DataItem data = dao_saveData(configData);
            updateConfigData(&data);
            configData = 0;
        }
        sleepDevice(); // now need to loop infinitely, interrupt will wake the device up and continue from next instruction -i.e. start of this loop
    }
    
    return 0;
}
