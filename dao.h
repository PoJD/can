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
    
/**
 * Core type of data.
 */
typedef enum {
    /** should the switch be suppressed? (i.e. no CAN message should be sent when this is true?) */
    SUPPRESS_SWITCH = 0,
    /** hearbeat timeout in seconds */
    HEARTBEAT_TIMEOUT = 1,
    /** id of the node. Note that this way (by passing a config message with NODE_ID) this chip node_id is changed.
         So the same message would not be processed by this node anymore since it won't listen to that traffic anymore */
    NODE_ID = 2
} DataType;

/**
 * Core Data Item structure - wraps some data item (i.e. type and value)
 */
typedef struct {
    DataType dataType;
    byte value; // up to 8 bits to hold the data itself
} DataItem;

/**
 * Save data item into the storage.
 * @param data data to store - 16 bits of data received and to be interpreted as Data structure
 * @return the parsed Data instance representing the data
 */
DataItem dao_saveDataItem (int data);

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

