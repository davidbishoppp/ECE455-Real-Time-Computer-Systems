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

#define MAX_NUM_CARS 19

#define FLOW_POLL_PERIOD_MS 100
#define LIGHTS_POLL_PERIOD_MS 250
#define TRAFFIC_POLL_PERIOD_MS 1000
#define DISPLAY_POLL_PERIOD_MS 1000

#define YELLOW_LIGHT_DURATION_MS 1000

enum light_color {
	green = GPIO_Pin_2,
	yellow = GPIO_Pin_1,
	red = GPIO_Pin_0
};

int elapsed_tick_count = 0;
enum light_color light_status = red;

#define Data	GPIO_Pin_6
#define Clock	GPIO_Pin_7
#define Reset	GPIO_Pin_8

// Prototypes
#define Flow_Adjustment_Task_Priority 0
static void Flow_Adjustment_Task(void *pvParameters);

#define Generator_Task_Priority 1
static void Generator_Task(void *pvParameters);

#define Light_State_Task_Priority 1
static void Light_State_Task(void *pvParameters);
static void Light_State_Timer_Callback(TimerHandle_t xTimer);

#define Display_Task_Priority 2
static void Display_Task(void *pvParameters);

// Queues
xQueueHandle flowToLightQueueHandle = 0;
xQueueHandle flowToTrafficQueueHandle = 0;
xQueueHandle trafficToDisplayQueueHandle = 0;
xQueueHandle lightToDisplayQueueHandle = 0;

// Timers
TimerHandle_t Lights_Timer = 0;

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

uint16_t Get_ADC_Reading() {
	ADC_SoftwareStartConv(ADC1);
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	uint16_t adc_val = ADC_GetConversionValue(ADC1);
	adc_val = (adc_val + 2 - 52) / 39;
	return (adc_val > 100) ? 100 : adc_val;
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

	// Create Queues inter-task communication, declared as global variables above.
	flowToTrafficQueueHandle = xQueueCreate(1, sizeof( uint16_t ));
	flowToLightQueueHandle = xQueueCreate(1, sizeof(uint16_t));
	trafficToDisplayQueueHandle = xQueueCreate(1, sizeof(uint16_t));
	lightToDisplayQueueHandle = xQueueCreate(1, sizeof(enum light_color));

	// Add queues to the registry
	vQueueAddToRegistry(flowToTrafficQueueHandle, "FlowToTrafficQueue");
	vQueueAddToRegistry(flowToLightQueueHandle, "FlowToLightsQueue");
	vQueueAddToRegistry(trafficToDisplayQueueHandle, "TrafficToDisplayQueue");
	vQueueAddToRegistry(lightToDisplayQueueHandle, "LightsToDisplayQueue");

	xTaskCreate( Flow_Adjustment_Task, "FlowAdjustment", configMINIMAL_STACK_SIZE, NULL, Flow_Adjustment_Task_Priority, NULL);
	xTaskCreate( Generator_Task, "Generator", configMINIMAL_STACK_SIZE, NULL, Generator_Task_Priority, NULL);
	//xTaskCreate( Light_State_Task, "LightState", configMINIMAL_STACK_SIZE, NULL, Light_State_Task_Priority, NULL);
	xTaskCreate( Display_Task, "Display", configMINIMAL_STACK_SIZE, NULL, Display_Task_Priority, NULL);
	Lights_Timer = xTimerCreate("LightsStateTimer", pdMS_TO_TICKS(LIGHTS_POLL_PERIOD_MS), pdTRUE, 0, Light_State_Timer_Callback);

	// Start the tasks and timer running.
	xTimerStart(Lights_Timer, 0);
	vTaskStartScheduler();

	return 0;
}

static void Flow_Adjustment_Task(void *pvParameters) {
	TickType_t last_wake_time = xTaskGetTickCount();
	//printf("Flow Adjustment Task!\n");
	while(1) {
		uint16_t adc_reading = Get_ADC_Reading();
		//printf("ADC Reading: %i\n", adc_reading);
		xQueueOverwrite(flowToLightQueueHandle, &adc_reading);
		xQueueOverwrite(flowToTrafficQueueHandle, &adc_reading);
		vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(FLOW_POLL_PERIOD_MS));
	}
}

static void Generator_Task(void *pvParameters) {
	uint16_t set = 0;
	uint16_t flow_val = 0;
	TickType_t last_wake_time = xTaskGetTickCount();
	int elapsed_tick_count = 0;
	int elapsed_duration_ms = 0;
	int time_till_next_car_ms = 0;

	while(1) {

		set = 0;
		elapsed_duration_ms = elapsed_tick_count * TRAFFIC_POLL_PERIOD_MS;

		if (xQueueReceive(flowToTrafficQueueHandle, &flow_val, 200)) {
			time_till_next_car_ms = 6000 - 50 * flow_val;
			if (elapsed_duration_ms >= time_till_next_car_ms) {
				elapsed_tick_count = 0;
				set = 1;
			}
			//printf("Elapsed duration (ms) : %i\n", elapsed_duration_ms);
			//printf("Time till next car (ms): %i\n", time_till_next_car_ms);
			//printf("Time to add new car? %c\n", (set == 1) ? 'Y' : 'N');
		}

		xQueueOverwrite(trafficToDisplayQueueHandle, &set);
		vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(TRAFFIC_POLL_PERIOD_MS));
		elapsed_tick_count++;
	}
}

static void Light_State_Task(void *pvParameters) {
	enum light_color light_status = red;
	uint16_t flow_val = 0;
	TickType_t last_wake_time = xTaskGetTickCount();
	int elapsed_tick_count = 0;
	int elapsed_duration_ms = 0;
	int total_duration_ms = 0;

	while(1) {

		elapsed_duration_ms = elapsed_tick_count * LIGHTS_POLL_PERIOD_MS;

		if (xQueueReceive(flowToLightQueueHandle, &flow_val, 200)) {
			if (light_status == red) {
				total_duration_ms = 5000 - 20 * flow_val;
				if (elapsed_duration_ms >= total_duration_ms) {
					elapsed_tick_count = 0;
					light_status = green;
				}
			} else if (light_status == green) {
				total_duration_ms = 2500 + 35 * flow_val;
				if (elapsed_duration_ms >= total_duration_ms) {
					elapsed_tick_count = 0;
					light_status = yellow;
				}
			} else if (light_status == yellow) {
				if (elapsed_duration_ms >= YELLOW_LIGHT_DURATION_MS) {
					elapsed_tick_count = 0;
					light_status = red;
				}
			}
		}

		xQueueOverwrite(lightToDisplayQueueHandle, &light_status);
		vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(LIGHTS_POLL_PERIOD_MS));
		elapsed_tick_count++;
	}
}


static void Light_State_Timer_Callback(TimerHandle_t xTimer) {
	uint16_t flow_val = 0;
	int elapsed_duration_ms = 0;
	int total_duration_ms = 0;

	elapsed_duration_ms = elapsed_tick_count * LIGHTS_POLL_PERIOD_MS;

	if (xQueueReceive(flowToLightQueueHandle, &flow_val, 200)) {
		if (light_status == red) {
			total_duration_ms = 5000 - 20 * flow_val;
			if (elapsed_duration_ms >= total_duration_ms) {
				elapsed_tick_count = 0;
				light_status = green;
			}
		} else if (light_status == green) {
			total_duration_ms = 2500 + 35 * flow_val;
			if (elapsed_duration_ms >= total_duration_ms) {
				elapsed_tick_count = 0;
				light_status = yellow;
			}
		} else if (light_status == yellow) {
			if (elapsed_duration_ms >= YELLOW_LIGHT_DURATION_MS) {
				elapsed_tick_count = 0;
				light_status = red;
			}
		}
	}

	xQueueOverwrite(lightToDisplayQueueHandle, &light_status);
	elapsed_tick_count++;
}

static void Display_Task(void *pvParameters) {
	enum light_color light_status = red;
	TickType_t last_wake_time = xTaskGetTickCount();
	int traffic = 0;
	uint16_t addCar = 1;
	while(1) {
		//printf("Display Task!\n");
		xQueueReceive(lightToDisplayQueueHandle, &light_status, 0); // get new light status if there is one
		set_light(light_status);

		xQueueReceive(trafficToDisplayQueueHandle, &addCar, 0);

		//printf("Display adding car? %c\n", (addCar != 0) ? 'Y' : 'N');

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
			addCar = 0;
		}
		move_traffic(traffic); // Set traffic LEDs
		vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(DISPLAY_POLL_PERIOD_MS));
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
