
/* 
 * Relay mappings implementation - aka simple array/sort of a map dumb implementation
 * 
 * File:   relayMappings.c
 * Author: pojd
 *
 * Created on October 19, 2018, 0:03 AM
 */

#include "canSwitches.h"
#include "relayMappings.h"

/** 
 * Real mapping to port and bit shifts.
 * 
 * Each line here matches 1 label in output on relay silkscreen. I.e. line 1 matches label 1 on ground floor, line 2 label 2, etc. 
 * It should match can-pcb schematic of pin outputs on the chip. We have the mappings split per floor too
 * 
 * TODO update first value below to match the respective canIDs being sent over the wire once it is all clear (how many switches per room). For now it is is just from 1 upwards
 * TODO e.g. perhaps good way to do that would be to use canSwitches.h defines + x, e.g. living_room+1, etc?
 */

// ground floor mappings
mapping mappingsGround[] = { 
    { 1,  &PORTD, 7 }, // RD7  O1
    { 2,  &PORTB, 0 }, // RB0  O2
    { 3,  &PORTD, 6 }, // RD6  O3
    { 4,  &PORTB, 1 }, // RB1  O4
    { 5,  &PORTD, 5 }, // RD5  O5
    { 6,  &PORTB, 4 }, // RB4  O6
    { 7,  &PORTD, 4 }, // RD4  O7
    { 8,  &PORTB, 5 }, // RB5  O8
    { 9,  &PORTC, 7 }, // RC7  O9
    { 10, &PORTB, 6 }, // RB6  O10
    { 11, &PORTC, 6 }, // RC6  O11
    { 12, &PORTB, 7 }, // RB7  O12
    { 13, &PORTC, 5 }, // RC5  O13
    { 14, &PORTA, 0 }, // RA0  O14
    { 15, &PORTC, 4 }, // RC4  O15
    { 16, &PORTA, 1 }, // RA1  O16
    { 17, &PORTD, 3 }, // RD3  O17
    { 18, &PORTA, 2 }, // RA2  O18
    { 19, &PORTD, 2 }, // RD2  O19
    { 20, &PORTA, 3 }, // RA3  O20
    { 21, &PORTD, 1 }, // RD1  O21
    { 22, &PORTA, 5 }, // RA5  O22
    { 23, &PORTD, 0 }, // RD0  O23
    { 24, &PORTE, 0 }, // RE0  O24
    { 25, &PORTC, 3 }, // RC3  O25
    { 26, &PORTE, 1 }, // RE1  O26
    { 27, &PORTC, 2 }, // RC2  O27
    { 28, &PORTE, 2 }, // RE2  O28
    { 29, &PORTC, 1 }, // RC1  O29
    { 30, &PORTC, 0 }, // RC0  O30
};

// first floor mappings
mapping mappingsFirst[] = { 
    { 129, &PORTD, 7 }, // RD7  O1
    { 130, &PORTB, 0 }, // RB0  O2
    { 131, &PORTD, 6 }, // RD6  O3
    { 132, &PORTB, 1 }, // RB1  O4
    { 133, &PORTD, 5 }, // RD5  O5
    { 134, &PORTB, 4 }, // RB4  O6
    { 135, &PORTD, 4 }, // RD4  O7
    { 136, &PORTB, 5 }, // RB5  O8
    { 137, &PORTC, 7 }, // RC7  O9
    { 138, &PORTB, 6 }, // RB6  O10
    { 139, &PORTC, 6 }, // RC6  O11
    { 140, &PORTB, 7 }, // RB7  O12
    { 141, &PORTC, 5 }, // RC5  O13
    { 142, &PORTA, 0 }, // RA0  O14
    { 143, &PORTC, 4 }, // RC4  O15
    { 144, &PORTA, 1 }, // RA1  O16
    { 145, &PORTD, 3 }, // RD3  O17
    { 146, &PORTA, 2 }, // RA2  O18
    { 147, &PORTD, 2 }, // RD2  O19
    { 148, &PORTA, 3 }, // RA3  O20
    { 149, &PORTD, 1 }, // RD1  O21
    { 150, &PORTA, 5 }, // RA5  O22
    { 151, &PORTD, 0 }, // RD0  O23
    { 152, &PORTE, 0 }, // RE0  O24
    { 153, &PORTC, 3 }, // RC3  O25
    { 154, &PORTE, 1 }, // RE1  O26
    { 155, &PORTC, 2 }, // RC2  O27
    { 156, &PORTE, 2 }, // RE2  O28
    { 157, &PORTC, 1 }, // RC1  O29
    { 158, &PORTC, 0 }  // RC0  O30 
};

byte sizeGround = sizeof(mappingsGround)/sizeof(mappingsGround[0]);
byte sizeFirst = sizeof(mappingsFirst)/sizeof(mappingsFirst[0]);

/*
 * API methods
 */

mapping* canIDToPortMapping(Floor floor, byte canID) {
    mapping *m, *mEnd;
    
    if (floor==GROUND) {
        m = mappingsGround;
        mEnd = mappingsGround + sizeGround;
    } else {
        m = mappingsFirst;
        mEnd = mappingsFirst + sizeFirst;        
    }
    
    for (; m < mEnd; m++) {
        if (m->canID == canID) {
            return m;
        }
    }
    
    return NULL; // should never happen, means we got a canID we do not understand
}

void mapPortsToOutputs(Floor floor, byte* data) {
    mapping *m, *mEnd;
    byte currentBitCount = 0, currentByte = 0;
    
    if (floor==GROUND) {
        m = mappingsGround;
        mEnd = mappingsGround + sizeGround;
        *data++ = sizeGround;
    } else {
        m = mappingsFirst;
        mEnd = mappingsFirst + sizeFirst;        
        *data++ = sizeFirst;
    }

    // regardless of how many switches we actually have, always set next 4 bytes...
    for (int i=0; i<5; i++) {
        data[i] = 0;
    }
    
    for (; m < mEnd; m++) {
        // first find out whether the respective bit in the respective port is set, 
        // then shift it to set the current bit in the current byte starting from the largest bit
        currentByte += ( (*m->port >> m->portBit) & 1 ) << (7-currentBitCount);
        
        if (currentBitCount++ == 7) {
            // ok, now we have the full byte to set, so reset and set the output data byte
            *data++ = currentByte;
            currentByte = 0;
            currentBitCount = 0;
        }
    }
    
    // if something was already added up to the current byte (i.e. we could be in the middle of it), then add it to data too
    if (currentByte) {
        *data++ = currentByte;        
    }
}
