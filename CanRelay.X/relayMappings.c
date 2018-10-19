
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

/*
 * Private methods
 */

const mapping* getMappingForFloor (Floor floor) {
    return floor==GROUND ? mappingsGround : mappingsFirst;
}

byte sizeOfMapping (const mapping* m) {
    return sizeof(m)/sizeof(m[0]);
}

/*
 * API methods
 */

byte activeSwitchesCount (Floor floor) {
    const mapping* m = getMappingForFloor(floor);
    return sizeOfMapping(m);
}

const mapping* canIDToPortMapping(Floor floor, byte canID) {
    const mapping* m = getMappingForFloor(floor);
    const mapping* mEnd = m + sizeOfMapping(m);
    for (; m < mEnd; m++) {
        if (m->canID == canID) {
            return m;
        }
    }
    
    return NULL; // should never happen, means we got a canID we do not understand
}