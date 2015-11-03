/* 
 * Main CAN interface. Provides methods for initializing the CAN protocol stack as well as sending CAN traffic over.
 * Implemented from PIC18F2XKXX datasheet, so could work for other chips too (assuming xc.h is included in your main file prior to this one)
 * 
 * File:   can.h
 * Author: pojd
 *
 * Created on October 31, 2015, 10:19 PM
 */

#ifndef CAN_H
#define	CAN_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "utils.h"

typedef enum {
    NORMAL   = 0b000, // normal message is a message sent by the node to reveal some action performed
    HEARTBEAT = 0b001, // hearbeat message is also sent by this node, but only triggered by a timer
    CONFIG   = 0b010  // config would typically be sent by some master node in the network to setup this node (so this node would receive this message instead)
} MessageType;
    
/**
 * CanMessage structure - main structure for sending CAN traffic over
 */
typedef struct {
     
     /*
      * The below will become part of the can ID sent as part of this CAN message (custom protocol)
      */

     /** the message ID - 8 bits only in this application, other 3 as part of the final can ID are used to encode other information  */
     byte messageID;
          
     /** type of the message to be sent  */
     MessageType messageType;

     /*
      * The below will become part of the data payload sent as part of this CAN message (custom protocol)
      */
     
     /**
      * Boolean determining whether the switch is on or off. The semantics of this are up to the switch (e.g. on always in the case of lights,
      * while on/off in the case of reed switch)
      * By default the switch is on, e.g. heartbeat message will find out on its own though
      */
     boolean isSwitchOn;
 } CanMessage;

/** Modes of the controller (last parameter in can_init*/ 
#define CONFIG_MODE 0b100
#define LOOPBACK_MODE 0b010
#define NORMAL_MODE 0b000

/**
 * Initialize the CAN stack protocol according to the data sheet, see below:
 * 
 * Initial LAT and TRIS bits for RX and TX CAN.
 * Ensure that the ECAN module is in Configuration mode.
 * Select ECAN Operational mode.
 * Set up the Baud Rate registers.
 * Set up the Filter and Mask registers.
 * Set the ECAN module to normal mode or any
 * other mode required by the application logic.
 * 
 * In normal mode, the CAN module automatically over-
 * rides the appropriate TRIS bit for CANTX. The user
 * must ensure that the appropriate TRIS bit for CANRX
 * is set.

 * @param baudRate the baud rate to use (in kbits per seond). Max is 500 (the implementation uses 16TQ for the bit sequencing)
 * @param cpuSpeed speed of the clock in MHz (mind that PLL settings in registers may affect this)
 * @param mode new mode to use
 */
 void can_init(int baudRate, int cpuSpeed, byte mode);
 
/**
 * Attempts to send the message using TXB0 register (not using any others right now)
 * 
 * @param canID: the can ID (standard, ie 11 bits) of the message to be sent
 * @param data: data to be sent over CAN (takes all 8 bytes)
 */
void can_send(CanMessage *canMessage);

#ifdef	__cplusplus
}
#endif

#endif	/* CAN_H */

