/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"

/*-----------------------------------------------------------*/
#define QUEUE_LENGTH 100

#define MAX_NUM_CARS 19

#define REFRESH_TIME_MS 1000

enum light_color {
	green = GPIO_Pin_2,
	yellow = GPIO_Pin_1,
	red = GPIO_Pin_0
};

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6

#define Data	GPIO_Pin_6
#define Clock	GPIO_Pin_7
#define Reset	GPIO_Pin_8

// Prototypes
#define Flow_Adjustment_Task_Priority 1
static void Flow_Adjustment_Task(void *pvParameters);

#define Generator_Task_Priority 1
static void Generator_Task(void *pvParameters);

#define Light_State_Task_Priority 1
static void Light_State_Task(void *pvParameters);

#define Display_Task_Priority 1
static void Display_Task(void *pvParameters);

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
void Manager_Task( void *pvParameters );
void Blue_LED_Controller_Task( void *pvParameters );
void Green_LED_Controller_Task( void *pvParameters );
void Red_LED_Controller_Task( void *pvParameters );
void Amber_LED_Controller_Task( void *pvParameters );

// Queues
xQueueHandle ADC_Queue_Handle = 0;
xQueueHandle Traffic_Queue_Handle = 0;
xQueueHandle Light_Queue_Handle = 0;

// Timers
TimerHandle_t Display_Timer = 0;

void GPIO_Setup() {
	NVIC_SetPriorityGrouping(0);

	// Enable AHB1 Clock
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	// Struct to IN GPIOs
	GPIO_InitTypeDef GPIO_Struct;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2; // LED GPIO
	GPIO_Struct.GPIO_Pin |= GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8; // Shift Register GPIO
	GPIO_Struct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Struct.GPIO_OType = GPIO_OType_PP;
	GPIO_Struct.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOC, &GPIO_Struct);

	// Struct to init Analog GPIO
	GPIO_InitTypeDef GPIO_Struct_AN;
	GPIO_Struct_AN.GPIO_Pin = GPIO_Pin_3;
	GPIO_Struct_AN.GPIO_Mode = GPIO_Mode_AN;
	GPIO_Struct_AN.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOC, &GPIO_Struct_AN);

	GPIO_SetBits(GPIOC, Reset);
	GPIO_SetBits(GPIOC, Clock);
	GPIO_SetBits(GPIOC, Data);
}

void ADC_Setup() {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	ADC_InitTypeDef ADC_Struct;
	ADC_Struct.ADC_Resolution = ADC_Resolution_12b;
	ADC_Struct.ADC_ScanConvMode = DISABLE;
	ADC_Struct.ADC_ContinuousConvMode = DISABLE;
	ADC_Struct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;
	ADC_Struct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_Struct.ADC_DataAlign = ADC_DataAlign_Right;

	ADC_Init(ADC1, &ADC_Struct);
	ADC_Cmd(ADC1, ENABLE);
	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_84Cycles);
}

/**
 * Gets ADC value in a percentage of 1-6
 */
uint16_t Get_ADC_Percent() {
	ADC_SoftwareStartConv(ADC1);
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	return (ADC_GetConversionValue(ADC1) * 6) >> 13; // shift 13 == divide by 4096
}

/**
 * Sets the clock and data pins of SR to move traffic and add a car
 */
void set_traffic() {
	GPIO_SetBits(GPIOC, Data);
	GPIO_ResetBits(GPIOC, Clock);
	GPIO_SetBits(GPIOC, Clock);
}

/**
 * Sets the clock and data pins of SR to move traffic
 */
void unset_traffic() {
	GPIO_ResetBits(GPIOC, Data);
	GPIO_ResetBits(GPIOC, Clock);
	GPIO_SetBits(GPIOC, Clock);
}

/**
 * Moves the traffic LEDs based on the provided traffic, car and light status
 */
void move_traffic(int traffic) {
	// Iterate over traffic bits
	for (int i = MAX_NUM_CARS; i; i--) {
		// mask the bit we want
		if (traffic & 1) {
			set_traffic();
		} else {
			unset_traffic();
		}
		traffic >>= 1;
	}
}

void reset_traffic() {
	GPIO_ResetBits(GPIOC, Reset);
	GPIO_SetBits(GPIOC, Reset);
}

/**
 * TODO: is enum of for this????
 */
void set_light(enum light_color light_status) {
	GPIO_ResetBits(GPIOC, green | yellow | red);
	GPIO_SetBits(GPIOC, light_status);
}

/*--------------------------TESTING--------------------------*/

void LED_on() {
	GPIO_SetBits(GPIOC, GPIO_Pin_0);
	GPIO_SetBits(GPIOC, GPIO_Pin_1);
	GPIO_SetBits(GPIOC, GPIO_Pin_2);
}

void LED_off() {
	GPIO_ResetBits(GPIOC, GPIO_Pin_0);
	GPIO_ResetBits(GPIOC, GPIO_Pin_1);
	GPIO_ResetBits(GPIOC, GPIO_Pin_2);
}

void test() {
	int traffic = 0b1010101010101;
	move_traffic(traffic);
	return;

	int i = 0;
	bool addCar = false;
	while (1) {
		//LED_on();
		printf("value: %i\n", Get_ADC_Percent());
		//LED_off();
		continue;
		move_traffic(addCar);
		addCar = !addCar;
		i++;
		if (i >= 12) {
			reset_traffic();
			i = 0;
		}
	}
}
/*-----------------------------------------------------------*/

int main(void) {

	GPIO_Setup();
	ADC_Setup();

	// Create Queues for ADC and Traffic values, declared as global variables above.
	ADC_Queue_Handle = xQueueCreate(QUEUE_LENGTH, sizeof( uint16_t ));
	Traffic_Queue_Handle = xQueueCreate(QUEUE_LENGTH, sizeof(uint32_t));
	Light_Queue_Handle = xQueueCreate(QUEUE_LENGTH, sizeof(enum light_color));

	// Add queues to the registry
	vQueueAddToRegistry(ADC_Queue_Handle, "ADCQueue");
	vQueueAddToRegistry(Traffic_Queue_Handle, "TrafficQueue");
	vQueueAddToRegistry(Light_Queue_Handle, "LightQueue");

	xTaskCreate( Flow_Adjustment_Task, "FlowAdjustment", configMINIMAL_STACK_SIZE, NULL, Flow_Adjustment_Task_Priority, NULL);
	xTaskCreate( Generator_Task, "Generator", configMINIMAL_STACK_SIZE, NULL, Generator_Task_Priority, NULL);
	xTaskCreate( Light_State_Task, "LightState", configMINIMAL_STACK_SIZE, NULL, Light_State_Task_Priority, NULL);
	xTaskCreate( Display_Task, "Display", configMINIMAL_STACK_SIZE, NULL, Display_Task_Priority, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}

/**
 * TODO: make event group to that flow has been put on a queue, place flow on queue if flag for traffic generator and/or light system are off
 */
static void Flow_Adjustment_Task(void *pvParameters) {
	printf("Flow Adjustment Task!\n");
	while(1) {
		uint16_t pot_value = Get_ADC_Percent();
		xQueueSend(ADC_Queue_Handle, &pot_value, 1000);
		xQueueSend(ADC_Queue_Handle, &pot_value, 1000);
		vTaskDelay(pdMS_TO_TICKS(REFRESH_TIME_MS));
	}
}

/**
 * TODO: if flow event flag is not set dont need to check queue
 * make a event group for display t read if generator data is on queue
 */
static void Generator_Task(void *pvParameters) {
	int calls = 0;
	while(1) {
		printf("Generator Task!\n");
		// Get ADC percentage
		uint16_t pot_value;
		if (xQueueReceive(ADC_Queue_Handle, &pot_value, 200)) {
			// TODO make actual probability
			if (calls >= pot_value) {
				calls = 0;
				uint8_t set = 1;
				xQueueSend(Traffic_Queue_Handle, &set, 500);
			}
			calls++;
		}
		vTaskDelay(pdMS_TO_TICKS(REFRESH_TIME_MS));
	}
}

/**
 * TODO: read event group to see if flow data is on queue for light state
 * change light timer time depending on flow data
 */
static void Light_State_Task(void *pvParameters) {
	enum light_color light_status = green;
	while(1) {
		printf("Light State Task!\n");
		if (light_status == green) {
			light_status = red;
		} else {
			light_status = green;
		}
		xQueueSend(Light_Queue_Handle, &light_status, 500);
		vTaskDelay(pdMS_TO_TICKS(REFRESH_TIME_MS*4));
	}
}

/**
 * TODO: watch event group and add new traffic or change light if even flags are set.
 */
static void Display_Task(void *pvParameters) {
	enum light_color light_status = red;
	int traffic = 0;
	while(1) {
		printf("Display Task!\n");
		xQueueReceive(Light_Queue_Handle, &light_status, 200); // get new light status if there is one
		set_light(light_status);

		int addCar = 0;
		xQueueReceive(Traffic_Queue_Handle, &addCar, 200);

		traffic >>= 1; // Move all traffic by 1
		if (light_status != green) { // If the light isn't green we move the car that pasted the line back to the next available spot
			if (traffic & 0x400) {
				traffic = traffic & ~0x400;
				for (int i = 11; i < MAX_NUM_CARS; i++) {
					if ( !(traffic & (1 << i)) ) {
						traffic |= (1 << i);
						break;
					}
				}
			}
		}
		if (addCar) {
			traffic |= (1 << MAX_NUM_CARS);
		}
		move_traffic(traffic); // Set traffic LEDs
		vTaskDelay(pdMS_TO_TICKS(REFRESH_TIME_MS));
	}
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software 
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
