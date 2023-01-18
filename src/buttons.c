/*
 * buttons.c
 *
 *  Created on: 21Jun.,2019
 *      Author: samspencer
 */

#include "buttons.h"
#if FRAMEWORK == STM32CUBE
#include "tim.h"
#endif
#include <stdlib.h>

/* Helper Macros */
#define TRUE 1
#define FALSE 0
#define MULTIPLE_BUTTON_TIME 100

/* Button flag definitions */
#define CLEAR 0

uint16_t numButtons = 0;
volatile uint32_t timerTriggered = CLEAR; // holds the index of which switch triggered the hold
uint16_t buttonHoldTime;
#if FRAMEWORK == STM32CUBE
TIM_HandleTypeDef *holdTim = NULL;
#endif

volatile Button buttons[NUM_ALL_SWITCHES];

/*******PRIVATE FUNCTION PROTOTYPES*******/

/*******PUBLIC FUNCTIONS*******/

/**
 * @brief 	Configures hardware and software properties to use timer based hold functionality
 * 		This function should be called after buttons_init() and stm32 timer init config
 * 		Standard timer functions should already be configured
 * 		as this function only adjust the prescaler and timer period (count value)
 * @param 	timeHandle	handle for the timer to be used for hold events
 * @param	time		desired hold time in milliseconds (mS)
 * @retval ButtonErrorState
 */
/*
ButtonErrorState buttons_setHoldTimer(TIM_HandleTypeDef *timHandle, uint16_t time)
{
	// Check parameters
	if(timHandle == NULL)
	{
		return ButtonParamError;
	}
	holdTim = timHandle;

	// Calculate prescaler and period values based on CPU frequency
	// The prescaler is set so that the timer resolution is equal to a millisecond
	// This allows for easy setting of the Period directly in milliseconds
	//
	uint32_t cpuFreq = HAL_RCC_GetSysClockFreq();
	holdTim->Init.Prescaler = cpuFreq / 10000;				// This assumes the clock is in the MHz range
	holdTim->Init.Period = time*10;

	// Update timer instance with new timing values and clear the interrupt flag to prevent initial mis-fire (bug found previously)
	if (HAL_TIM_Base_Init(holdTim) != HAL_OK)
	{
		return ButtonHalError;
	}
	__HAL_TIM_CLEAR_FLAG(holdTim, TIM_IT_UPDATE);
	return ButtonOk;
}
*/

/**
 * @brief 	Assigns a new instance of the Button type to the allocated memory (done in init function)
 * @param 	GPIOx 		port of the connected switch
 * @param 	GPIO_Pin 	pin of the connected switch
 * @param 	mode		physical button type
 * @param 	name		string name of the button
 * @param	index		index for the button in the memory allocation
 * @param	funcp		pointer to the desired handler function to be called
 * @retval ButtonErrorState
 */
ButtonErrorState buttons_create(
#if FRAMEWORK == STM32CUBE
	 GPIO_TypeDef *GPIOx,
#endif
	 uint32_t GPIO_Pin, ButtonMode mode, ButtonLogic logicMode,
	 uint16_t index, void (*funcp)(ButtonState state))
{
	if (funcp == NULL)
	{
		return ButtonParamError;
	}
	numButtons++;
	// configure hardware settings
#if FRAMEWORK == STM32CUBE
	buttons[index].port = GPIOx;
#endif
	buttons[index].pin = GPIO_Pin;
	buttons[index].logicMode = logicMode;

	// assign default init values
	buttons[index].state = Cleared;
	buttons[index].lastState = Released;
	buttons[index].mode = mode;
	buttons[index].lastTime = 0;
	buttons[index].pressEvent = FALSE;

	// map handler function
	buttons[index].handler = funcp;

	buttons[index].accelerationThreshold = BUTTON_ACCELERATION_THRESHOLD;
	buttons[index].accelerationCounter = 0;
	return ButtonOk;
}

/**
 * @brief 	Allows for the button states to be modified by external functions
 * @param 	index	The index of the button that is being updated
 * 		state	The state that the button is being changed to
 * @retval ButtonErrorState
 */
ButtonErrorState buttons_setState(uint8_t index, ButtonState state)
{
	if (index >= numButtons)
	{
		return ButtonParamError;
	}
	buttons[index].state = state;
	return ButtonOk;
}

/**
 * @brief 	Allows for the button last states to be modified by external functions
 * @param 	index	The index of the button that is being updated
 * 		state	The state that the button is being changed to
 * @retval ButtonErrorState
 */
ButtonErrorState buttons_setLastState(uint8_t index, ButtonState state)
{
	if (index >= numButtons)
	{
		return ButtonParamError;
	}
	buttons[index].lastState = state;
	return ButtonOk;
}

/**
 * @brief 	Returns the button state of button[index]
 * 		This function has no parameter checking (*BE CAREFUL*)
 * 		This is because it is returning the button state
 * @param 	index	The index of the button that is being updated
 * @retval ButtonState	The state of button[index]
 */
ButtonState buttons_getState(uint8_t index)
{
	return buttons[index].state;
}

/**
 * @brief 	Returns the associated GPIO pin for the passed button index
 * @param 	index	The index of the button that is being checked
 * @retval GPIO Pin
 */
uint32_t buttons_getPin(uint16_t index)
{
	return buttons[index].pin;
}

/**
 * @brief 	Checks the state of every button if a new action has occurred
 * 		This function should be polled regularly in the main loop
 * @param 	none
 * @retval none
 */
void buttons_triggerPoll(void)
{
	// Find which button has a new event, execute the callback handler, then clear the state when finished
	// Note that the full loop is allowed to continue in case multiple interrupts were fired before polling
	for (int i = 0; i < numButtons; i++)
	{
		if (buttons[i].state != Cleared)
		{
			ButtonState tempState = buttons[i].state;
			buttons[i].state = Cleared;
			buttons[i].handler(tempState);
		}
		if (buttons[i].accelerationTrigger)
		{
			buttons[i].handler(HeldRepeat);
			buttons[i].accelerationTrigger = FALSE;
		}
	}
	return;
}

/*
void buttons_holdTimerElapsed()
{
	HAL_TIM_Base_Stop_IT(holdTim);
	// Check hold states for all the buttons before hold is actioned
	// This ensures multiple holds are all captured before being actioned
	for(int i=0; i<numButtons; i++)
	{
		// Check not only if the button has not been released, but if a timer event was triggered for that button
		if(buttons[i].lastState == Pressed && ((timerTriggered >> i) & 1))
		{
			buttons[i].state = Held;
			buttons[i].lastState = Held;
		}
	}
	timerTriggered = CLEAR;
}
*/

/*******PRIVATE FUNCTIONS*******/
/**
 * @brief	Called for events on the EXTI interrupt line
 * 		Check which pin was called, then determines the edge type
 * 		Results are recorded then checked for debouncing
 * 		buttons[].state is used to determine if a handler needs to be called
 * @param 	GPIO_Pin - Pin that the edge event was detected on
 * @retval none
 */

void buttons_extiGpioCallback(uint16_t index, ButtonEmulateAction emulateAction)
{

	// and whether it was a press or release action
	uint8_t interruptState = 0;
	uint32_t tickTime;

	// If the button press is a hardware pin interrupt event
	if (emulateAction == ButtonEmulateNone)
	{
		// There is no need to access the button pin as the parameter is used
#if FRAMEWORK == STM32CUBE
		if ((buttons[index].port->IDR & buttons[index].pin) != (uint32_t)GPIO_PIN_RESET)
#elif FRAMEWORK == ARDUINO
		if (digitalRead(buttons[index].pin) == LOW)
#endif
		{
			// Check for physical logic state mode
			if (buttons[index].logicMode == ActiveLow)
			{
				interruptState = 1; // Released
			}
			else
			{
				interruptState = 0; // Pressed
			}
		}
		else
		{
			// Check for physical logic state mode
			if (buttons[index].logicMode == ActiveLow)
			{
				interruptState = 0; // Pressed
			}
			else
			{
				interruptState = 1; // Released
			}
		}
	}
	else if (emulateAction == ButtonEmulatePress)
	{
		interruptState = 0; // Pressed
	}
	else if (emulateAction == ButtonEmulateRelease)
	{
		interruptState = 1; // Released
	}

	// Debounce correct button and set handler flags to indicate an action
	tickTime = millis();
	if ((tickTime - buttons[index].lastTime) > DEBOUNCE_TIME)
	{
		/* NEW PRESS */
		// There is no need to check other conditions as time since release isn't important
		if (!interruptState)
		{
			// Check to see if the timer has already been started (aka. another switch is already being held)
			// If it has, but the time since it was triggered is below the threshold, include that button in the timerTriggered flag
			/*
			if (holdTim != NULL)
			{
				if (timerTriggered == CLEAR)
				{
					timerTriggered |= (1 << index);
					HAL_TIM_Base_Start_IT(holdTim);
				}

				// Check if another switch was pressed around the same time, and set it's timerTriggered flag too
				// But don't start the timer as it was already started, and the first button should trigger the hold timer
				else if (holdTim->Instance->CNT <= MULTIPLE_BUTTON_TIME)
				{
					timerTriggered |= (1 << index);
				}
			}
			*/

			// Update states
			// Check the previous press time for a double press action
			if ((tickTime - buttons[index].lastTime < DOUBLE_PRESS_TIME) && buttons[index].lastTime > 0)
			{
				buttons[index].state = DoublePressed;
				buttons[index].lastState = DoublePressed;
			}
			else
			{
				buttons[index].state = Pressed;
				buttons[index].lastState = Pressed;
			}
		}

		/* NEW RELEASED */
		else
		{
			if (buttons[index].lastState == Pressed)
			{
				/*
				if (holdTim != NULL)
				{
					HAL_TIM_Base_Stop_IT(holdTim);
					__HAL_TIM_SET_COUNTER(holdTim, 0);
				}
				*/
				buttons[index].state = Released;
				buttons[index].lastState = Released;
				timerTriggered = CLEAR;
			}
			else if (buttons[index].lastState == Held)
			{
				// Button was held, the hold event triggered, and then released
				// Hold timer doesn't need to be stopped as that was done in the timer callback
				buttons[index].state = HeldReleased;
				buttons[index].lastState = HeldReleased;
				buttons[index].accelerationThreshold = BUTTON_ACCELERATION_THRESHOLD;
				buttons[index].accelerationCounter = 0;
			}
			else if (buttons[index].lastState == DoublePressed)
			{
				/*
				if (holdTim != NULL)
				{
					HAL_TIM_Base_Stop_IT(holdTim);
					__HAL_TIM_SET_COUNTER(holdTim, 0);
				}
				*/
				buttons[index].state = DoublePressReleased;
				buttons[index].lastState = DoublePressReleased;
				timerTriggered = CLEAR;
			}
		}
		buttons[index].lastTime = tickTime;
	}
}
