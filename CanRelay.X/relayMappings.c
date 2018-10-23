
/* 
 * Relay mappings implementation - aka simple array/sort of a map dumb implementation
 * 
 * File:   relayMappings.c
 * Author: pojd
 *
 * Created on October 19, 2018, 0:03 AM
 */

#include "canSwitches.h"
#include "dao.h"
#include "relayMappings.h"

/** 
 * Static output array to list all port and bit shifts.
 * 
 * Each line/array item here matches 1 label in output on relay silkscreen. I.e. line 1 matches label 1 on ground floor, line 2 label 2, etc. 
 * Strictly speaking when accessing, for output x, index x-1 should be used since as usual, arrays are indexed from 0
 * 
 * It should match can-pcb schematic of pin outputs on the chip. See https://github.com/PoJD/can-pcb/tree/master/CanRelay for details
 */
Output outputs[OUTPUTS_COUNT] = { 
    { &PORTD, 7 }, // RD7  O1
    { &PORTB, 0 }, // RB0  O2
    { &PORTD, 6 }, // RD6  O3
    { &PORTB, 1 }, // RB1  O4
    { &PORTD, 5 }, // RD5  O5
    { &PORTB, 4 }, // RB4  O6
    { &PORTD, 4 }, // RD4  O7
    { &PORTB, 5 }, // RB5  O8
    { &PORTC, 7 }, // RC7  O9
    { &PORTB, 6 }, // RB6  O10
    { &PORTC, 6 }, // RC6  O11
    { &PORTB, 7 }, // RB7  O12
    { &PORTC, 5 }, // RC5  O13
    { &PORTA, 0 }, // RA0  O14
    { &PORTC, 4 }, // RC4  O15
    { &PORTA, 1 }, // RA1  O16
    { &PORTD, 3 }, // RD3  O17
    { &PORTA, 2 }, // RA2  O18
    { &PORTD, 2 }, // RD2  O19
    { &PORTA, 3 }, // RA3  O20
    { &PORTD, 1 }, // RD1  O21
    { &PORTA, 5 }, // RA5  O22
    { &PORTD, 0 }, // RD0  O23
    { &PORTE, 0 }, // RE0  O24
    { &PORTC, 3 }, // RC3  O25
    { &PORTE, 1 }, // RE1  O26
    { &PORTC, 2 }, // RC2  O27
    { &PORTE, 2 }, // RE2  O28
    { &PORTC, 1 }, // RC1  O29
    { &PORTC, 0 }, // RC0  O30
};

/**
 * Dynamic "map" of all mappings from canID to outputs. Initially loaded using DAO, at runtime can be changed
 */
Mapping mappings[MAX_MAPPING_SIZE];
byte mappingsSize = 0;

/*
 * API methods
 */

void initMapping () {
    mappingsSize = 0;
    
    // loop through all potential mappings now, skip when finding first not set
    // i.e. therefore if someone attempts to update mapping that has a gap before it (previous mapping not set), it would be ignored
    int bucket = 0;
    for (; bucket < MAX_MAPPING_SIZE; bucket++) {
        DataItem dataItem = dao_loadDataItem(bucket+MAPPING_START_DAO_BUCKET);
        if (!dao_isValid(&dataItem)) {
            break;
        }
        
        // higher 8 bits is canID, lower 8 bits is output number index starting from 1
        // just to be super sure somehow we did not get invalid outputIndex into DAO, rather check output index value read
        // since through the API method calls we have this covered (see updateMapping method below)
        // this time we do not break the loop, just continue on to the next one and set some dummy value for canID to make sure it is never triggered

        byte canID = (dataItem.value >> 8) & MAX_8_BITS;
        byte outputIndex = dataItem.value & MAX_8_BITS;
        if (outputIndex<=0 || outputIndex>OUTPUTS_COUNT) {
            canID = UNMMAPED_CANID; // should never be used as real mapping
            outputIndex = OUTPUTS_COUNT; // just to make sure we have valid outputs at least in the mappings
        }
        
        mappings[bucket].canID = canID;
        // we always store the number in DAO from 1 to also allow CONFIG messages to use real numbers from 1 matching the silkscreen of the PCB
        mappings[bucket].output = &outputs[outputIndex-1];
    }

    mappingsSize = bucket;
}

Output* canIDToOutput (byte canID) {
    if (canID==UNMMAPED_CANID) {
        return NULL;
    }
    
    Mapping *m = mappings;
    Mapping *mEnd = mappings + mappingsSize;
    
    for (; m < mEnd; m++) {
        if (m->canID == canID) {
            return m->output;
        }
    }
    
    return NULL; // should never happen, means we got a canID we do not understand
}

void retrieveOutputStatus(byte* data) {
    byte currentBitCount = 0, currentByte = 0;
    
    Mapping *m = mappings;
    Mapping *mEnd = mappings + mappingsSize;
    
    // first byte is always the size of real mappings
    *data++ = mappingsSize;

    // regardless of how many switches we actually have, always set next 4 bytes...
    for (int i=0; i<5; i++) {
        data[i] = 0;
    }
    
    for (; m < mEnd; m++) {
        // first find out whether the respective bit in the respective port is set, 
        // then shift it to set the current bit in the current byte starting from the largest bit
        currentByte += ( (*m->output->port >> m->output->portBit) & 1 ) << (7-currentBitCount);
        
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

void updateMapping (byte mappingNumber, byte canID, byte outputNumber) {
    // majority of checks done by the datatypes themselves, so only check the output number is in range 1.OUTPUTS_COUNT as that is real mapping size of relay
    // and also check that mapping number is in range 1 .. max number of mappings
    // we do not care if we received UNMMAPED_CANID canID here since that could also potentially be stored already in DAO by someone, so we just need to check
    // that in the canIDToOutput method
    if (outputNumber>0 && outputNumber<=OUTPUTS_COUNT && mappingNumber>0) {
        DataItem dataItem;

        // mappingNumber shall be a sequence of numbers 1..MAX, so use it as the bucket number to store it into using the DAO
        dataItem.bucket = mappingNumber-1 + MAPPING_START_DAO_BUCKET;
        // value would be canID the higher 8 bits and outputNumber the lower 8 bits
        dataItem.value = (canID << 8) + outputNumber;

        // store new value into DAO and update runtime mappings array
        dao_saveDataItem(&dataItem);
        
        mappings[mappingNumber-1].canID = canID;
        mappings[mappingNumber-1].output = &outputs[outputNumber-1];
    }
}
