/* 
 * 
 * Main source file for CanSwitch in PIC microcontroller.
 * 
 * File:   main.c
 * Author: pojd
 *
 * Created on October 28, 2015, 6:07 PM
 */

#include <xc.h> // all specifics for this chip
#include "utils.h" // some generic utilities (data types, etc)
#include "config.h" // my custom parameter setup for this chip
#include "can.h" // my custom can bus setup and API functions

boolean buttonPressed = FALSE;
boolean timerElapsed = FALSE;
int seconds = 0;
int hearbeatTimeout = 300; // 5 * 60 sec = 5 minutes, TODO configurable?

/*
 * Input and timer setup
 */

void configureInput() {
    // only configure B ports - since only B7:B4 have the interrupt on change feature, we need that...
    // so we configure B5 only (since B6 and B7 also have PGC and PGD that we need for debugging)
    // all other shared functionality on the pin is disabled by default, so no need to override anything
    TRISBbits.TRISB5 = 1; // avoid setting other TRIS bits on B since CAN also needs that and did already set it up before
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

/*
 * Input and timer processing
 */

void onInputChange() {
    // now confirm the PORT B is low (so we only notify on change from high to low)
    if (PORTBbits.RB5 == 0) {
        buttonPressed = TRUE; // and change the flag to let the main thread handle this message
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

int main(void) {
    configureCan();
    configureInterrupts();

    // testing only
    unsigned char testChar[] = "12345678";
    sendCanMessage(0b10001110001, testChar);
    
    while (1) {
        if (buttonPressed) {
            // TODO send CAN message (get all data you can - error registers, current B5 in PORTB, uptime, etc)
            buttonPressed = FALSE; // clear the flag
        }
        if (timerElapsed) {
            // TODO send CAN hearbeat message (maybe the same as above?)
            timerElapsed = FALSE;
        }
    }
    
    return 0;
}

