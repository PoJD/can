/* 
 * Relay mappings covers all configuration needed for the relay to properly translate from nodeIDs into respective output pins to get switched on in the CanRelay board.
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
 * Output structure represent the pair of reference to port and bit to change
 * This is a static array inside the firmware to know how to translate from output numbers on the silk screen to individual port bits.
 * 
 * The same for each relay
 */
typedef struct {
    volatile byte* port; // reference to one of the port registers of the chip (e.g. PORTA, PORTB, etc)
    byte portBit; // actual bit that is to be set/read in this map
} Output;

/**
 * UsedOutputs is a very very simple sort of iterator, really just a pointer with the size to allow external world loop through the used outputs of this CanRelay
 */
typedef struct {
    Output* array;
    byte size;
} UsedOutputs;

// how many outputs do physically exist on this CanRelay board?
#define OUTPUTS_COUNT 30


/**
 * Mapping from nodeID -> output, i.e. for a given nodeID it would keep a number of the output to know how to "turn it on or off"
 * This "map" is dynamic, stored in DAO and can be changed dynamically at runtime, e.g. by sending CAN CONFIG messages to the CanRelay.
 * 
 * It is not really a real map, merely an array, i.e. it allows dupes too
 * 
 * Therefore this would be different for different floors
 */
typedef struct {
    byte nodeID; // nodeID to map from (as transmitted over the CAN bus line)
    byte outputNumber; // output number (indexed from 1 to match silkscreen labels)
} Mapping;

/**
 * Mappings is a very very simple sort of iterator, really just a pointer with the size to wrap the "collection"
 */
typedef struct {
    Mapping* array;
    byte size;
} Mappings;


// max size of the dynamic mappings. Use max byte size for this since. We also limit this by the CONFIG data schema, where mapping number is just 1 byte 
#define MAX_MAPPING_SIZE MAX_8_BITS
#define MAPPING_START_DAO_BUCKET 1 // 0 is reserved for floor, so we start from 1
#define UNMMAPED_NODEID MAX_8_BITS // unmapped can ID in the mapping to use as a marker for invalid mapping

/**
 * Operations
 */

/**
 * Initialize mapping using the DAO. This method has to be called in order for the other methods in this header file to work properly!
 */
void initMapping();

/**
 * For a given nodeID, return a reference to output (port and bit to change). Note that multiple nodeIDs can map to the same port and bit,
 * i.e. we can have more light switches connected to one canSwitch that end up switching on the same light even though they are physically
 * connected to different input pins on CanSwitch and thus sending different nodeIDs over the wire. It just gives enough flexibility
 * routing the wires in the walls so that hopefully no need to reuse the very same ware among more wall switches.
 * 
 * @param nodeID the nodeID received on the wire
 * @param time actual time in quarters per second as measured by the caller. This method internally checks whether the internally stored time
 * of last access to the found output (if found) is smaller than the parameter, i.e. effectively checking if at least 250ms already passed. The actual
 * time passed depends on the actual time this method is invoked, but should be in range 250-500ms as a result.
 * @return Output to use to switch on the respective output or NULL if no such mapping found or this method was called too soon after last invocation for the underlying output
 */
Output* nodeIDToOutput (byte nodeID, unsigned long time);

/**
 * Updates the in passed array of byte data with current status of output ports. First byte is always the active switches count. 
 * Remaining 4 bytes are either filled in with real outputs or set as blank depending on the number of outputs
 * 
 * @param data data to get populated by this method, always sets 5 bytes
 */
void retrieveOutputStatus (byte* data);

/**
 * Get UsedOutputs structure so that the caller can iterate through all of them and perform actions against them.
 * Mind that this call returns all outputs irrespective when they were last access. So unlike nodeIDToOutput method, 
 * this method provides no protection from sporadic or very fast calls of operations on the outputs, so the caller
 * needs to assure this is not invoked far too often to cause some fast blinks in the outputs
 * 
 * @return reference to the used outputs (only registered outputs in there)
 */
UsedOutputs* getUsedOutputs();

/**
 * Gets the mappings used at runtime by this CanRelay
 * 
 * @return reference to the internal dynamic "map" from nodeID to outputs
 */
Mappings* getRuntimeMappings();

/**
 * Trigger to update the respective nodeIDMapping. This method would make sure this update is permanent (e.g. storing it into DAO) 
 * and at the same time update any runtime structures holding the data. The caller has to make sure all previous mappings are also set,
 * otherwise this would be ignored
 * 
 * @param mappingNumber
 * @param mappingNodeID
 * @param mappingOutputNumber
 */
void updateMapping (byte mappingNumber, byte mappingNodeID, byte mappingOutputNumber);

#ifdef	__cplusplus
}
#endif

#endif	/* RELAYMAPPINGS_H */

