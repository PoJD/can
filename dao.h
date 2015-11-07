/* 
 * Wrapper for data storing
 * 
 * File:   dao.h
 * Author: pojd
 *
 * Created on November 7, 2015, 11:48 AM
 */

#ifndef DAO_H
#define	DAO_H

#ifdef	__cplusplus
extern "C" {
#endif

#include "utils.h"

// not initialized EEPROM has all 1s by default, so invalid is the int set to 0x3FFF (2^14) since we only use 14bits for the data itself
#define INVALID_VALUE MAX_14_BITS
    
/**
 * Core type of data.
 */
typedef enum {
    /** id of the node. Note that this way (by passing a config message with NODE_ID) this chip node_id is changed.
         So the same message would not be processed by this node anymore since it won't listen to that traffic anymore */
    NODE_ID = 0,
    /** should the switch be suppressed? (i.e. no CAN message should be sent when this is true?) */
    SUPPRESS_SWITCH = 1,
    /** hearbeat timeout in seconds */
    HEARTBEAT_TIMEOUT = 2
} DataType;

/**
 * Core Data Item structure - wraps some data item (i.e. type and value)
 */
typedef struct {
    DataType dataType; // 2 bits to hold data type
    unsigned int value; // up to 14 bits to hold the data itself (max 16384)
} DataItem;

/**
 * Detects whether the in-passed data item is valid or not
 * @param dataItem data item to check
 * @return true if valid, false otherwise
 */
boolean dao_isValid (DataItem *dataItem);

/**
 * Save data item into the storage.
 * @param data data to store - 16 bits of data received and to be interpreted as Data structure
 * @return the parsed Data instance representing the data
 */
DataItem dao_saveDataItem (unsigned int data);

/**
 * Loads data item from storage for the given dataType
 * 
 * @param dataType dataType to retrieve data item for
 * @return new data item
 */
DataItem dao_loadDataItem(DataType dataType);

#ifdef	__cplusplus
}
#endif

#endif	/* DAO_H */

