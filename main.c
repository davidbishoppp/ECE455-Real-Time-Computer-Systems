

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"



/*-----------------------------------------------------------*/
#define mainQUEUE_LENGTH 100

#define amber  	0
#define green  	1
#define red  	2
#define blue  	3

#define amber_led	LED3
#define green_led	LED4
#define red_led		LED5
#define blue_led	LED6


/*
 * TODO: Implement this function for any hardware specific clock configuration
 * that was not already performed before main() was called.
 */
static void prvSetupHardware( void );

/*
 * The queue send and receive tasks as described in the comments at the top of
 * this file.
 */
static void Manager_Task( void *pvParameters );
static void Blue_LED_Controller_Task( void *pvParameters );
static void Green_LED_Controller_Task( void *pvParameters );
static void Red_LED_Controller_Task( void *pvParameters );
static void Amber_LED_Controller_Task( void *pvParameters );

xQueueHandle xQueue_handle = 0;

void GPIO_Setup() {
	NVIC_SetPriorityGrouping(0);

	// Enable AHB1 Clock
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

	// Struct to IN GPIOs
	GPIO_InitTypeDef GPIO_Struct;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
	GPIO_Struct.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Struct.GPIO_OType = GPIO_OType_PP;
	GPIO_Struct.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOC, &GPIO_Struct);

	// Struct to init Analog GPIO
	GPIO_InitTypeDef GPIO_Struct_AN;
	GPIO_Struct.GPIO_Pin = GPIO_Pin_3;
	GPIO_Struct.GPIO_Mode = GPIO_Mode_AN;
	GPIO_Struct.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_Struct.GPIO_OType = GPIO_OType_PP;
	GPIO_Struct.GPIO_PuPd = GPIO_PuPd_NOPULL;

	GPIO_Init(GPIOC, &GPIO_Struct_AN);
}

void ADC_Setup() {
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

	ADC_InitTypeDef ADC_Struct;
	ADC_Struct.ADC_Resolution = ADC_Resolution_12b;
	ADC_Struct.ADC_ScanConvMode = DISABLE;
	ADC_Struct.ADC_ContinuousConvMode = ENABLE;
	ADC_Struct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
	ADC_Struct.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_Struct.ADC_NbrOfConversion = 1;

	ADC_Init(ADC1, &ADC_Struct);

	ADC_CommonInitTypeDef ADC_Common_Struct;
	ADC_Common_Struct.ADC_Mode = ADC_Mode_Independent;
	ADC_Common_Struct.ADC_Prescaler = ADC_Prescaler_Div2;
	ADC_Common_Struct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;
	ADC_Common_Struct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;

	ADC_CommonInit(&ADC_Common_Struct);



	ADC_RegularChannelConfig(ADC1, ADC_Channel_13, 1, ADC_SampleTime_480Cycles);


	ADC_Cmd(ADC1, ENABLE);
}

uint16_t Get_ADC() {
	ADC_SoftwareStartConv(ADC1);
	while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
	return ADC_GetConversionValue(ADC1);
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
/*-----------------------------------------------------------*/

int main(void)
{

	GPIO_Setup();
	ADC_Setup();

	while (1) {
		LED_on();
		printf("value: %i\n", Get_ADC());
		LED_off();
	}

	return 0;

	/* Initialize LEDs */
	STM_EVAL_LEDInit(amber_led);
	STM_EVAL_LEDInit(green_led);
	STM_EVAL_LEDInit(red_led);
	STM_EVAL_LEDInit(blue_led);

	/* Configure the system ready to run the demo.  The clock configuration
	can be done here if it was not done before main() was called. */
	prvSetupHardware();


	/* Create the queue used by the queue send and queue receive tasks.
	http://www.freertos.org/a00116.html */
	xQueue_handle = xQueueCreate( 	mainQUEUE_LENGTH,		/* The number of items the queue can hold. */
							sizeof( uint16_t ) );	/* The size of each item the queue holds. */

	/* Add to the registry, for the benefit of kernel aware debugging. */
	vQueueAddToRegistry( xQueue_handle, "MainQueue" );

	xTaskCreate( Manager_Task, "Manager", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
	xTaskCreate( Blue_LED_Controller_Task, "Blue_LED", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate( Red_LED_Controller_Task, "Red_LED", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate( Green_LED_Controller_Task, "Green_LED", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
	xTaskCreate( Amber_LED_Controller_Task, "Amber_LED", configMINIMAL_STACK_SIZE, NULL, 1, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	return 0;
}


/*-----------------------------------------------------------*/

static void Manager_Task( void *pvParameters )
{
	uint16_t tx_data = amber;


	while(1)
	{

		if(tx_data == amber)
			STM_EVAL_LEDOn(amber_led);
		if(tx_data == green)
			STM_EVAL_LEDOn(green_led);
		if(tx_data == red)
			STM_EVAL_LEDOn(red_led);
		if(tx_data == blue)
			STM_EVAL_LEDOn(blue_led);

		if( xQueueSend(xQueue_handle,&tx_data,1000))
		{
			printf("Manager: %u ON!\n", tx_data);
			if(++tx_data == 4)
				tx_data = 0;
			vTaskDelay(1000);
		}
		else
		{
			printf("Manager Failed!\n");
		}
	}
}

/*-----------------------------------------------------------*/

static void Blue_LED_Controller_Task( void *pvParameters )
{
	uint16_t rx_data;
	while(1)
	{
		if(xQueueReceive(xQueue_handle, &rx_data, 500))
		{
			if(rx_data == blue)
			{
				vTaskDelay(250);
				STM_EVAL_LEDOff(blue_led);
				printf("Blue Off.\n");
			}
			else
			{
				if( xQueueSend(xQueue_handle,&rx_data,1000))
					{
						printf("BlueTask GRP (%u).\n", rx_data); // Got wwrong Package
						vTaskDelay(500);
					}
			}
		}
	}
}


/*-----------------------------------------------------------*/

static void Green_LED_Controller_Task( void *pvParameters )
{
	uint16_t rx_data;
	while(1)
	{
		if(xQueueReceive(xQueue_handle, &rx_data, 500))
		{
			if(rx_data == green)
			{
				vTaskDelay(250);
				STM_EVAL_LEDOff(green_led);
				printf("Green Off.\n");
			}
			else
			{
				if( xQueueSend(xQueue_handle,&rx_data,1000))
					{
						printf("GreenTask GRP (%u).\n", rx_data); // Got wrong Package
						vTaskDelay(500);
					}
			}
		}
	}
}

/*-----------------------------------------------------------*/

static void Red_LED_Controller_Task( void *pvParameters )
{
	uint16_t rx_data;
	while(1)
	{
		if(xQueueReceive(xQueue_handle, &rx_data, 500))
		{
			if(rx_data == red)
			{
				vTaskDelay(250);
				STM_EVAL_LEDOff(red_led);
				printf("Red off.\n");
			}
			else
			{
				if( xQueueSend(xQueue_handle,&rx_data,1000))
					{
						printf("RedTask GRP (%u).\n", rx_data); // Got wrong Package
						vTaskDelay(500);
					}
			}
		}
	}
}


/*-----------------------------------------------------------*/

static void Amber_LED_Controller_Task( void *pvParameters )
{
	uint16_t rx_data;
	while(1)
	{
		if(xQueueReceive(xQueue_handle, &rx_data, 500))
		{
			if(rx_data == amber)
			{
				vTaskDelay(250);
				STM_EVAL_LEDOff(amber_led);
				printf("Amber Off.\n");
			}
			else
			{
				if( xQueueSend(xQueue_handle,&rx_data,1000))
					{
						printf("AmberTask GRP (%u).\n", rx_data); // Got wrong Package
						vTaskDelay(500);
					}
			}
		}
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
	NVIC_SetPriorityGrouping( 0 );

	/* TODO: Setup the clocks, etc. here, if they were not configured before
	main() was called. */
}

