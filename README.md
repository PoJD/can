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
