
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
 * Here in the internal size we simply keep the maximum number of output used so far by any mapping (1 based)
 * This way we keep track of all outputs used and it is assumed that all smaller numbered outputs are used as well (to avoid extra logic looping through all used all the time when matching)
 * 0 on startup and gets updated on initial load or during runtime too.
 */
UsedOutputs usedOutputs = { outputs, 0 };

/**
 * Dynamic "map" of all mappings from nodeID to outputs. Initially loaded using DAO, at runtime can be changed
 */
Mapping mappingsArray[MAX_MAPPING_SIZE];
Mappings mappings = { mappingsArray, 0 };

/** 
 * Simple array where each index is reserved for output and value is the last time a non-GET operation was invoked for that output.
 * This way we implement sort of a switch debouncer inside CanRelay and protect against sporadic CAN messages from CanSwitches.
 * It is more generic - i.e. any operation other than GET for a given output is always tracked. So even if someone tries to click very 
 * quickly perhaps via Apple HomeKit, it would be ignored if done too quickly (faster than 250-500ms).
 * We do not use mappings for this in order to properly "protect" outputs directly since multiple nodeIDs in mapping can be mapped to 1 output
 * 
 * The actual time stored is in quarters per second and no need to worry about overflow - it is about 34 years and unlikely 
 * there would be any power outage within 34 years :)
 * 
 * Only real used outputs are actually set and used, the rest is ignored.
 */
unsigned long outputsLastAccessTime[OUTPUTS_COUNT];

/*
 * Private methods
 */

void setLastAccessTime(byte outputNumber, unsigned long time) {
    outputsLastAccessTime[outputNumber-1] = time;
}

unsigned long getLastAccessTime(byte outputNumber) {
    return outputsLastAccessTime[outputNumber-1];
}

void updateUsedOutputs(byte outputNumber) {
    // outputNumber is 1 based, so we can take directly as the size
    if (outputNumber > usedOutputs.size) {
        usedOutputs.size = outputNumber;
    }
    // and also update last time for this output to be 0 - this is first time we are enabling it (either on startup or at runtime), so first invocation will reset this time 
    // to the real time and protection against sporadic access would kick in on subsequent operations
    setLastAccessTime(outputNumber, 0);    
}

void updateMappingCache(byte mappingNumber, byte nodeID, byte outputNumber) {
    mappings.array[mappingNumber-1].nodeID = nodeID;
    mappings.array[mappingNumber-1].outputNumber = outputNumber;

    if (mappingNumber > mappings.size) {
        mappings.size = mappingNumber;
    }
}


/*
 * API methods
 */

UsedOutputs* getUsedOutputs() {
    return &usedOutputs;
}

Mappings* getRuntimeMappings() {
    return &mappings;
}

void initMapping () {
    mappings.size = 0;
    usedOutputs.size = 0;
    
    // loop through all potential mappings now, skip when finding first not set
    // i.e. therefore if someone attempts to update mapping that has a gap before it (previous mapping not set), it would be ignored
    int bucket = 1;
    for (; bucket <= MAX_MAPPING_SIZE; bucket++) {
        DataItem dataItem = dao_loadDataItem(bucket-1 + MAPPING_START_DAO_BUCKET);
        if (!dao_isValid(&dataItem)) {
            break;
        }
        
        // higher 8 bits is nodeID, lower 8 bits is output number starting from 1
        // just to be super sure somehow we did not get invalid outputIndex into DAO, rather check output index value read
        // since through the API method calls we have this covered (see updateMapping method below)
        // this time we do not break the loop, just continue on to the next one and set some dummy value for nodeID to make sure it is never triggered

        byte nodeID = (dataItem.value >> 8) & MAX_8_BITS;
        byte outputNumber = dataItem.value & MAX_8_BITS;
        if (outputNumber<=0 || outputNumber>OUTPUTS_COUNT) {
            nodeID = UNMMAPED_NODEID; // should never be used as real mapping
            outputNumber = OUTPUTS_COUNT; // just to make sure we have valid outputs at least in the mappings
        } else {
            // for valid mappings only, update the used outputs...
            updateUsedOutputs(outputNumber);
        }
        // we always store the number in DAO from 1 to also allow CONFIG messages to use real numbers from 1 matching the silkscreen of the PCB
        updateMappingCache(bucket, nodeID, outputNumber);
    }
}

Output* nodeIDToOutput (byte nodeID, unsigned long time) {
    if (nodeID==UNMMAPED_NODEID) {
        return NULL;
    }    
    
    Mapping *m = mappings.array;
    Mapping *mEnd = mappings.array + mappings.size;
    
    for (; m < mEnd; m++) {
        if (m->nodeID == nodeID) {
            // now check the last time this output was used and if interval not passed yet, return NULL too
            // we count in quarters per second, so make sure at least 1 quarter already passed
            if (time > getLastAccessTime(m->outputNumber) + 1) {
                // if all OK, just update the last access time and return the respective output
                setLastAccessTime(m->outputNumber, time);
                return &outputs[m->outputNumber-1];
            } else {
                return NULL;
            }
        }
    }
    
    return NULL; // should never happen, means we got a nodeID we do not understand
}

void retrieveOutputStatus(byte* data) {
    byte currentBitCount = 0, currentByte = 0;
    
    Output *o = usedOutputs.array;
    Output *oEnd = usedOutputs.array + usedOutputs.size;
    
    // first byte is always the size of used outputs
    *data++ = usedOutputs.size;

    // regardless of how many outputs we actually use, always set next 4 bytes...
    for (int i=0; i<5; i++) {
        data[i] = 0;
    }
    
    for (; o < oEnd; o++) {
        // first find out whether the respective bit in the respective port is set, 
        // then shift it to set the current bit in the current byte starting from the largest bit
        currentByte += ( (*o->port >> o->portBit) & 1 ) << (7-currentBitCount);
        
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

void updateMapping (byte mappingNumber, byte nodeID, byte outputNumber) {
    // majority of checks done by the datatypes themselves, so only check the output number is in range 1.OUTPUTS_COUNT as that is real mapping size of relay
    // and also check that mapping number is in range 1 .. max number of mappings
    // we do not care if we received UNMMAPED_NODEID nodeID here since that could also potentially be stored already in DAO by someone, so we just need to check
    // that in the nodeIDToOutput method
    
    // also allow MAX_8_BITS outputnumber and nodeID that would effectively "erase" that mapping in DAO (use the same as default values)
    if (mappingNumber>0 && ( (outputNumber>0 && outputNumber<=OUTPUTS_COUNT) || (outputNumber==MAX_8_BITS && nodeID == MAX_8_BITS) )) {
        DataItem dataItem;

        // mappingNumber shall be a sequence of numbers 1..MAX, so use it as the bucket number to store it into using the DAO
        dataItem.bucket = mappingNumber-1 + MAPPING_START_DAO_BUCKET;
        // value would be nodeID the higher 8 bits and outputNumber the lower 8 bits
        dataItem.value = (nodeID << 8) + outputNumber;

        // store new value into DAO 
        dao_saveDataItem(&dataItem);
    }
    
    if (outputNumber>0 && outputNumber<=OUTPUTS_COUNT) {
        // now also update runtime status of used outputs and mappings
        updateMappingCache(mappingNumber, nodeID, outputNumber);
        updateUsedOutputs(outputNumber);
    }
}
