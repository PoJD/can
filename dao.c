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
    // highest 2 bits = dataType, the rest = data itself
    DataType dataType = (0b1100000000000000 & data) >> 14;
    int intData = 0b11111111111111 & data; // drop 2 highest bits only

    DataItem dataItem;
    dataItem.dataType = dataType;
    dataItem.value = intData;
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
