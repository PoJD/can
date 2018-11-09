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
* Typical values in HEX now sent for the CanSwitches (firmware version 0, no errors and toggle) shall be in range 0-7 - just the switch counter in there. For anything larger, a CAN error or new firmware version
* The switch now supports wiring up to 8 real wall switches to minimize the need to have PCB in each wall switch. This switch can then send CAN message for any of the 8 B port input changes to logical zero. The switch then sends CAN ID of nodeID + PORTB pin number, e.g. nodeID directly for change on B0, nodeID+1 for change on B1, etc
* As long as the wall switch count does not exceed 8, each can be tight up to individual input pin even if it 2 or more physical light switches should be switching on 1 output pin on CanRelay. Just the respective mapping has to be set properly. See mapping of ports section in CanRelay.X
* If EEPROM bucket 3 is set to a value greater than 0, then the CanSwitch would actually send 2 messages on change on input portB0 to set OFF both floors (e.g. canID in that case would be equal to floor GROUND and floor FIRST respectively, data byte would be similar as above, just the 2 bits for operation would be equal to 0b10 = OFF. By default when this EEPROM bucket is not set, this behavior would not kick in
* DEBUG mode
    * Disabled by default and needs to be enabled in firmware (defined constant in firmware file)
    * non DEBUG mode (default) heavily utilizes power savings, putting the chip to sleep using low current drawning specs to minimize power consumption, disabling the crystal, whole chip and even the transceiver, only waken up by input interrupt. Current drawn in sleep measured as low as 1.8uA. The current drawn in non DEBUG mode is about 18mA instead.
* Extra features in DEBUG mode
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
* As of firmware version 3, CanRelay also supports receiving CONFIG messages (nodeID has to match the floor this time exactly), then it assumes exactly 3 bytes of data and uses these to set new mapping from nodeID to output number
* As of firmware version 3, CanRelay also understands MAPPING message types and would reply with runtime mappings set for this floor. See more details below

#### Mapping of ports
* See https://github.com/PoJD/can/blob/master/CanRelay.X/relayMappings.c for details (mappings outputs to ports and bits to change
* CanRelay keeps internally all mappings from output numbers as visible on the silkscreen (1..30) to output PORTs and bits to change and in addition to that allows a dynamic "map" from nodeID to a given output. Multiple outputs can be configured to be mapped to the same nodeID, e.g. being able to set multiple switches to switch on the same light
* CanRelay stores the dynamic mappings in EEPROM, starting from bucket 1 (byte 2)
* The received CONFIG messages should have 3 bytes: mapping number, nodeID, output number. Mapping number should be in range 1..0xFF and marks the position in DAO to store this mapping into. Mind that the firmware does not check whether all previous mappings were set, if not, this new would get effectively ignored on next startup since all mappings are assumed to be present in EEPROM in sequence from bucket 1. nodeID shall be any 8 bit value representing the nodeID as transmitted over CAN. Output number shall be any number in range 1..30 and marks the respective output label on the silkscreen
* File canRelayMappings.sh contains all mappings for both floors that are now used

## Communication Protocol
Custom communication protocol was established, inspired partially in VSCP

CAN ID
* Extended CAN identifiers not used, so only 11 bits used for standard CAN ID
* First 3 bits are reserved for message type: https://github.com/PoJD/piclib/blob/master/can.h
* Remaining 8 bits are reserved for internal nodeID: https://github.com/PoJD/can/blob/master/CanSetup.X/canSwitches.h
* Node id 1st bit is always the floor, so effectively 7 bits remaining for each floor (which should be sufficient). https://github.com/PoJD/can/blob/master/CanSetup.X/canSwitches.h lists all nodeIDs for all switches. Each has 8 IDs reserved since in theory we can wire up to 8 individual wall switches to one CanSwitch

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
    * For CanSwitch
        * allows canSwitches to be configured remotely over CAN bus, so only the respective CAN node would process this message
        * 2 bytes of data CAN sent in this case
        * highest 2 bits = datatype ( https://github.com/PoJD/piclib/blob/master/dao.h ), remaining 14 bits = value of this datatype
        * The respective node stores these new values into EEPROM and updates the runtime variables to act accordingly (allows changing nodeID, heartbeat timeout and suppressing the switch functionality for CanSwitch.
        * Means this is only supported in DEBUG mode for CanSwitches
    * For CanRelay
        * allows relay mappings to be dynamically set, see above Mapping of ports section
* HEARTBEAT (1)
    * Only sent by the CanSwitches
    * Similar as NORMAL. In addition to the 1 byte sent in NORMAL message, this also sets 4 more bytes in CAN data as per the above
    * Also sent only in DEBUG mode
* MAPPING (5) and MAPPING_REPLY (6)
    * Only CanRelay would reply to this traffic. For this message type and nodeID = floor, regardless the data frame, the respective CanRelay would send over all relayMappings it currently uses at runtime to translate from nodeIDs to outputs. The CAN ID would be = MAPPING_REPLY + floor nodeID
    * If more than 8 mappings are used, it would split the mappings into multiple CAN messages
    * no counter sent or total size, just all mappings over
    * Is meant to be used in target application only for testing purpose to confirm mappings set and used in CanRelay since this would occupy the chip for quite some time and could lead to dropped operation traffic (no buffer overflow in there and if it would to be sending say 32 messages, that would add up to quite some amount of time already, so do not use this message in PROD!

### Examples
See below examples as they can be used with the cansend utility (http://elinux.org/Can-utils). So you can invoke e.g. cansend can0 XXX, where XXX is in the below table
* 202#80.04 - changes heartbeat for node 2 to 4 seconds
* 202#81.2C - changes heartbeat for node 2 to 300 seconds (5 minutues = default)
* 202#40.01 - switches on suppress switch on node 2 (pressing the switch on that node after this action will have no effect - no CAN message would get sent)
* 202#00.03 - changes nodeID for node 2 to nodeID 3 (so after this, the same message would not get processed by the same node again anymore since the nodeID changed)
* 201#C0.01 - sets the flag of node 1 to send OFF messages for both floors when switch press on port B 0 is detected
* 011#10    - toggles the switch on node 11 (here the number in the data can be any number as long as first 2 bits are 0, i.e. anything from 0 to 3F. Ranges would then tell more about the actual sending chip as per the encoding of the data byte. See https://github.com/PoJD/piclib/blob/master/can.h method can_combineCanDataByte. E.g. for firmware version 1 and no errors the CanSwitch would actually send data byte between 08 and 0F
* 000#00 - should never be sent in the current implementation, but would effectively toggle all outputs on floor 0 (using NORMAL message)
* 080#00 - should never be sent in the current implementation, but would effectively toggle all outputs on floor 1
* 200#03.04.02 - changes/sets mapping 3 in floor 0 to map nodeID 4 to output 2. All mappings up to 3 (e.g. 1 and 2) has to be set in order for this to be effective 

#### Complex message types below
* 311#00 - toggles the switch of the node 11 (any number between 0 and 3F for data)
* 311#40 - switches on the node 11 (any number between 40 and 7F)
* 311#80 - switches off the node 11 (any number between 80 and BF)
* 300#C0 - complex get - detect the state of all switches on floor 0 - a complex reply will come (any number between C0 and FF would work)
 * for example: 400#1E.00.00.00.00.00.00.01 (see COMPLEX REPLY above. Here we have 1E outputs = 30, all have value 0 = off, CAN errors are 0 and FIRMWARE version is 1. Can ID has no impact here other than the range - see above (e.g. 300 to 37F is for floor 0, for larger can IDs the 1st floor CanRelay would reply instead - i.e. for 380 to 3FF). The reply would include the floor too in canID, e.g. 400 for ground floor and 480 for first floor
* 300#40 - switches on all lights on floor 0
* 380#80 - switches off all lights on floor 1

#### Mappings
* 500#00 - get all runtime mappings of floor 0
    * A reply could look like: 600#01.01.02.02.03.02.05.03 - so just 4 mappings for this floor, mapping nodeID 1 to output 1, nodeID 2 to output 2, nodeID 3 to output 2 and nodeID 5 to output 3


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
* Start can using the above: (s2 below is 50kbps). Using the generic device allows the OS to survive unplug and replug of the USBTin which often results in different ACM* device being set in /dev/

```bash
#!/bin/bash 
  
set -x 

device=/dev/ttyACM*
slcan_attach -f -s2 -o $device -n can0
slcand $device can0
ifconfig can0 up
```
