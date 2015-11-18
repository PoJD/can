/* 
 * File:   main.c
 * Author: pojd
 *
 * Created on November 18, 2015, 2:18 PM
 */

#include "canSwitches.h"
#include "dao.h"

/*
 * Sets up EEPROM of this node as a canSwitch - using the node passed in to be set as NODE_ID for CanSwitch project
 */
void setupCanSwitch(CanSwitchNode node) {
    DataItem dataItem;
    dataItem.dataType = NODE_ID;
    dataItem.value = node;
    dao_saveDataItem(&dataItem);
}
/*
 * Sets up EEPROM of this node as a canRelay - using the floor passed in to be set as FLOOR for CanRelay project
 */
void setupCanRelay(Floor floor) {
    DataItem dataItem;
    dataItem.dataType = FLOOR;
    dataItem.value = floor;
    dao_saveDataItem(&dataItem);
}

int main(void) {
    // for example setup as CanSwitch for Garage
    setupCanSwitch(GARAGE);
    
    // or setup as CanRelay for 1st floor
    setupCanRelay(FIRST);
    return 0;
}

