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
    // 5. 7, 8, 11, 12 should not be used according to CanRelay.X    
    WORK_ROOM         = 1,
    GARAGE            = 2,
    TECHNICAL_ROOM    = 3,
    BATHROOM_DOWN     = 4,
    WC_DOWN           = 6,
    PANTRY            = 9,
    KITCHEN           = 10,
    LIVING_ROOM       = 13,
    LOBBY             = 14,
    HALL_DOWN         = 15,
    GUEST_ROOM        = 16,
    CLEANING_ROOM     = 17,
    
    /** 1st floor */
    // 128 + (5. 7, 8, 11, 12) should not be used according to CanRelay.X    
    CHILD_ROOM_1      = 129,
    CHILD_ROOM_2      = 130,
    CHILD_BATHROOM    = 131,
    CHILD_ROOM_3      = 132,
    CHILD_ROOM_4      = 134,
    HALL_UP           = 137,
    BEDROOM           = 138,
    BEDROOM_BATHROOM  = 141,
    CLOAK_ROOM        = 142
} CanSwitchNode;

typedef enum {
    GROUND = 0,
    FIRST = 0b10000000
} Floor;

/**
 * Operators (CAN data) sent over CAN bus for a given node ID.
 */

#define COMPLEX_OPERATOR_SWITCH 0b10000000
#define COMPLEX_OPERATOR_SET    0b01000000
#define COMPLEX_OPERATOR_CLEAR  0b00100000
#define COMPLEX_OPERATOR_GET    0b00010000

#ifdef	__cplusplus
}
#endif

#endif	/* CANSWITCHES_H */

