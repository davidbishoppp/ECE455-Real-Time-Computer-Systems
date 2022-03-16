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

#define TEST_BENCH_1 1
//#define TEST_BENCH_2 1
//#define TEST_BENCH_3 1

// Test Bench Defines
#ifdef TEST_BENCH_1
	#define TASK1_EXECUTION_MS 95
	#define TASK1_PERIOD_MS 500

	#define TASK2_EXECUTION_MS 150
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 250
	#define TASK3_PERIOD_MS 750
#endif

#ifdef TEST_BENCH_2
	#define TASK1_EXECUTION_MS 95
	#define TASK1_PERIOD_MS 250

	#define TASK2_EXECUTION_MS 150
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 250
	#define TASK3_PERIOD_MS 750
#endif

#ifdef TEST_BENCH_3
	#define TASK1_EXECUTION_MS 100
	#define TASK1_PERIOD_MS 500

	#define TASK2_EXECUTION_MS 200
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 200
	#define TASK3_PERIOD_MS 500
#endif

// Queues
QueueHandle_t create_task_queue;

QueueHandle_t delete_task_queue;

QueueHandle_t request_active_task_queue;
QueueHandle_t get_active_task_queue;

QueueHandle_t request_complete_task_queue;
QueueHandle_t get_complete_task_queue;

QueueHandle_t request_overdue_task_queue;
QueueHandle_t get_overdue_task_queue;

// Timers
TimerHandle_t Task_1_Generator_Timer;
TimerHandle_t Task_2_Generator_Timer;
TimerHandle_t Task_3_Generator_Timer;

enum task_type {PERIODIC, APERIODIC};

struct dd_task {
	TaskHandle_t t_handle;
	enum task_type type;
	uint32_t task_id;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
};

struct dd_task_list {
	struct dd_task task;
	struct dd_task_list* next_task;
};

void create_dd_task( TaskHandle_t t_handle, enum task_type type, uint32_t task_id, uint32_t absolute_deadline) {
	// create a dd_task
	// send the dd_task to queue
}

void delete_dd_task(uint32_t task_id) {
	// send task_id to queue
}

const struct dd_task_list* get_active_dd_task_list(void) {
	// send request to queue
	// receive pointer to root
}

const struct dd_task_list* get_complete_dd_task_list(void) {
	// send request to queue
	// receive pointer to root
}

const struct dd_task_list* get_overdue_dd_task_list(void) {
	// send request to queue
	// receive pointer to root
}

void append_to_list(struct dd_task_list* root, struct dd_task node) {
	// convert dd_task to dd_task_list
	// dd_task_list new_node = malloc();
	// cur->next = &new_node;
	// append node to end of root
}

void insert_to_list(struct dd_task_list* root, struct dd_task node) {
	// convert dd_task to dd_task_list
	// insert node into root where deadline fits
}

int main(void) {
	// create queues
	// create timers
	// create F tasks ( scheduler (highest priority), monitor (second highest priority) )
	// start timers
	// start scheduler
	return 0;
}

static void DD_Scheduler(void *pvParameters) {
	/**
	 * Highest priority tasks and schedules current dd task in the lists
	 */

	// malloc active / completed / overdue lists
	struct dd_task_list* active_task_list; // = malloc...
	struct dd_task_list* completed_task_list; // = malloc...
	struct dd_task_list* overdue_task_list; // = malloc...

	while(1) {
		// non-blocking check completed queue
		// remove all completed tasks from active queue to completed list
		//
		// non-blocking check release queue
		// reset highest priority task's priority
		// put released tasks in active list
		//
		// set task's priority to 1 below scheduler
		//
		// non-blocking check requests for active / completed / overdue
		// respond to requests
		//
		// task delay the scheduler for some # of seconds/ticks
	}
}


static void DD_Task_Generator_Callback_1(TimerHandle_t xTimer) {
	/**
	 * timer call back to create new dd tasks from user defined
	 */
	// *never block*
	// on each call create a new dd_task
	// place dd_task on create_task_queue
}

static void DD_Task_1(void *pvParameters) {
	/**
	 * get execution time and inforamtion from parameters
	 * while (1) {
	 * 	start = get_ticks();
	 * 	while (cur - start < wait_time);
	 * 	delete_dd_task(task_id);
	 * }
	 */
}

static void Monitor_Task(void *pvParameters) {
	/**
	 * monitor other tasks to report missed deadlines
	 */
	// request active / complete / overdue lists
	// wait for receiving lists
	// flush active / complete / overdue lists to file ( style out put for debug )
	// free (free all nodes, set head to null) complete / overdue list
	// Track processor usage?
	//
	// task delay the monitor for some # of seconds/ticks

}

//static void

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
