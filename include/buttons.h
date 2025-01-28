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

// Include proper libraries for each supported framework
#if FRAMEWORK_STM32CUBE
#if defined(STM32G4xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32H5xx)
#include "stm32h5xx_hal.h"
#elif defined(STM32G0xx)
#include "stm32g0xx_hal.h"
#endif
#elif FRAMEWORK_ARDUINO
#include <Arduino.h>
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Check that a valid MCU core has been defined
#if !defined(FRAMEWORK_ARDUINO) && !defined(FRAMEWORK_STM32CUBE)
#error *** BUTTONS.H - No supported framework defined for GPIO handling ***
#endif

// Debouncing logic:
// Minimum practical switch windows (total active time) = 20ms
// Minimum time between previous release and next press = 100ms


/* Debouncing and handling */
// minimum debounce time for mechanical switches
#define DEBOUNCE_TIME 5
#define DEBOUNCE_LOW_TO_HIGH 10
#define DEBOUNCE_HIGH_TO_LOW 50

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

#ifndef MULTIPLE_BUTTON_TIME
#define MULTIPLE_BUTTON_TIME 100
#endif

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
   // Assign in application
	ButtonMode mode;				    		// physical hardware type of the button (eg. latching or momentary)
   ButtonLogic logicMode;					// Sets whether the input is active low or high
	void (*handler)(ButtonState state); // pointer to the handler function for that button
   uint16_t pin;								// hardware pin
#if FRAMEWORK_STM32CUBE
    GPIO_TypeDef *port;						// hardware port
#endif
   // Private
	volatile ButtonState state;		    	// current state of button. Also used to trigger polled handler functions
	volatile ButtonState lastState;     	// previous state of button (used for toggling and debouncing)
	volatile uint32_t lastTime;		    	// time since last event (used for debouncing and holding)
	uint8_t accelerationCounter;				// May be used in application to track hold acceleration functionality
	uint8_t accelerationThreshold;
	volatile uint8_t accelerationTrigger;
	uint8_t pressEvent;							// Stores whether a press event has occured so that the release event is cancelled
	volatile uint8_t timerTriggered;
} Button;

//-------------- PUBLIC FUNCTION PROTOTYPES --------------//
#if FRAMEWORK_ARDUINO
void buttons_AssignTimerStopCallback(void (*callback)(void));
void buttons_AssignTimerStartCallback(void (*callback)(void));
void buttons_AssignTimerGetCounterCallback(uint32_t (*callback)(void));
#elif FRAMEWORK_STM32CUBE
void buttons_SetHoldTimer(TIM_HandleTypeDef *timHandle, uint16_t time);
#endif
void buttons_Init(Button* button);

void buttons_ExtiGpioCallback(Button* button, ButtonEmulateAction emulateAction);
void buttons_HoldTimerElapsed(Button* buttons, uint16_t numButtons);
void buttons_TriggerPoll(Button* buttons, uint16_t numButtons);

#ifdef __cplusplus
}
#endif

#endif /* BUTTONS_H_ */
