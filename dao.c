#include <xc.h>
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

/**
 * Sets up eeprom read/write. It also sets the address to write/read the data to/from in EEPROM based on the dataType
 * 
 * @param dataType dataType to setup the address for
 */
void dao_setupEeprom(DataType dataType) {
    // write to EEPROM, write only, rest are flags cleared by us this way
    EECON1 = 0;
    
    // we have 10bits of data (1024 bytes), it is enough to only use the lower 6 bits though, we only need 1 byte per each dataType
    EEADRH = 0;
    EEADR = dataType;
}

/**
 * Performs a 1 cycle operation (no operation).
 */
void noop() {
    asm("NOP");
}

/*
 * Public API
 */

boolean dao_isValid (DataItem *dataItem) {
    // this assumes the data will be 0 if unset
    if (dataItem->dataType == HEARTBEAT_TIMEOUT || dataItem->dataType == NODE_ID) {
        return dataItem->value ? TRUE : FALSE;
    }
    return TRUE;
}


DataItem dao_saveDataItem (int data) {
    DataItem dataItem = dao_parseData(data);
    // always disable interrupts during EEPROM write
    di();
    
    dao_setupEeprom(dataItem.dataType);

    // now enable write to EEPROM
    EECON1bits.WREN = 1;
    
    EEDATA = dataItem.value; // prepare data to be written
    
    // initiate write now
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;

    // wait for the write to finish (this bit is cleared by HW)
    while(EECON1bits.WR);
    // clear the interrupt flag (not enabled handling it anyway, must be cleared according to datasheet)
    PIR4bits.EEIF = 0;
    
    // and disable write to EEPROM
    EECON1bits.WREN = 0;
    
    // now enable interrupts again
    ei();

    return dataItem; // return a copy of the instance, not a reference to this local instance
}

DataItem dao_loadDataItem(DataType dataType) {
    DataItem result;
    result.dataType = dataType;
    
    // always disable interrupts during EEPROM read
    di();
    
    dao_setupEeprom(dataType);
    
    // initiate read now
    EECON1bits.RD = 1;

    // wait 1 cycle
    noop();
    // and read the data
    result.value = EEDATA;
    
    // now enable interrupts again
    ei();

    return result; // return a copy of the instance, not a reference to this local instance
}
