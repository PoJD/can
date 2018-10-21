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
 * we can also potentially add additional light switches in the house if needed. */
typedef enum {
    /** ground floor */
    WORK_ROOM_110         = 1,
    GARAGE_109            = 9,
    TECHNICAL_ROOM_108    = 17,
    BATHROOM_DOWN_107     = 25,
    PANTRY_104            = 33,
    KITCHEN_103           = 41,
    LIVING_ROOM_103       = 49,
    LOBBY_101             = 57,
    HALL_DOWN_102         = 65,
    GUEST_ROOM_105        = 73,
    CLEANING_ROOM_106     = 81,
    
    /** 1st floor */
    CHILD_ROOM_1_205      = 129,
    CHILD_ROOM_2_207      = 137,
    CHILD_BATHROOM_206    = 145,
    HALL_UP_201           = 153,
    BEDROOM_202           = 161,
    BEDROOM_BATHROOM_203  = 169,
    CLOAK_ROOM_204        = 177
} CanSwitchNode;

typedef enum {
    GROUND = 0,
    FIRST = 0b10000000
} Floor;

#ifdef	__cplusplus
}
#endif

#endif	/* CANSWITCHES_H */

