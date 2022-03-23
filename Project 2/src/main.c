/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
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

// Tasks
#define DD_SCHEDULER_PRIORITY 3
#define MONITOR_TASK_PRIORITY 2
#define DD_RUNNING_TASK_PRIORITY 1
#define DD_WAIT_TASK_PRIORITY 0

static void DD_Scheduler(void *pvParameters);
static void DD_Task_Generator_Callback_1(TimerHandle_t xTimer);
static void DD_Task_Generator_Callback_2(TimerHandle_t xTimer);
static void DD_Task_Generator_Callback_3(TimerHandle_t xTimer);
static void DD_Task_1(void *pvParameters);
static void DD_Task_2(void *pvParameters);
static void DD_Task_3(void *pvParameters);
static void Monitor_Task(void *pvParameters);

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

// Event Group
EventGroupHandle_t DD_Scheduler_Event_Group;

enum task_type {PERIODIC, APERIODIC};

struct dd_task {
	TaskHandle_t t_handle;
	enum task_type type;
	char task_id;
	char task_iteration;
	TickType_t release_time;
	TickType_t absolute_deadline;
	TickType_t completion_time;
};

struct dd_task_list {
	struct dd_task* task;
	struct dd_task_list* next_task;
};

void create_dd_task( TaskHandle_t t_handle, enum task_type type, uint32_t task_id, uint32_t absolute_deadline) {
	// create a dd_task
	// send the dd_task to queue
	// set bit 0 in scheduler event group
}

void delete_dd_task(uint32_t task_id) {
	// send task_id and task_iteration to queue
	// set bit 1 in scheduler event group
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

void append_to_list(struct dd_task_list* root, struct dd_task* node) {
	// convert dd_task to dd_task_list
	// dd_task_list new_node = malloc();
	// cur->next = &new_node;
	// append node to end of root
}

void insert_to_list(struct dd_task_list* root, struct dd_task* node) {
	// convert dd_task to dd_task_list
	// insert node into root where deadline fits
}

dd_task* remove_from_list(struct dd_task_list* root, char task_id_iteration[2]) {
	// find task with given id and iteration
	// set its next to NULL and set its previous to its next
}

/**
 * Sets the last character of the task name to the iteration we're at.
 */
void set_task_name(char* task_name, int iterations) {
	char c = '0';
	if (iterations > 10) {
		iterations += 7; // skip some ascii characters to get to A-Z after numbers
	}
	c += iterations;
	if (c > 'Z') {
		c = '0'; // roll back
	}

	task_name[sizeof(task_name)-1] = c; // set the last open
}

int main(void) {
	// create queues
	create_task_queue = xQueueCreate(100, sizeof(struct dd_task));

	delete_task_queue = xQueueCreate(100, sizeof(char)*2); // send two characters, the task id and task iteration.

	request_active_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_active_task_queue = xQueueCreate(1, sizeof(uint8_t));

	request_complete_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_complete_task_queue = xQueueCreate(1, sizeof(uint8_t));

	request_overdue_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_overdue_task_queue = xQueueCreate(1, sizeof(uint8_t));

	// create timers
	Task_1_Generator_Timer = xTimerCreate("Task_1_Generator_Timer", pdMS_TO_TICKS(TASK1_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_1);
	Task_2_Generator_Timer = xTimerCreate("Task_2_Generator_Timer", pdMS_TO_TICKS(TASK2_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_2);
	Task_3_Generator_Timer = xTimerCreate("Task_3_Generator_Timer", pdMS_TO_TICKS(TASK3_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_3);

	// create F tasks ( scheduler (highest priority), monitor (second highest priority) )
	xTaskCreate(DD_Scheduler, "DD_Scheduler", configMINIMAL_STACK_SIZE, NULL, DD_SCHEDULER_PRIORITY, NULL);
	xTaskCreate(Monitor_Task, "Monitor_Task", configMINIMAL_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, NULL);

	// create event group for scheduler function calls
	DD_Scheduler_Event_Group = xEventGroupCreate();

	// start timers
	xTimerStart(Task_1_Generator_Timer, 0);
	xTimerStart(Task_2_Generator_Timer, 0);
	xTimerStart(Task_3_Generator_Timer, 0);

	// start scheduler
	vTaskStartScheduler();

	return 0;
}

/**
 * Highest priority tasks and schedules current dd task in the lists
 */
static void DD_Scheduler(void *pvParameters) {
	// malloc active / completed / overdue lists
	struct dd_task_list active_task_list = {NULL, NULL};
	struct dd_task_list completed_task_list = {NULL, NULL};
	struct dd_task_list overdue_task_list = {NULL, NULL};

	uint8_t count_active_tasks = 0;
	uint8_t count_completed_tasks = 0;
	uint8_t count_overdue_tasks = 0;

	while(1) {
		// wait for one of the scheduler's function to be called
		xEventGroupWaitBits(DD_Scheduler_Event_Group, BIT_0, pdTRUE, pdFALSE, portMAX_DELAY);

		// set root of active list to default priority
		dd_task* root_task = active_task_list.task;

		if (root_task != NULL) {
			TaskHandle_t root_task_handle = root_task->t_handle;
			vTaskPrioritySet(root_task_handle, DD_WAIT_TASK_PRIORITY);
		}

		// non-blocking check for overdue tasks

		// non-blocking check completed queue
		char task_id_iteration[2];
		while (xQueueReceive(remove_task_queue, task_id_iteration, 0)) {
			// remove completed task from active queue and add to completed list
			dd_task* removed_task = remove_task_from_list(active_task_list, task_id_iteration);
			insert_to_list(completed_task_list, removed_task);
		}

		// non-blocking check release queue
		struct dd_task new_task;
		while (xQueueReceive(create_task_queue, new_task, 0)) {
			// put released tasks in active list
			// xTaskCreate(DD_Task, name, configMINIMAL_STACK_SIZE, NULL, DD_WAIT_TASK_PRIORITY, NULL); // TODO: send execution times through params
		}

		// set highest priority task's priority to active task priority

		// non-blocking check requests for active / completed / overdue
		// respond to requests

	}
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_1(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID

	TickType_t tick_count = xTaskGetTickCount();

	struct dd_task new_task;
	new_task.t_handle = DD_Task_1;
	new_task.type = PERIODIC;
	new_task.task_id = '1';
	new_task.task_iteration = (char) iteration + 48;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK1_PERIOD_MS);
	new_task.completion_time = 0;

	// place dd_task on create_task_queue
	xQueueSend(create_task_queue, &new_task, 0);
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_2(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID

	TickType_t tick_count = xTaskGetTickCount();

	struct dd_task new_task;
	new_task.t_handle = DD_Task_2;
	new_task.type = PERIODIC;
	new_task.task_id = '2';
	new_task.task_iteration = (char) iteration + 48;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK2_PERIOD_MS);
	new_task.completion_time = 0;

	// place dd_task on create_task_queue
	xQueueSend(create_task_queue, &new_task, 0);
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_3(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID

	TickType_t tick_count = xTaskGetTickCount();

	struct dd_task new_task;
	new_task.t_handle = DD_Task_3;
	new_task.type = PERIODIC;
	new_task.task_id = '3';
	new_task.task_iteration = iteration + 48;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK3_PERIOD_MS);
	new_task.completion_time = 0;

	// place dd_task on create_task_queue
	xQueueSend(create_task_queue, &new_task, 0);
}

static void DD_Task_1(void *pvParameters) {
	while (1) {
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK1_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
	}
}

static void DD_Task_2(void *pvParameters) {
	while (1) {
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK2_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
	}
}

static void DD_Task_3(void *pvParameters) {
	while (1) {
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK3_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
	}
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
