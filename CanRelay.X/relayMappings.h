/* 
 * Relay mappings covers all configuration needed for the relay to properly translate from nodeIDs (canIDs) into respective output pins to get switched on in the CanRelay board.
 * 
 * File:   relayMappings.h
 * Author: pojd
 *
 * Created on October 18, 2018, 11:26 PM
 */

#ifndef RELAYMAPPINGS_H
#define	RELAYMAPPINGS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <xc.h>
#include "utils.h"

/**
 * Mapping from canID -> pair of reference to port and bit to change
 */
typedef struct map {
    int canID; // can ID transmitted over the wire (typically nodeID + x for a given smart wall switch depending on which light switch was pressed in that room)
    volatile byte* port; // reference to one of the port registers of the chip (e.g. PORTA, PORTB, etc)
    byte shift; // actual bit shift - which bit in that port range should be changed. only 1 of the 8 bits shall be set to 1 then. E.g. 0b00010000 would represent changing bit 4
} mapping;

/** 
 * Real mapping to port and bit shifts.
 * 
 * Each line here matches 1 label in output on relay silkscreen. I.e. line 1 matches label 1 on ground floor, line 2 label 2, etc. 
 * It should match can-pcb schematic of pin outputs on the chip
 * TODO update first value below to match the respective canIDs being sent over the wire once it is all clear (how many switches per room). For now it is is just from 1 upwards
 * TODO e.g. perhaps good way to do that would be to use canSwitches.h defines + x, e.g. living_room+1, etc?
 */
const mapping mappings[] = { 
    // ground floor mappings
    { 1,  &PORTD, 0b10000000 }, // RD7  O1
    { 2,  &PORTB, 0b00000001 }, // RB0  O2
    { 3,  &PORTD, 0b01000000 }, // RD6  O3
    { 4,  &PORTB, 0b00000010 }, // RB1  O4
    { 5,  &PORTD, 0b00100000 }, // RD5  O5
    { 6,  &PORTB, 0b00010000 }, // RB4  O6
    { 7,  &PORTD, 0b00010000 }, // RD4  O7
    { 8,  &PORTB, 0b00100000 }, // RB5  O8
    { 9,  &PORTC, 0b10000000 }, // RC7  O9
    { 10, &PORTB, 0b01000000 }, // RB6  O10
    { 11, &PORTC, 0b01000000 }, // RC6  O11
    { 12, &PORTB, 0b10000000 }, // RB7  O12
    { 13, &PORTC, 0b00100000 }, // RC5  O13
    { 14, &PORTA, 0b00000001 }, // RA0  O14
    { 15, &PORTC, 0b00010000 }, // RC4  O15
    { 16, &PORTA, 0b00000010 }, // RA1  O16
    { 17, &PORTD, 0b00001000 }, // RD3  O17
    { 18, &PORTA, 0b00000100 }, // RA2  O18
    { 19, &PORTD, 0b00000100 }, // RD2  O19
    { 20, &PORTA, 0b00001000 }, // RA3  O20
    { 21, &PORTD, 0b00000010 }, // RD1  O21
    { 22, &PORTA, 0b00100000 }, // RA5  O22
    { 23, &PORTD, 0b00000001 }, // RD0  O23
    { 24, &PORTE, 0b00000001 }, // RE0  O24
    { 25, &PORTC, 0b00001000 }, // RC3  O25
    { 26, &PORTE, 0b00000010 }, // RE1  O26
    { 27, &PORTC, 0b00000100 }, // RC2  O27
    { 28, &PORTE, 0b00000100 }, // RE2  O28
    { 29, &PORTC, 0b00000010 }, // RC1  O29
    { 30, &PORTC, 0b00000001 }, // RC0  O30
    
    // first floor mappings
    { 129, &PORTD, 0b10000000 }, // RD7  O1
    { 130, &PORTB, 0b00000001 }, // RB0  O2
    { 131, &PORTD, 0b01000000 }, // RD6  O3
    { 132, &PORTB, 0b00000010 }, // RB1  O4
    { 133, &PORTD, 0b00100000 }, // RD5  O5
    { 134, &PORTB, 0b00010000 }, // RB4  O6
    { 135, &PORTD, 0b00010000 }, // RD4  O7
    { 136, &PORTB, 0b00100000 }, // RB5  O8
    { 137, &PORTC, 0b10000000 }, // RC7  O9
    { 138, &PORTB, 0b01000000 }, // RB6  O10
    { 139, &PORTC, 0b01000000 }, // RC6  O11
    { 140, &PORTB, 0b10000000 }, // RB7  O12
    { 141, &PORTC, 0b00100000 }, // RC5  O13
    { 142, &PORTA, 0b00000001 }, // RA0  O14
    { 143, &PORTC, 0b00010000 }, // RC4  O15
    { 144, &PORTA, 0b00000010 }, // RA1  O16
    { 145, &PORTD, 0b00001000 }, // RD3  O17
    { 146, &PORTA, 0b00000100 }, // RA2  O18
    { 147, &PORTD, 0b00000100 }, // RD2  O19
    { 148, &PORTA, 0b00001000 }, // RA3  O20
    { 149, &PORTD, 0b00000010 }, // RD1  O21
    { 150, &PORTA, 0b00100000 }, // RA5  O22
    { 151, &PORTD, 0b00000001 }, // RD0  O23
    { 152, &PORTE, 0b00000001 }, // RE0  O24
    { 153, &PORTC, 0b00001000 }, // RC3  O25
    { 154, &PORTE, 0b00000010 }, // RE1  O26
    { 155, &PORTC, 0b00000100 }, // RC2  O27
    { 156, &PORTE, 0b00000100 }, // RE2  O28
    { 157, &PORTC, 0b00000010 }, // RC1  O29
    { 158, &PORTC, 0b00000001 }  // RC0  O30 
};

/**
 * For a given canID, return a reference to a mapping (port and bit shift)
 * 
 * @param canID the canID received on the wire
 * @return mapping to use to switch on the respective output
 */
const mapping* canIDToPortMapping(byte canID);

#ifdef	__cplusplus
}
#endif

#endif	/* RELAYMAPPINGS_H */

