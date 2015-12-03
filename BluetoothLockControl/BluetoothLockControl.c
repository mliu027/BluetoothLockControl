/*
 * BluetoothLockControl.c
 *
 * Created: 11/20/2015 8:04:19 PM
 *  Author: Mitch Liu
 */ 


#include <avr/io.h>
#include "keypad.h"
#include "timer.h"
#include "scheduler.h"
#include "io.c"
#include "usart_ATmega1284.h"
#include <avr/eeprom.h>
#define CORRECTPINSTARTADDR 0x00

task tasks[5];

// --------Global Variables ----------
unsigned char inputPin[16];
unsigned char position;
unsigned char lastReceivedChar;
unsigned short controlCounter;
unsigned char correctPin[16];
uint8_t correctPinAddr = CORRECTPINSTARTADDR;
// --------Global Variables ----------


//---------------Flags---------------
unsigned char checkPinFlag;
unsigned char lockedFlag;
unsigned char keypadEnable;
unsigned char bluetoothEnable;
unsigned char isCorrect;
unsigned char checkCounter;
unsigned char showPin;
unsigned char newPinInputComplete;
unsigned char pinInputComplete;
//---------------Flags---------------


void clearInputPin()						//clears the Character Array for Input Pin
{
	for(unsigned char i = 0; i < 8; ++i){
		inputPin[i] = '\0';
	}
}

void clearFlags(){
	checkPinFlag = 0;
	keypadEnable = 0;
	bluetoothEnable = 0;
	controlCounter = 0;
	lockedFlag = 1;
	showPin = 0;
	pinInputComplete = 0;
}

unsigned char getInputPinLength(){
	unsigned char i = 0;
	while(inputPin[i] != '\0'){
		++i;
	}
	return i;
}

unsigned char getCorrectPinLength(){
	unsigned char i = CORRECTPINSTARTADDR;
	while(eeprom_read_byte(i) != '\0'){
		++i;
	}
	return i;
}

enum SM_Controller{controllerinit, controllerWait, controllerKeypad, controllerBluetooth, controllerCheck,
	 controllerUnlocked, controllerLocked, controllerLockWaitRelease, controllerChangePin, controllerIntrusionDetected, 
	 controllerIntrusionPin, controllerIntrusionCheck};
int TickFct_Controller(int state){
	unsigned char key = GetKeypadKey();
	unsigned char doorClosed = (~PIND & 0x10) >> 4;
	switch(state){
		case controllerinit:
			clearFlags();											
			lockedFlag = 1;
			showPin = 0;
			state = controllerWait;
			clearInputPin();
			LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
			break;
		case controllerWait:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(key == 'A'){
				state = controllerKeypad;
				keypadEnable = 1;
				clearInputPin();
				LCD_DisplayString(1, "Pin: ");
			}
			else if(key == 'B'){
				state = controllerBluetooth;
				bluetoothEnable = 1;
				clearInputPin();
				LCD_DisplayString(1, "Waiting to Rec  C to Cancel");
			}
			break;
		case controllerBluetooth:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(key == 'C'){
				state = controllerWait;
				bluetoothEnable = 0;
				LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
				clearFlags();
			}
			else if(pinInputComplete){
				state = controllerCheck;
				checkPinFlag = 1;
				bluetoothEnable = 0;
				LCD_DisplayString(1, "Checking...     C to Cancel");
			}
			else{
				state = controllerBluetooth;
			}
			break;
		case controllerKeypad:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(pinInputComplete){
				state = controllerCheck;
				checkPinFlag = 1;
				keypadEnable = 0;
				LCD_DisplayString(1, "Checking...     C to cancel");
			}
			else{
				state = controllerKeypad;
			}
			break;
		case controllerCheck:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(key == 'C'){
				state = controllerWait;
				clearInputPin();
				clearFlags();
				LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
			}
			else if(checkPinFlag){
				state = controllerCheck;
			}
			else{
				if(lockedFlag){
					state = controllerLocked;
					controlCounter = 0;
					LCD_DisplayString(1, "Incorrect Pin");
				}
				else{
					state = controllerUnlocked;
					LCD_DisplayString(1, "Press A to Lock C to change Pin");
				}
			}
			break;
		case controllerLocked:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(controlCounter >= 750){
				state = controllerWait;
				clearInputPin();
				LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
				clearFlags();
			}
			else{
				state = controllerLocked;
			}
			break;
		case controllerUnlocked:
			if(key == 'A'){
				state = controllerLockWaitRelease;
			}
			else if(key == 'C'){
				state = controllerChangePin;
				showPin = 1;
				keypadEnable = 1;
				pinInputComplete = 0;
				clearInputPin();
				LCD_DisplayString(1, "Pin: ");
			}
			break;
		case controllerLockWaitRelease:
			if(!doorClosed && lockedFlag ){
				state = controllerIntrusionDetected;
				PORTA = 0xFF;
				LCD_DisplayString(1, "ALARM! Enter Pin: ");
			}
			else if(key != '\0'){
				state = controllerLockWaitRelease;
			}
			else{
				state = controllerWait;
				clearInputPin();
				LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
				clearFlags();
			}
			break;
		case controllerChangePin:
			if(!newPinInputComplete){
				state = controllerChangePin;
			}
			else{
				state = controllerUnlocked;
				keypadEnable = 0;
				showPin = 0;
				LCD_DisplayString(1, "Press A to Lock C to change Pin");
			}
			break;
		case controllerIntrusionDetected:
			state = controllerIntrusionPin;
			keypadEnable = 1;
			pinInputComplete = 0;
			clearInputPin();
			break;
		case controllerIntrusionPin:
			if(pinInputComplete){
				state = controllerIntrusionCheck;
				checkPinFlag = 1;
				keypadEnable = 0;
				LCD_DisplayString(1, "Checking...     C to cancel");
			}
			else{
				state = controllerIntrusionPin;
			}
			break;
		case controllerIntrusionCheck:
			if(checkPinFlag){
				state = controllerIntrusionCheck;
			}
			else{
				if(lockedFlag){
					state = controllerIntrusionDetected;
					controlCounter = 0;
					LCD_DisplayString(1, "Incorrect Pin");
					delay_ms(500);
					LCD_DisplayString(1, "ALARM! Enter Pin: ");
				}
				else{
					state = controllerUnlocked;
					PORTA = 0x00;
					LCD_DisplayString(1, "Press A to Lock C to change Pin");
				}
			}
			break;
		default:
			state = controllerinit;
			break;
	}
	switch(state){
		case controllerinit:
			break;
		case controllerWait:
			break;
		case controllerKeypad:
			keypadEnable = 1;
			break;
		case controllerBluetooth:
			bluetoothEnable = 1;
			break;
		case controllerCheck:
			break;
		case controllerLocked:
			++controlCounter;
			break;
		case controllerUnlocked:
			break;
		case controllerLockWaitRelease:
			break;
		case controllerChangePin:
			break;
		case controllerIntrusionDetected:
			break;
		case controllerIntrusionPin:
			break;
		case controllerIntrusionCheck:
			break;	
	}
	return state;
}

enum SM_BluetoothReceiver{bluetoothWait, bluetoothReceived, bluetoothWaitForNext};
int TickFct_BluetoothReceiver(int state){
	switch(state){
		case bluetoothWait:
			if(USART_HasReceived(0) && !bluetoothEnable){
				USART_Flush(0);
				state = bluetoothWait;
			}
			else if(USART_HasReceived(0) && bluetoothEnable){
				state = bluetoothReceived;
				position = 0;
				lastReceivedChar = 0;
				clearInputPin();
			}
			else{
				state = bluetoothWait;
			}
			break;
		case bluetoothReceived:
			if(!bluetoothEnable){
				state = bluetoothWait;
			}
			else if(lastReceivedChar != '*'){
				state = bluetoothWaitForNext;
			}
			else{
				state = bluetoothWait;
			}
			break;
		case bluetoothWaitForNext:
			if(!bluetoothEnable){
				state = bluetoothWait;
			}
			else if(!USART_HasReceived(0)){
				state = bluetoothWaitForNext;
			}
			else{
				state = bluetoothReceived;
			}
			break;
		default:
			state = bluetoothWait;
			break;
	}
	switch(state){
		case bluetoothWait:
			break;
		case bluetoothReceived:
			while(lastReceivedChar != '*'){
				lastReceivedChar = USART_Receive(0);
				if(lastReceivedChar != '*'){
					inputPin[position++] = lastReceivedChar;
				}
				else{
					inputPin[position] = '\0';
					pinInputComplete = 1;
				}
			}
			break;
		case bluetoothWaitForNext:
			break;
	}
	return state;
}

enum SM_KeypadReceiver{keypadWait1, keypadWait2, keypadPressed, keypadWaitRelease};
int TickFct_KeypadReceiver(int state){
	unsigned char key = GetKeypadKey();
	switch(state){
		case keypadWait1:
			if(key == '\0' || !keypadEnable){
				state = keypadWait1;
			}
			else{
				state = keypadPressed;
				position = 0;
				clearInputPin();
			}
			break;
		case keypadPressed:
			if(!keypadEnable){
				state = keypadWait1;
			}
			else if(key == '*'){
				state = keypadWait1;
			}
			else{
				state = keypadWaitRelease;
			}
			break;
		case keypadWaitRelease:
			if(!keypadEnable){
				state = keypadWait1;
			}
			else if(key !='\0'){
				state = keypadWaitRelease;
			}
			else{
				state = keypadWait2;
			}
			break;
		case keypadWait2:
			if(!keypadEnable){
				state = keypadWait1;
			}
			else if(key == '\0'){
				state = keypadWait2;
			}
			else{
				state = keypadPressed;
			}
			break;
		default:
			state = keypadWait1;
			break;
	}
	switch(state){
		case keypadWait1:
			break;
		case keypadWait2:
			break;
		case keypadPressed:
			if(key != '*'){
				if(key != 'A' && key != 'C'){
					inputPin[position++] = key;
					if(showPin){
						LCD_WriteData(key);
					}
					else{
						LCD_WriteData('*');
					}
				}
			}
			else{
				if(showPin){
					LCD_DisplayString(1, "Changing...");
					inputPin[position] = '\0';
					newPinInputComplete = 1;
					unsigned char counter = 0;
					correctPinAddr = CORRECTPINSTARTADDR;
					while(1){
						PORTA = 1;
						eeprom_write_byte(correctPinAddr++, inputPin[counter]);
						if(inputPin[counter] == '\0'){
							break;
						}
						++counter;
					}
					newPinInputComplete = 1;
				}
				else{
					inputPin[position] = '\0';
					pinInputComplete = 1;
				}
			}
			break;
		case keypadWaitRelease:
			break;
	}
	return state;
}

enum SM_CheckPin {checkInit, checkWait, checkCheck};
int TickFct_CheckPin(int state){
	switch(state){
		case checkInit:
			state = checkWait;
			break;
		case checkWait:
			if(checkPinFlag){
				state = checkCheck;
				isCorrect = 1;
				checkCounter = 0;
				correctPinAddr = CORRECTPINSTARTADDR;
				if(getCorrectPinLength() != getInputPinLength()){
					isCorrect = 0;
				}
				while(eeprom_read_byte(correctPinAddr + checkCounter) != '\0' && isCorrect){
					if(eeprom_read_byte(correctPinAddr + checkCounter) != inputPin[checkCounter]){
						isCorrect = 0;
						break;
					}
					++checkCounter;
				}
				if(isCorrect){
					lockedFlag = 0;
				}
				else{
					lockedFlag = 1;
				}
				checkPinFlag = 0;
			}
			else{
				state = checkWait;
			}
			break;
		case checkCheck:
			state = checkWait;
			break;
		default:
			state = checkInit;
			break;
	}
	switch(state){
		case checkInit:
			break;
		case checkWait:
			break;
		case checkCheck:
			break;
	}
	return state;
}

enum SM_USART{usartInit, usartLocked, usartUnlocked};
int TickFct_USART(int state){
	switch(state){
		case usartInit:
			state = usartLocked;
			USART_Send(0x01, 1);
			break;
		case usartLocked:
			if(lockedFlag){
				state = usartLocked;
			}
			else{
				state = usartUnlocked;
				USART_Send(0x00, 1);
				LCD_DisplayString(1, "Unlocking...");
				delay_ms(5000);
				LCD_DisplayString(1, "Press A to Lock C to Change Pin");
			}
			break;
		case usartUnlocked:
			if(lockedFlag){
				state = usartLocked;
				USART_Send(0x01, 1);
				LCD_DisplayString(1, "Locking...");
				delay_ms(5000);
				LCD_DisplayString(1, "A for Keypad    B for Bluetooth");
			}
			else{
				state = usartUnlocked;
			}
			break;
		default:
			state = usartInit;
			break;
	}
	return state;
}

ISR(TIMER1_COMPA_vect)
{
	unsigned char i;
	for(i = 0; i < 5; ++i){
		if( tasks[i].elapsedTime >= tasks[i].period){
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += 1;
	}
}

int main(void)
{
	DDRA = 0xFF;	PORTA = 0x00;
	DDRB = 0xF0;	PORTB = 0x0F;
	DDRC = 0xFF;	PORTC = 0x00;
	DDRD = 0x00;	PORTD = 0xFF;
	eeprom_write_byte(correctPinAddr++, '0');
	eeprom_write_byte(correctPinAddr++, '0');
	eeprom_write_byte(correctPinAddr++, '0');
	eeprom_write_byte(correctPinAddr++, '0');
	eeprom_write_byte(correctPinAddr++, '\0');
	initUSART(0);
	initUSART(1);
	LCD_init();
	tasks[0].state = controllerinit;
	tasks[0].period = 1;
	tasks[0].elapsedTime = 1;
	tasks[0].TickFct = &TickFct_Controller;
	
	tasks[1].state = bluetoothWait;
	tasks[1].period = 5;
	tasks[1].elapsedTime = 5;
	tasks[1].TickFct = &TickFct_BluetoothReceiver;
	
	tasks[2].state = keypadWait1;
	tasks[2].period = 10;
	tasks[2].elapsedTime = 10;
	tasks[2].TickFct = &TickFct_KeypadReceiver;
	
	tasks[3].state = checkInit;
	tasks[3].period = 15;
	tasks[3].elapsedTime = 15;
	tasks[3].TickFct = &TickFct_CheckPin;
	
	tasks[4].state = usartInit;
	tasks[4].period = 5;
	tasks[4].elapsedTime = 5;
	tasks[4].TickFct = &TickFct_USART;
	
	TimerSet(1);
	TimerOn();
	while(1)
    {
    }
}