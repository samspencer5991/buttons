/*
 * buttons.h
 *
 *  Created on: 21Jun.,2019
 *      Author: samspencer
 *
 * Buttons is used to refer to software instances
 * Switches is used to refer to hardware switches
 *
 * SWC 	- Switch Control
 * SWFS	- Switch FootSwitch
 * FS	- FootSwitch
 *
 * The intended api usage is described below:
 *
 * First, call buttons_init() with the number of buttons you wish to use
 *
 * Then create each button using the buttons_create() function.
 * The pointers for the buttons are stored privately in the .c file.
 * Each button may be referenced individually using it's index,
 * however, this is only recommended for specific cases
 * where getting/setting states manually is required.
 *
 * The handler function that is called for each button is called
 * for each button is passed the state of that button.
 * An example handler might look like this:
 *
	void FS1_HANDLER(ButtonState state)
	{
		if(state == Pressed)
		{

		}
		else if(state == Released)
		{

		}
		else if(state == Held)
		{

		}
		else if(state == HeldReleased)
		{

		}
	}
 *
 * Additionally, the timer triggered function has to be checked externally of this api (e.g. in main.c)
 * The timer instance that was passed to buttons_init() can check for the button timer and action accordingly.
 * Because a button's state will only change to
 * Any function which has the potential for error will return an error state
 * This should be checked and handled by the main application.
 * The error state is given by the ButtonErrorState typedef
 *
 *
 * Button hold logic is implemented using a single hardware timer.
 * This reduces hardware resources consumed by the api with one caveat.
 * Individual hold times are not recorded, only a "global" hold time/
 * For example, one button is held, and just before the hold time elapses,
 * a second button is pressed and held. This will trigger both buttons being held
 * despite the fact that they were not both held for the required hold time.
 *
 * For multiple simultaneous hold events, the first button will be called,
 * and then every button required for the event can be checked. The extra buttons
 * have to have their states reset to cleared as this is only done for the first button.
 */

#ifndef BUTTONS_H_
#define BUTTONS_H_

#if FRAMEWORK == STM32CUBE
#include "gpio.h"
#elif FRAMEWORK == ARDUINO
#include <Arduino.h>
#endif

#include "definition_linker.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_ALL_SWITCHES 2

#ifndef NUM_ALL_SWITCHES
//#error *** Define NUM_ALL_SWITCHES according to how many buttons are being used ***
#endif

/* Debouncing and handling */
#define DEBOUNCE_TIME 20 // minimum debounce time for mechanical switches

// Counter value when the acceleration is reset (button is released)
#ifndef BUTTON_ACCELERATION_THRESHOLD
#define BUTTON_ACCELERATION_THRESHOLD 18
#endif
// Granular steps for decrementing the acceleration counter
#ifndef BUTTON_ACCELERATION_STEP
#define BUTTON_ACCELERATION_STEP 1
#endif
// Value for the acceleration counter to max out at
#ifndef BUTTON_ACCELERATION_CAP
#define BUTTON_ACCELERATION_CAP 6
#endif

#ifndef DOUBLE_PRESS_TIME
#define DOUBLE_PRESS_TIME 300
#endif

// represents function errors from the API
typedef enum
{
	ButtonMemError,
	ButtonParamError,
	ButtonHalError,
	ButtonOk
} ButtonErrorState;

typedef enum
{
	ActiveLow,
	ActiveHigh
} ButtonLogic;

// Activity states of each button
typedef enum
{
	Pressed,
	DoublePressed,
	Released,
	DoublePressReleased,
	Held,
	HeldReleased,
	Cleared,
	HeldRepeat
} ButtonState;

typedef enum
{
	ButtonEmulatePress,
	ButtonEmulateRelease,
	ButtonEmulateNone
} ButtonEmulateAction;

// Typedefs for button type
typedef enum
{
	Momentary,
	latching
} ButtonMode;

typedef enum
{
	ButtonPending,
	ButtonCancel,
	ButtonContinue
} ButtonBinaryDecision;

// Stores data related to each button
typedef struct
{
	ButtonMode mode;								// physical hardware type of the button (eg. latching or momentary)
	volatile ButtonState state;				// current state of button. Also used to trigger polled handler functions
	volatile ButtonState lastState; 			// previous state of button (used for toggling and debouncing)
	volatile uint32_t lastTime;				// time since last event (used for debouncing and holding)
	uint8_t accelerationCounter;				// May be used in application to track hold acceleration functionality
	uint8_t accelerationThreshold;
	volatile uint8_t accelerationTrigger;
	uint16_t pin;									// hardware pin
#if FRAMEWORK == STM32CUBE
	GPIO_TypeDef *port;							// hardware port
#endif
	uint8_t pressEvent;							// Stores whether a press event has occured so that the release event is cancelled
	ButtonLogic logicMode;						// Sets whether the input is active low or high
	void (*handler)(ButtonState state); 	// pointer to the handler function for that button
} Button;

/* PUBLIC FUNCTION PROTOTYPES */
/* SETUP FUNCTIONS */
#if FRAMEWORK == ARDUINO
// ButtonErrorState buttons_setHoldTimer(TIM_HandleTypeDef *timHandle, uint16_t time);
ButtonErrorState buttons_create(uint32_t GPIO_Pin, ButtonMode mode, ButtonLogic logicMode,
								uint16_t index, void (*funcp)(ButtonState state));

#elif FRAMEWORK == STM32CUBE
ButtonErrorState buttons_setHoldTimer(TIM_HandleTypeDef *timHandle, uint16_t time);
ButtonErrorState buttons_create(GPIO_TypeDef *GPIOx, uint32_t GPIO_Pin, ButtonMode mode, ButtonLogic logicMode,
								uint16_t index, void (*funcp)(ButtonState state));
#endif

/* SET FUNCTIONS */
ButtonErrorState buttons_setState(uint8_t index, ButtonState state);
ButtonErrorState buttons_setLastState(uint8_t index, ButtonState state);

/* GET FUNCTIONS */
uint32_t buttons_getPin(uint16_t index);
ButtonState buttons_getState(uint8_t index);

/* POLLING/LOOP FUNCTIONS */
void buttons_extiGpioCallback(uint16_t index, ButtonEmulateAction emulateAction);
void buttons_holdTimerElapsed();
void buttons_triggerPoll();

extern volatile Button buttons[NUM_ALL_SWITCHES];

#ifdef __cplusplus
}
#endif

#endif /* BUTTONS_H_ */
