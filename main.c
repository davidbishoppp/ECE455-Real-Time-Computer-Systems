

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

#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

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
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
void Manager_Task( void *pvParameters );
void Blue_LED_Controller_Task( void *pvParameters );
void Green_LED_Controller_Task( void *pvParameters );
void Red_LED_Controller_Task( void *pvParameters );
void Amber_LED_Controller_Task( void *pvParameters );

xQueueHandle ADC_Queue_Handle = 0;
xQueueHandle Traffic_Queue_Handle = 0;
xQueueHandle Light_Queue_Handle = 0;

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

uint16_t Get_ADC_Percent() {
	ADC_SoftwareStartConv(ADC1);
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	return (ADC_GetConversionValue(ADC1) * 6) / 4100;
}

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

void move_traffic(bool addCar) {
	GPIO_ResetBits(GPIOC, Clock);
	GPIO_ResetBits(GPIOC, Data);
	if (addCar) {
		GPIO_SetBits(GPIOC, Data);
	}
	GPIO_SetBits(GPIOC, Clock);
}

void reset_traffic() {
	GPIO_ResetBits(GPIOC, Reset);
	GPIO_SetBits(GPIOC, Reset);
}

void test() {
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
	Traffic_Queue_Handle = xQueueCreate(QUEUE_LENGTH, sizeof(uint8_t));
	Light_Queue_Handle = xQueueCreate(QUEUE_LENGTH, sizeof(uint8_t));

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

static void Flow_Adjustment_Task(void *pvParameters) {
	printf("Flow Adjustment Task!\n");
	while(1) {
		uint16_t pot_value = Get_ADC_Percent();
		xQueueSend(ADC_Queue_Handle, &pot_value, 1000);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

static void Generator_Task(void *pvParameters) {
	int calls = 0;
	while(1) {
		printf("Generator Task!\n");
		// Get ADC percentage
		uint16_t pot_value;
		if (xQueueReceive(ADC_Queue_Handle, &pot_value, 500)) {
			// TODO make actual probablity
			if (calls >= pot_value) {
				calls = 0;
				uint8_t set = 1;
				xQueueSend(Traffic_Queue_Handle, &set, 1000);
			}
			calls++;
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

static void Light_State_Task(void *pvParameters) {
	while(1) {
		printf("Light State Task!\n");
		vTaskDelay(1000);
	}
}

static void Display_Task(void *pvParameters) {
	uint32_t traffic = 0;
	while(1) {
		printf("Display Task!\n");
		uint8_t traffic_light;
		if (xQueueReceive(Traffic_Queue_Handle, &traffic_light, 1000)) {

		}
		vTaskDelay(1000);
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
/*-----------------------------------------------------------*/

static void prvSetupHardware( void )
{
	/* Ensure all priority bits are assigned as preemption priority bits.
	http://www.freertos.org/RTOS-Cortex-M3-M4.html */


	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}
