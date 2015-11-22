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
 * On purpose the values 128 and 0 are omitted (all 0 except from 1st bit)
 */
typedef enum {
    /** ground floor */
    WORK_ROOM         = 1,
    GARAGE            = 2,
    TECHNICAL_ROOM    = 3,
    BATHROOM_DOWN     = 4,
    WC_DOWN           = 5,
    PANTRY            = 6,
    KITCHEN           = 7,
    LIVING_ROOM       = 8,
    LOBBY             = 9,
    HALL_DOWN         = 10,
    GUEST_ROOM        = 13, // 11 and 12 should not be used according to CanRelay.X
    CLEANING_ROOM     = 14,
    
    /** 1st floor */
    CHILD_ROOM_1      = 129,
    CHILD_ROOM_2      = 130,
    CHILD_BATHROOM    = 131,
    CHILD_ROOM_3      = 132,
    CHILD_ROOM_4      = 133,
    HALL_UP           = 134,
    BEDROOM           = 135,
    BEDROOM_BATHROOM  = 136,
    CLOAK_ROOM        = 137
} CanSwitchNode;

typedef enum {
    GROUND = 0,
    FIRST = 0b10000000
} Floor;

#ifdef	__cplusplus
}
#endif

#endif	/* CANSWITCHES_H */

