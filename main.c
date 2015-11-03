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

#define BAUD_RATE 50 // speed in kbps
#define CPU_SPEED 16 // speed in MHz

/** 
 * These should be constants really (written and read from EEPROM)
 */

boolean suppressSwitch = FALSE;
int hearbeatTimeout = 300; // 5 * 60 sec = 5 minutes
byte nodeID = 0b10101010;

/**
 * These are control variables used by the main loop
 */

boolean switchPressed = FALSE;
boolean timerElapsed = FALSE;
int seconds = 0;


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

    // loopback for now for testing
    can_init(BAUD_RATE, CPU_SPEED, LOOPBACK_MODE);
}

/*
 * Input and timer processing
 */

void onInputChange() {
    // now confirm the PORT B is low (so we only notify on change from high to low)
    if (PORTBbits.RB5 == 0) {
        switchPressed = TRUE; // and change the flag to let the main thread handle this message
    }
    INTCONbits.RBIF = 0; // clear the interrupt
}

void onTimer() {
    if (++seconds >= hearbeatTimeout) {
        seconds = 0;
        timerElapsed = TRUE;
    }
    INTCONbits.TMR0IF = 0; // clear the interrupt
}

/**
 * Fired when an interrupt occurs. Internally checks for the port b interrupt being enabled and the flag being set and then processes it accordingly
 * Or sends a heartbeat message if the timer occurred otherwise
 */
void interrupt handleInterrupt(void) {
    // check if port B change interrupt is enabled and interrupt flag set
    if (INTCONbits.RBIE && INTCONbits.RBIF) {
        onInputChange();
    }
    // check if timer 0 interrupt is enabled and interrupt flag set
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {
        onTimer();
    }
}

void sendCanMessage(MessageType messageType) {
    CanMessage message;
    message.isSwitchOn = switchPressed;
    message.messageID = nodeID;
    message.messageType = messageType;
    can_send(&message);
}

int main(void) {
    configureCan();
    configureInterrupts();

    while (1) {
        if (!suppressSwitch && switchPressed) {
            sendCanMessage(NORMAL);
            switchPressed = FALSE; // clear the flag
        }
        if (timerElapsed) {
            sendCanMessage(HEARTBEAT);
            timerElapsed = FALSE;
        }
    }
    
    return 0;
}

// TODO: EEPROM data (or maybe flash since these should be pretty much constants really, but should survive restart)
// 1) ID of this NODE (read it on startup) - 8 bits only - 1st bit should be the floor, so leaves us with 127 nodes per floor
// 2) suppress switch flag
// 3) Heartbeat timeout
