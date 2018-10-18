
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

const mapping* canIDToPortMapping(byte canID) {
    const mapping* m = mappings;
    const mapping* mEnd = m + sizeof(mappings)/sizeof(mappings[0]);
    for (; m < mEnd; m++) {
        if (m->canID == canID) {
            return m;
        }
    }
    
    return 0; // should never happen, means we got a nodeID we do not understand
}