/* 
 * List of nodes to be used by the CanXXX applications. This list is a full list of can switches - 
 * CanSwitch uses that to know what the node is and CanRelay uses that to know what output pin to switch on
 * 
 * File:   canSwitches.h
 * Author: pojd
 *
 * Created on November 18, 2015, 1:53 PM
 */

#ifndef CANSWITCHES_H
#define	CANSWITCHES_H

#ifdef	__cplusplus
extern "C" {
#endif

/**
 * Node - canSwitch. Note that 1st bit should be reserved for the floor (1 = 1st floor, 0 = ground floor).
 * On purpose the values 128 and 0 are omitted (all 0 except from 1st bit).
 * 
 * Each room has up to 8 IDs reserved to it. Since each CanSwitch can have up to 8 input switches connected to it. Can Relay needs to translate the IDs to the respective output pins
 * once it is clear how many lights each of the room would have. That way we have flexibility built into each of the room and no need to change switches once they are flashed and 
 * we can also potentially add additional light switches in the house if needed. 
 * 
 * The comment talks about physical wall switches / number of light circuits for those switches. And as a result how many inputs we need for that switch (e.g. 1I)
 */
typedef enum {
    /** ground floor */
    WORK_ROOM_110         = 0x1,  // 3V/2S 5I
    GARAGE_109            = 0x9,  // 3V/1S 3I
    TECHNICAL_ROOM_108    = 0x11, // 2V/1S 3I
    BATHROOM_DOWN_107     = 0x19, // 2V/2S 3I
    WC_DOWN_107           = 0x21, // 1V/1S 1I
    PANTRY_104            = 0x29, // 1V/1S 1I
    KITCHEN_103           = 0x31, // 2V/7S 8I
    LIVING_ROOM_103       = 0x39, // 2V/6S 8I
    LOBBY_101             = 0x41, // 2V/2S 5I
    LOBBY_CLOAK_ROOM_101  = 0x49, // 1V/1S 1I
    HALL_DOWN_102         = 0x51, // 5V/3S 8I
    GUEST_ROOM_105        = 0x59, // 2V/2S 3I
    CLEANING_ROOM_106     = 0x61, // 2V/1S 3I
    
    /** 1st floor */
    CHILD_ROOM_1_205      = 0x81,
    CHILD_ROOM_2_207      = 0x89,
    CHILD_BATHROOM_206    = 0x91,
    HALL_UP_201           = 0x99,
    BEDROOM_202           = 0xA1,
    BEDROOM_BATHROOM_203  = 0xA9,
    CLOAK_ROOM_204        = 0xB1
} CanSwitchNode;

typedef enum {
    GROUND = 0,
    FIRST = 0b10000000
} Floor;

#ifdef	__cplusplus
}
#endif

#endif	/* CANSWITCHES_H */

