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

### CanSetup.X
This project serves to setup either CanSwitch or CanRealy chips - write the necessary data to EEPROM based on the node.
Key features:

* invoke setupCanSwitch (nodeId) to setup EEPROM of the chip as the CanSwitch
* invoke setupCanRelay (bit floor) to setup EEPROM of the chip as the CanRelay
* Uses port RC1 as a "status port"
** It blinks shortly (1/4 of a second) after a button is pressed to indicate that works
** Followed by either 1sec long blink to indicate CAN message sent OK or 4 short blinks in a sec to indicate a CAN error

### CanRelay.X
This project is the can relay implementation. The chips programmed this way are planned to be installed on a PCB directly controlling the relays for lights.
Key features:

* Registers for all NORMAL messages for a given floor (the floor has to be setup in EEPROM first)
* For a given normal message, it would ignore the first bit and treat the rest as the counter of port to switch on.

#### Mapping of ports
* 1-8 toggles PORTC1..PORTC7
* 9-16 toggles PORTB1..PORTB7 (PORTB3 and PORTB2 are used by CANRX and CANTX though, so should not be used = 11 and 12)
* 17-24 toggles PORTA1..PORTA7 (PORTA4 does not exist though, so should not be used = 21)
