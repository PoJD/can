#include <xc.h>
#include "dao.h"

/**
 * Internal methods
 */

/**
 * Performs a 1 cycle operation (no operation).
 */
void noop() {
    asm("NOP");
}

/**
 * Parse the given 16 bit integer into the instance of dataItem
 * 
 * @param data 16bit integer to parse
 * @return Data instance
 */
DataItem dao_parseData(unsigned int data) {
    // first 2 bits = dataType, the other 14 bits = data itself
    DataType dataType = data >> 14;
    unsigned int intData = MAX_14_BITS & data; // take 14 lowest bits only

    DataItem dataItem;
    dataItem.dataType = dataType;
    dataItem.value = intData;
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
    
    // we have 10bits of data (1024 bytes), it is enough to only use the lower 8 bits though, we only need a few bytes per each dataType
    // each datatype would use 2 bytes of data - so 16bits top (in fact not more than 14bits though)
    EEADRH = 0;
    EEADR = dataType << 1;
}

/**
 * Writes the pre-prepared byte of data out to the storage and waits for the hardware to finish writing it
 */
void dao_writeByte() {
    // initiate write now
    EECON2 = 0x55;
    EECON2 = 0xAA;
    EECON1bits.WR = 1;

    // wait for the write to finish (this bit is cleared by HW)
    while(EECON1bits.WR);
    // clear the interrupt flag (not enabled handling it anyway, must be cleared according to datasheet)
    PIR4bits.EEIF = 0;
}

/**
 * Reads next byte from the storage assuming all the registers were setup properly in order to do so (address, etc)
 */
byte dao_readByte() {
    EECON1bits.RD = 1; // request read bit
    noop(); // wait 1 cycle
    return EEDATA; // and now read the data
}

/*
 * Public API
 */

boolean dao_isValid (DataItem *dataItem) {
    if (dataItem->value == INVALID_VALUE) {
        return FALSE;
    }

    // in addition to that heartbeat and node ID cannot be 0
    if (dataItem->dataType == HEARTBEAT_TIMEOUT || dataItem->dataType == NODE_ID) {
        return dataItem->value ? TRUE : FALSE;
    }
    return TRUE;
}


DataItem dao_saveDataItem (unsigned int data) {
    DataItem dataItem = dao_parseData(data);
    // always disable interrupts during EEPROM write
    di();
    
    dao_setupEeprom(dataItem.dataType);

    // now enable write to EEPROM
    EECON1bits.WREN = 1;
    
    // first write 8 higher bits of the value
    EEDATA = dataItem.value >> 8;
    dao_writeByte();
    
    // now lower 8 bits of the value
    EEADR ++;
    EEDATA = MAX_8_BITS & dataItem.value;
    dao_writeByte();

    // now disable write to EEPROM
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
    
    // now read high 8 bits of data (first two should always be 0 though since we only could store 14bits of data)
    result.value = dao_readByte() << 8;
    
    // now move the address and initiate another read to get the 8 lower bits
    EEADR ++;
    result.value += dao_readByte();
    
    // now enable interrupts again
    ei();

    return result; // return a copy of the instance, not a reference to this local instance
}
