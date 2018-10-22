# CAN

This project consists of a couple of MPLAB (@Microchip) projects using my custom piclib library (https://github.com/PoJD/piclib). These are primarily focused on automating my home using CAN as the underlying technology and Microchip PIC microcontrollers as the barebone chips on the network.
Everything was originally implemented using microcontroller PIC18F25K80, but assuming Microchip is consistent in register names, etc, I assume other 8 bit controllers with CAN support should work too.

As of firmware version 2 of CanRelay, CanRelay actually uses CPU PIC18F45K80 (44 pin TFQP) as opposed to CanSwitch with PIC18F25K80. The reason being more output ports on CanRelay. 

## Technologies

* Microchip XC8 compiler

## Getting started in MPLAB

* git clone git@github.com:PoJD/can.git
* Now import any of the projects in the top directory to MPLAB

## Projects

### CanSwitch.X
This project is the can switch implementation. The plan is to program the wall switches for lights with this project. 
Key features:

* It does simply send a CAN message upon an input change (high to low) on PORTB0..PORTB7. 
* The node ID is read from EEPROM. The message type for these messages is NORMAL (binary 0b000) and so the nodeID effectively becomes the canID being transmitted
* The data contains just one byte by default that consists of 2bits operation (0b00 = TOGGLE), 1 bit for any CAN registry ERROR, 2 bits for firmware and last 3 bits for switch counter (in non DEBUG mode the only way to see roughly if the wall switch is not bouncing itself after each switch)
* The switch now supports wiring up to 8 real wall switches to minimize the need to have PCB in each wall switch. This switch can then send CAN message for any of the 8 B port input changes to logical zero. The switch then sends CAN ID of nodeID + PORTB pin number, e.g. nodeID directly for change on B0, nodeID+1 for change on B1, etc
* As long as the wall switch count does not exceed 8, each can be tight up to individual input pin even if it 2 or more physical light switches should be switching on 1 output pin on CanRelay. Just the respective mapping has to be set properly in https://github.com/PoJD/can/blob/master/CanRelay.X/relayMappings.c (see mapping of ports section in CanRelay)
* DEBUG mode
    * Disabled by default and needs to be enabled in firmware (defined constant in firmware file)
    * non DEBUG mode (default) heavily utilizes power savings, putting the chip to sleep using low current drawning specs to minimize power consumption, disabling the crystal, whole chip and even the transceiver, only waken up by input interrupt. Current drawn in sleep measured as low as 1.8uA
* Etra features in DEBUG mode
    * supports CONFIG messages being sent to the node to update nodeID or heartbeat timeout or a flag to suppress the switch (it needs to be up in order to receive any CAN traffic, thus this only works in DEBUG mode)
    * timer using the external crystal (crystal by default used only to get clocks for sending the CAN message)
    * Thanks to timer the logic can actually measure the time and as a result the following new features are added
    * Sending regular heartbeat messages (10sec interval by default). In addition to the first default byte which is sent normally upon switch it also reads the error counts from CAN controller (receive and send error count), firmware version and time since start and uploads all these as separate bytes to the output
    * Uses port RC1 as a "status port"
        * It blinks shortly (1/4 of a second) after a button is pressed to indicate that works
        * Followed by either 1sec long blink to indicate CAN message sent OK or 4 short blinks in 2 secs to indicate a CAN error

### CanSetup.X
This project serves to setup either CanSwitch or CanRealy chips - write the necessary data to EEPROM based on the node.
Key features:

* invoke setupCanSwitch (nodeId) to setup EEPROM of the chip as the CanSwitch
* invoke setupCanRelay (bit floor) to setup EEPROM of the chip as the CanRelay

Remember to set the right processor family! Since version 2 of firmware for CanRelay, different device family is used for relays only (PIC18F45K80, TQFP package)

### CanRelay.X
This project is the can relay implementation. The chips programmed this way are planned to be installed on a PCB directly controlling the relays for lights.
Key features:

* Registers for all NORMAL and COMPLEX messages for a given floor (the floor has to be setup in EEPROM first)
* For a given message, it would take the canID and translate it using relayMappings.h to actual port and bit to change (to assure labels on the relay do match lines in the relayMappings file)

#### Mapping of ports
* See https://github.com/PoJD/can/blob/master/CanRelay.X/relayMappings.c for details
* Each line in the respective array there represents 1 label on the relay starting from label 1

## Communication Protocol
Custom communication protocol was established, inspired partially in VSCP

CAN ID
* Extended CAN identifiers not used, so only 11 bits used for standard CAN ID
* First 3 bits are reserved for message type: https://github.com/PoJD/piclib/blob/master/can.h
* Remaining 8 bits are reserved for internal nodeID: https://github.com/PoJD/can/blob/master/CanSetup.X/canSwitches.h
* Node id 1st bit is always the floor, so effectively 7 bits remaining for each floor (which should be sufficient). https://github.com/PoJD/can/blob/master/CanSetup.X/canSwitches.h lists all CAN IDs for all switches. Each has 8 IDs reserved since in theory we can wire up to 8 individual wall switches to one CanSwitch

CAN Data
* NORMAL (0) and COMPLEX (3) message types (aka action message types)
    * bit 8 and 7 combine the operation (00 togle, 01 ON, 10 OFF, 11 GET). For any message with operation GET the canRelay node would reply with another CAN message (type complex_reply). Will always send the state of all nodeIDs as currently set on the respective floor. (i.e. for any nodeID in the given can relay's range a reply would be sent with all data. E.g. for 0-127 all data for nodes on floor 0, for 128-255, all data on floor 1)
    * This model in general could lead to CAN conflicts - 2 nodes trying to do 2 different actions for the same node ID and message type. Therefore it is mandated, that if 2 distinct nodes need to change settings of a particular node, the message type has to differ (for example web app running on odroid will always send COMPLEX messages while canSwitch would always send NORMAL messages - so the can switch has precedence since it has lower CAN ID and never results in conflicting state of the CAN bus - e.g. the same node ID, but different message type)
    * This supports also nodeID 0 - with 1 st bit of nodeID equal to floor. Then the action is applied to all nodes - i.e. toggle/on/off all nodes on that floor
* COMPLEX REPLY (4)
    * Reply to a complex message with operation GET
    * Sets the canID = floor (e.g. when both relays are connected, we can distinguish this way)
    * Contains all data of all outputs as defined in https://github.com/PoJD/can/blob/master/CanRelay.X/relayMappings.c
    * byte 1: actual output size (how many outputs are there defined in relayMappings.c (should match number of outputs connected to the relay)
    * byte 2-5: all outputs regardless the actual size
    * byte 6 and 7: CAN bus error counts. TXERRCNT (transmit error count) and RXERRCNT (receive error count)
    * byte 8: firmware version
* CONFIG (2)
    * allows canSwitches to be configured remotely over CAN bus, so only the respective CAN node would process this message
    * 2 bytes of data CAN sent in this case
    * highest 2 bits = datatype ( https://github.com/PoJD/piclib/blob/master/dao.h ), remaining 14 bits = value of this datatype
    * The respective node stores these new values into EEPROM and updates the runtime variables to act accordingly (allows changing nodeID, heartbeat timeout and suppressing the switch functionality for CanSwitch.
    * Means this is only supported in DEBUG mode for CanSwitches
* HEARTBEAT (1)
    * Only sent by the CanSwitches
    * Similar as NORMAL. In addition to the 1 byte sent in NORMAL message, this also sets 4 more bytes in CAN data as per the above
    * Also sent only in DEBUG mode

### Examples
See below examples as they can be used with the cansend utility (http://elinux.org/Can-utils). So you can invoke e.g. cansend can0 XXX, where XXX is in the below table
* 202#80.04 - changes heartbeat for node 2 to 4 seconds
* 202#81.2C - changes heartbeat for node 2 to 300 seconds (5 minutues = default)
* 202#40.01 - switches on suppress switch on node 2 (pressing the switch on that node after this action will have no effect - no CAN message would get sent)
* 202#00.03 - changes nodeID for node 2 to nodeID 3 (so after this, the same message would not get processed by the same node again anymore since the nodeID changed)
* 011#10    - toggles the switch on node 11 (here the number in the data can be any number as long as first 2 bits are 0, i.e. anything from 0 to 3F. Ranges would then tell more about the actual sending chip as per the encoding of the data byte. See https://github.com/PoJD/piclib/blob/master/can.h method can_combineCanDataByte. E.g. for firmware version 1 and no errors the CanSwitch would actually send data byte between 08 and 0F

#### Complex message types below
* 311#00 - toggles the switch of the node 11 (any number between 0 and 3F for data)
* 311#40 - switches on the node 11 (any number between 40 and 7F)
* 311#80 - switches off the node 11 (any number between 80 and BF)
* 300#C0 - complex get - detect the state of all switches on floor 0 - a complex reply will come (any number between C0 and FF would work)
 * for example: 400#1E.00.00.00.00.00.00.01 (see COMPLEX REPLY above. Here we have 1E outputs = 30, all have value 0 = off, CAN errors are 0 and FIRMWARE version is 1. Can ID has no impact here other than the range - see above (e.g. 300 to 37F is for floor 0, for larger can IDs the 1st floor CanRelay would reply instead - i.e. for 380 to 3FF). The reply would include the floor too in canID, e.g. 400 for ground floor and 480 for first floor
* 300#40 - switches on all lights on floor 0
* 380#80 - switches off all lights on floor 1

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
