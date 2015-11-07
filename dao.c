#include "dao.h"

/**
 * Internal methods
 */

/**
 * Parse the given 16 bit integer into the instance of dataItem
 * 
 * @param data 16bit integer to parse
 * @return Data instance
 */
DataItem dao_parseData(int data) {
    // first 8 bits = dataType, the other 8 bits = data itself
    DataType dataType = data >> 8;
    byte byteData = 0b11111111 & data; // take 8 lowest bits only

    DataItem dataItem;
    dataItem.dataType = dataType;
    dataItem.value = byteData;
    return dataItem; // return copy, not a reference
}

/*
 * Public API
 */

DataItem dao_saveDataItem (int data) {
    DataItem dataItem = dao_parseData(data);
    // TODO implement
    
    return dataItem; // return a copy of the instance, not a reference to this local instance
}

DataItem dao_loadDataItem(DataType dataType) {
    DataItem result;
    // TODO implement

    return result; // return a copy of the instance, not a reference to this local instance
}
