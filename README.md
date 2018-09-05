# CAN

This project consists of a couple of MPLAB (@Microchip) projects using my custom piclib library (https://github.com/PoJD/piclib). These are primarily focused on automating my home using CAN as the underlying technology and Microchip PIC microcontrollers as the barebone chips on the network.
Everything was implemented using microcontroller PIC18F25K80, but assuming Microchip is consistent in register names, etc, I assume other 8 bit controllers with CAN support should work too.

## Technologies

* Microchip XC8 compiler

## Getting started in MPLAB

* git clone git@github.com:PoJD/can.git
* Now import any of the projects in the top directory to MPLAB

## Projects

### CanSwitch.X
This project is the can switch implementation. The plan is to program the wall switches for lights with this project. 
Key features:

* It does simply send a CAN message upon an input change (high to low) on PORTB5. The CAN ID is read from EEPROM. The data contains just the switch state (so 0 all the time)
* It does send regular heartbeat messages (5mins interval by default) with the state of the port B (most likely high then). Together with this information, it also reads the error counts from CAN controller (receive and send error count)
* It also supports CONFIG messages being sent to the node to update nodeID or heartbeat timeout or a flag to suppress the switch
* Uses port RC1 as a "status port"
 * It blinks shortly (1/4 of a second) after a button is pressed to indicate that works
 * Followed by either 1sec long blink to indicate CAN message sent OK or 4 short blinks in a sec to indicate a CAN error

### CanSetup.X
This project serves to setup either CanSwitch or CanRealy chips - write the necessary data to EEPROM based on the node.
Key features:

* invoke setupCanSwitch (nodeId) to setup EEPROM of the chip as the CanSwitch
* invoke setupCanRelay (bit floor) to setup EEPROM of the chip as the CanRelay

### CanRelay.X
This project is the can relay implementation. The chips programmed this way are planned to be installed on a PCB directly controlling the relays for lights.
Key features:

* Registers for all NORMAL and COMPLEX messages for a given floor (the floor has to be setup in EEPROM first)
* For a given message, it would ignore the first bit and treat the rest as the counter of port to switch on.

#### Mapping of ports
* 1-8 toggles PORTA0..PORTA7 (PORTA4 does not exist though, so should not be used = 5, RA7 and RA6 are used by crystal, so should not be used either = 7 and 8)
* 9-16 toggles PORTB0..PORTB7 (PORTB3 and PORTB2 are used by CANRX and CANTX though, so should not be used = 11 and 12)
* 17-24 toggles PORTC0..PORTC7

## Communication Protocol
Custom communication protocol was established, inspired partially in VSCP

CAN ID
* Extended CAN identifiers not used, so only 11 bits used for standard CAN ID
* First 3 bits are reserved for message type: https://github.com/PoJD/piclib/blob/master/can.h
* Remaining 8 bits are reserved for internal nodeID: https://github.com/PoJD/can/blob/master/CanSetup.X/canSwitches.h
* Node id 1st bit is always the floor, so effectively 7 bits remaining for each floor (which should be sufficient)

CAN Data
* NORMAL (0) and COMPLEX (3) message types (aka action message types)
 * bit 7 = 1 => toggles the respective nodeID (assuming other bits are 0)
 * bit 6 = 1 => switches the respective nodeID on  (assuming other bits are 0)
 * bit 5 = 1 => switches the respective nodeID off (assuming other bits are 0)
 * bit 4 = 1 => requesting the state of the node (assuming other bits are 0). After such a message the canSwitch node would reply with another CAN message (type complex_reply). Will always send the state of all nodeIDs as currently set on the respective floor's nodeID. (i.e. for any nodeID in the given can relay's range a reply would be sent with all data. E.g. for 0-127 all data for nodes on floor 0, for 128-255, all data on floor 1)
 * This model in general could lead to CAN conflicts - 2 nodes trying to do 2 different actions for the same node ID and message type. Therefore it is mandated, that if 2 distinct nodes need to change settings of a particular node, the message type has to differ (for example web app running on odroid will always send COMPLEX messages while canSwitch would always send NORMAL messages - so the can switch has precedence since it has lower CAN ID and never results in conflicting state of the CAN bus - e.g. the same CAN ID, but different data)
 * This supports also nodeID 0 - with 1 st bit of nodeID equal to floor. Then the action is applied to all nodes - i.e. switch on or off or toggle all nodes on that floor
* COMPLEX REPLY (4)
 * Reply to a complex message with bit 4 = 1
 * Contains all data of all PORTS in CAN data. E.g. byte 1 = PORTA, byte 2 = PORTB and byte 3 = PORTC; together with CAN bus error counts. E.g. byte 4 = TXERRCNT (transmit error count). byte 5 = RXERRCNT (receive error count)
* CONFIG (2)
 * allows canSwitches to be configured remotely over CAN bus, so only the respective CAN node would process this message
 * 2 bytes of data CAN sent in this case
 * highest 2 bits = datatype ( https://github.com/PoJD/piclib/blob/master/dao.h ), remaining 14 bits = value of this datatype
 * The respective node stores these new values into EEPROM and updates the runtime variables to act accordingly (allows changing nodeID, heartbeat timeout and suppressing the switch functionality for CanSwitch or changing the floor for CanRelay - the latter not yet implemented, may never be to avoid changing the floor's relays. Still used to store the data in CanRelay EEPROM though)
* HEARTBEAT
 * Only sent by the CanSwitches
 * Similar as NORMAL, but only with bit 7 always equal to 1 (i.e. the switch was pressed). In addition to that, this also sets 2 more bytes in can data. Byte 2 = TXERRCNT (transmit error count). byte 3 = RXERRCNT (receive error count)

### Examples
See below examples as they can be used with the cansend utility (http://elinux.org/Can-utils). So you can invoke e.g. cansend can0 XXX, where XXX is in the below table
* 202#80.04 - changes heartbeat for node 2 to 4 seconds
* 202#81.2C - changes heartbeat for node 2 to 300 seconds (5 minutues = default)
* 202#C0.01 - changes debug mode for node 2 to 1 (i.e. true, i.e. sets up debug mode)
* 202#40.01 - switches on suppress switch on node 2 (pressing the switch on that node after this action will have no effect - no CAN message would get sent)
* 202#00.03 - changes nodeID for node 2 to nodeID 3 (so after this, the same message would not get processed by the same node again anymore since the nodeID changed)
* 011#80    - toggles the switch on node 11
Complex message types below
* 311#80 - toggles the switch of the node 11 (hex, so 17 in dec)
* 311#40 - switches on the node 11
* 311#20 - switches off the node 11
* 300#10 - complex get - detect the state of all switches on floor 0 - a complex reply will come
 * for example: 400#00.0C.01.00.00 (1st 3 bytes are states of the ports with switches, last 2 bytes are the CAN bus error counts). Can ID has no impact here other than the range - see above (e.g. 300 to 427 in dec is for floor 0, for larger can IDs the 1st floor CanRelay would reply instead)
* 300#40 - switches on all lights on floor 0
* 300#20 - switches off all lights on floor 0

## Setting up CAN on Odroid
### Autoload module on odroid
* Edit /etc/modules-load.d/can.conf
* Add the below contents there
```    
can 
can-raw 
slcan 
```

### Start can on odroid:
* Compile SocketCan on linux - http://elinux.org/Can-utils 
* Start can using the above: (s2 below is 50kbps)

```bash
#!/bin/bash 

set -x 

slcan_attach -f -s2 -o /dev/ttyACM0 -n can0 
slcand ttyACM0 can0 
ifconfig can0 up
```
