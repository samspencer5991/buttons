/*
 * buttons.c
 *
 *  Created on: 21Jun.,2019
 *      Author: samspencer
 */

#include "buttons.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Helper Macros */
#define TRUE	1
#define FALSE	0
#define CLEAR 0

/* For accurate button hold fundtionality, the main application must configure a timer,
* and assign the following callbacks. On the timer interrupt, buttons_holdTimerElapsed must be called
* with all required sequential button pointers and the number of buttons.
* The timer must be configured to provide a millisecond resolution counter, accessed by the
* timerGetCountCallback callback. 
* The timerConfigured flag indicates all timer callbacks have been assigned.
* Timer based functionality will only be used if this flag is set.
*
* For applications using the STM32Cube frame, the timer is instead a HAL typedef
*/
#if FRAMEWORK_STM32CUBE
TIM_HandleTypeDef* holdTim;
#elif FRAMEWORK_ARDUINO
void (*timerStopCallback)(void) = NULL;
void (*timerStartCallback)(void) = NULL;
uint32_t (*timerGetCountCallback)(void) = NULL;
#endif
uint8_t timerConfigured = FALSE;

uint16_t buttonHoldTime;

//-------------- PRIVATE FUNCTION PROTOTYPES --------------//
uint8_t buttons_GetPinState(Button* button);
void buttons_ResetTimerCounter();


//-------------- PUBLIC FUNCTIONS --------------//
void buttons_Init(Button* button)
{
	// Assign default values
	button->state = Cleared;
	button->lastState = Released;
	button->lastTime = 0;
	button->pressEvent = FALSE;

	button->accelerationThreshold = BUTTON_ACCELERATION_THRESHOLD;
	button->accelerationCounter = 0;
}

#if FRAMEWORK_ARDUINO
void buttons_AssignTimerStopCallback(void (*callback)(void))
{
    timerStopCallback = callback;
    if(timerStartCallback != NULL &&
       timerGetCountCallback != NULL)
    {
        timerConfigured = TRUE;
    }
}

void buttons_AssignTimerStartCallback(void (*callback)(void))
{
    timerStartCallback = callback;
    if(timerStopCallback != NULL &&
       timerGetCountCallback != NULL)
    {
        timerConfigured = TRUE;
    }
}

void buttons_AssignTimerGetCounterCallback(uint32_t (*callback)(void))
{
    timerGetCountCallback = callback;
    if(timerStopCallback != NULL &&
       timerStartCallback != NULL)
    {
        timerConfigured = TRUE;
    }
}

#elif FRAMEWORK_STM32CUBE
void buttons_SetHoldTimer(TIM_HandleTypeDef *timHandle, uint16_t time)
{
	// Check parameters
	if(timHandle == NULL)
	{
		return;
	}
	holdTim = timHandle;

	/* Calculate prescaler and period values based on CPU frequency
	 * The prescaler is set so that the timer resolution is equal to a millisecond
	 * This allows for easy setting of the Period directly in milliseconds
	 */
	uint32_t cpuFreq = HAL_RCC_GetSysClockFreq();
	holdTim->Init.Prescaler = cpuFreq / 10000;				// This assumes the clock is in the MHz range
	holdTim->Init.Period = time*10;
	timerConfigured = TRUE;

	// Update timer instance with new timing values and clear the interrupt flag to prevent initial mis-fire (bug found previously)
	if (HAL_TIM_Base_Init(holdTim) != HAL_OK)
	{
		return;
	}
	HAL_TIM_Base_Stop_IT(holdTim);
	__HAL_TIM_CLEAR_FLAG(holdTim, TIM_IT_UPDATE);
	buttons_ResetTimerCounter();
	return;
}

#endif

void buttons_TriggerPoll(Button* buttons, uint16_t numButtons)
{
	// Find which button has a new event, execute the callback handler, then clear the state when finished
	// Note that the full loop is allowed to continue in case multiple interrupts were fired before polling
	for(int i=0; i<numButtons; i++)
	{
		if(buttons[i].state != Cleared)
		{
			ButtonState tempState = buttons[i].state;
			buttons[i].state = Cleared;
			if(buttons[i].handler != NULL)
				buttons[i].handler(tempState);
		}
		if(buttons[i].accelerationTrigger)
		{
			buttons[i].handler(HeldRepeat);
			buttons[i].accelerationTrigger = FALSE;
		}
	}
}

void buttons_HoldTimerElapsed(Button* buttons, uint16_t numButtons)
{
	if(timerConfigured)
	{
#if FRAMEWORK_STM32CUBE
		HAL_TIM_Base_Stop_IT(holdTim);
#elif FRAMEWORK_ARDUINO
		if(timerStopCallback != NULL)
			timerStopCallback();
#endif
	}
	// Check hold states for all the buttons before hold is actioned
	// This ensures multiple holds are all captured before being actioned
	for(int i=0; i<numButtons; i++)
	{
		// Check not only if the button has not been released, but if a timer event was triggered for that button
		if(buttons[i].lastState == Pressed || buttons[i].lastState == DoublePressed && buttons[i].timerTriggered)
		{
			buttons[i].state = Held;
			buttons[i].lastState = Held;
			buttons[i].timerTriggered = 0;
		}
	}
}

void buttons_ExtiGpioCallback(Button* button, ButtonEmulateAction emulateAction)
{

	uint8_t interruptState = 0;
	uint32_t tickTime;

	// If the button press is a hardware pin interrupt event
	if(emulateAction == ButtonEmulateNone)
	{
		if(buttons_GetPinState(button) != 0)
		{
			// Check for physical logic state mode
			if(button->logicMode == ActiveLow)
			{
				interruptState = 1;	// Released
			}
			else
			{
				interruptState = 0;	// Pressed
			}

		}
		else
		{
			// Check for physical logic state mode
			if(button->logicMode == ActiveLow)
			{
				interruptState = 0;	// Pressed
			}
			else
			{
				interruptState = 1; // Released
			}
		}
	}
	else if(emulateAction == ButtonEmulatePress)
	{
		interruptState = 0;	// Pressed
	}
	else if(emulateAction == ButtonEmulateRelease)
	{
		interruptState = 1; // Released
	}

	// Debounce correct button and set handler flags to indicate an action
	#if FRAMEWORK_STM32CUBE
	tickTime = HAL_GetTick();
	#elif FRAMEWORK_ARDUINO
	tickTime = millis();
	#endif
	if((tickTime - button->lastTime) > DEBOUNCE_TIME)
	{
		// NEW PRESS
		// There is no need to check other conditions as time since release isn't important
		// A new press event should only be actioned after a release event for debouncing
		if(!interruptState && (button->lastState == Released || button->lastState == DoublePressReleased
										|| button->lastState == HeldReleased))
		{
			// Check to see if the timer has already been started (aka. another switch is already being held)
			// If it has, but the time since it was triggered is below the threshold, include that button in the timerTriggered flag
			if(timerConfigured)
			{
				if(!button->timerTriggered)
				{
					button->timerTriggered = 1;
					#if FRAMEWORK_STM32CUBE
					HAL_TIM_Base_Start_IT(holdTim);
					#elif FRAMEWORK_ARDUINO
					if(timerStartCallback != NULL)
						timerStartCallback();
					#endif
				}

				// Check if another switch was pressed around the same time, and set it's timerTriggered flag too
				// But don't start the timer as it was already started, and the first button should trigger the hold timer
				else if(tickTime <= MULTIPLE_BUTTON_TIME)
				{
					button->timerTriggered = 1;
				}
			}
			
			// Update states
			// Check the previous press time for a double press action
			if((tickTime - button->lastTime < DOUBLE_PRESS_TIME) && button->lastTime > 0)
			{
				button->state = DoublePressed;
				button->lastState = DoublePressed;
			}
			else
			{
				button->state = Pressed;
				button->lastState = Pressed;
			}
		}

		// NEW RELEASED //
		else if(button->lastState == Pressed || button->lastState == DoublePressed || button->lastState == Held)
		{
			if(button->lastState == Pressed)
			{
				if(timerConfigured)
				{
					#if FRAMEWORK_STM32CUBE
					HAL_TIM_Base_Stop_IT(holdTim);
					buttons_ResetTimerCounter();
					#elif FRAMEWORK_ARDUINO
					if(timerStopCallback != NULL)
						timerStopCallback();
					#endif
				}
				button->state = Released;
				button->lastState = Released;
				button->timerTriggered = 0;
				
			}
			else if(button->lastState == Held)
			{
				// Button was held, the hold event triggered, and then released
				// Hold timer doesn't need to be stopped as that was done in the timer callback
				button->state = HeldReleased;
				button->lastState = HeldReleased;
				button->accelerationThreshold = BUTTON_ACCELERATION_THRESHOLD;
				button->accelerationCounter = 0;
			}
			else if(button->lastState == DoublePressed)
			{
				if(timerConfigured)
				{
					#if FRAMEWORK_STM32CUBE
					HAL_TIM_Base_Stop_IT(holdTim);
					buttons_ResetTimerCounter();
					#elif FRAMEWORK_ARDUINO
					if(timerStopCallback != NULL)
						timerStopCallback();
					#endif
				}
				button->state = DoublePressReleased;
				button->lastState = DoublePressReleased;
				button->timerTriggered = 0;
			}
		}
		button->lastTime = tickTime;
	}
}


//-------------- PRIVATE FUNCTIONS --------------//
uint8_t buttons_GetPinState(Button* button)
{
#if MCU_CORE_RP2040
	return gpio_get(button->pin);
#elif MCU_CORE_STM32
	return HAL_GPIO_ReadPin(button->port, button->pin); 
#endif
}

void buttons_ResetTimerCounter()
{
#if FRAMEWORK_STM32CUBE
	__HAL_TIM_SET_COUNTER(holdTim, 0);
#endif
}


#ifdef __cplusplus
}
#endif