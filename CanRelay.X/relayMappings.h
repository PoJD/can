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
#include "canSwitches.h"
    
/**
 * Mapping from canID -> pair of reference to port and bit to change
 */
typedef struct map {
    byte canID; // can ID transmitted over the wire (typically nodeID + x for a given smart wall switch depending on which light switch was pressed in that room)
    volatile byte* port; // reference to one of the port registers of the chip (e.g. PORTA, PORTB, etc)
    byte portBit; // actual bit that is to be set/read in this map
} mapping;

/**
 * For a given canID, return a reference to a mapping (port and bit to change). Note that multiple canIDs can map to the same port and bit,
 * i.e. we can have more light switches connected to one canSwitch that end up switching on the same light even though they are physically
 * connected to different input pins on CanSwitch and thus sending different canIDs over the wire. It just gives enough flexibility
 * routing the wires in the walls so that hopefully no need to reuse the very same ware among more wall switches.
 * 
 * @param floor floor to get the mapping for
 * @param canID the canID received on the wire
 * @return mapping to use to switch on the respective output or NULL if no such mapping found
 */
mapping* canIDToPortMapping (Floor floor, byte canID);

/**
 * Updates the in passed array of byte data with current status of output ports. First byte is always the active switches count. 
 * Remaining 4 bytes are either filled in with real outputs or set as blank depending on the number of outputs
 * 
 * @param floor floor to populate data for
 * @param data data to get populated by this method, always sets 5 bytes
 */
void mapPortsToOutputs(Floor floor, byte* data);

#ifdef	__cplusplus
}
#endif

#endif	/* RELAYMAPPINGS_H */

