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
#include "../FreeRTOS_Source/include/event_groups.h"

/*-----------------------------------------------------------*/

#define DEBUG 0

//#define TEST_BENCH_1 1
#define TEST_BENCH_2 1
//#define TEST_BENCH_3 1

// Test Bench Defines
#ifdef TEST_BENCH_1
	#define TASK1_EXECUTION_MS 95
	#define TASK1_PERIOD_MS 500

	#define TASK2_EXECUTION_MS 150
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 250
	#define TASK3_PERIOD_MS 750

	#define HYPER_PERIOD_MS 1500
#endif

#ifdef TEST_BENCH_2
	#define TASK1_EXECUTION_MS 95
	#define TASK1_PERIOD_MS 250

	#define TASK2_EXECUTION_MS 150
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 250
	#define TASK3_PERIOD_MS 750

	#define HYPER_PERIOD_MS 1500
#endif

#ifdef TEST_BENCH_3
	#define TASK1_EXECUTION_MS 100
	#define TASK1_PERIOD_MS 500

	#define TASK2_EXECUTION_MS 200
	#define TASK2_PERIOD_MS 500

	#define TASK3_EXECUTION_MS 200
	#define TASK3_PERIOD_MS 500

	#define HYPER_PERIOD_MS 1500
#endif

// Tasks
#define DD_SCHEDULER_PRIORITY 4
#define MONITOR_TASK_PRIORITY 1
#define MONITOR_TASK_PRIORITY_PRINTING 6
#define DD_RUNNING_TASK_PRIORITY 2
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

QueueHandle_t event_queue;

// Timers
TimerHandle_t Task_1_Generator_Timer;
TimerHandle_t Task_2_Generator_Timer;
TimerHandle_t Task_3_Generator_Timer;

TimerHandle_t Monitor_Task_Timer;

enum task_type {PERIODIC, APERIODIC};

struct dd_task {
	TaskHandle_t t_handle;
	enum task_type type;
	uint32_t id;
	uint32_t iteration;
	char name[13];
	TickType_t release_time;
	TickType_t absolute_deadline;
	TickType_t completion_time;
};

struct dd_task_list {
	struct dd_task task;
	struct dd_task_list* next_task;
};

void create_dd_task(enum task_type type, uint32_t id, uint32_t iteration, char* task_name, TickType_t absolute_deadline) {
	// create a dd_task
	struct dd_task new_task;
	new_task.t_handle = NULL;
	new_task.type = type;
	new_task.id = id;
	sprintf(new_task.name, task_name);
	new_task.iteration = iteration;
	new_task.release_time = xTaskGetTickCount();
	new_task.absolute_deadline = absolute_deadline;
	new_task.completion_time = 0;

	// send new task to queue and notify scheduler of event
	xQueueSendFromISR(create_task_queue, &new_task, 4);
	xQueueOverwriteFromISR(event_queue, 1, (BaseType_t*) 4);
}

void delete_dd_task(struct dd_task completed_task) {
	// send task_id and task_iteration to queue
	uint8_t event = 1;
	completed_task.completion_time = xTaskGetTickCount();
	xQueueSend(delete_task_queue, &completed_task, 0);
	xQueueSend(event_queue, &event, 0);
}

struct dd_task_list* get_active_dd_task_list(void) {
	struct dd_task_list* active_list = NULL;
	uint8_t event = 1;
	xQueueOverwrite(request_active_task_queue, &event);
	xQueueOverwrite(event_queue, &event);
	xQueueReceive(get_active_task_queue, &active_list, 0);
	return active_list;
}

struct dd_task_list* get_complete_dd_task_list(void) {
	struct dd_task_list* complete_list = NULL;
	uint8_t event = 1;
	xQueueOverwrite(request_complete_task_queue, &event);
	xQueueOverwrite(event_queue, &event);
	xQueueReceive(get_complete_task_queue, &complete_list, 0);
	return complete_list;
}

struct dd_task_list* get_overdue_dd_task_list(void) {
	struct dd_task_list* overdue_list = NULL;
	uint8_t event = 1;
	xQueueOverwrite(request_overdue_task_queue, &event);
	xQueueOverwrite(event_queue, &event);
	xQueueReceive(get_overdue_task_queue, &overdue_list, 0);
	return overdue_list;
}

void append_to_list(struct dd_task_list** root, struct dd_task_list* node) {
	if (*root == NULL) {
		*root = node;
		return;
	}

	struct dd_task_list* cur = *root;
	while (cur->next_task != NULL) {
		cur = cur->next_task;
	}
	cur->next_task = node;
}

void insert_to_list(struct dd_task_list** root, struct dd_task_list* node) {
	if (*root == NULL) {
		*root = node;
		return;
	}


	struct dd_task_list* prev;
	struct dd_task_list* cur = *root;
	if (cur->task.absolute_deadline >= node->task.absolute_deadline) { // if the incoming node has higher priority than the first
		node->next_task = *root;
		*root = node;
		return;
	}
	while (cur->task.absolute_deadline <= node->task.absolute_deadline) {
		if (cur->next_task == NULL) {
			cur->next_task = node;
			return;
		}
		prev = cur;
		cur = cur->next_task;
	}
	prev->next_task = node;
	node->next_task = cur;
}

struct dd_task_list* remove_from_list(struct dd_task_list** root, struct dd_task task) {
	struct dd_task_list* prev;
	struct dd_task_list* cur = *root;
	while (cur->task.id != task.id || cur->task.iteration != task.iteration) {
		if (cur->next_task == NULL) {
			printf("Did not find task %s in list.", task.name);
			return NULL;
		}
		prev = cur;
		cur = cur->next_task;
	}
	if (cur == *root) {
		*root = cur->next_task;
	} else {
		prev->next_task = cur->next_task;
	}
	cur->next_task = NULL;
	return cur;
}

int main(void) {
	// create queues
	create_task_queue = xQueueCreate(10, sizeof(struct dd_task));

	delete_task_queue = xQueueCreate(10, sizeof(struct dd_task));

	request_active_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_active_task_queue = xQueueCreate(1, sizeof(struct dd_task_list *));

	request_complete_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_complete_task_queue = xQueueCreate(1, sizeof(struct dd_task_list *));

	request_overdue_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_overdue_task_queue = xQueueCreate(1, sizeof(struct dd_task_list *));

	event_queue = xQueueCreate(1, sizeof(uint8_t));

	// add queues to registry
	vQueueAddToRegistry(create_task_queue, "CreateTaskQueue");
	vQueueAddToRegistry(delete_task_queue, "DeleteTaskQueue");
	vQueueAddToRegistry(request_active_task_queue, "RequestActiveTaskQueue");
	vQueueAddToRegistry(get_active_task_queue, "GetActiveTaskQueue");
	vQueueAddToRegistry(request_complete_task_queue, "RequestCompleteTaskQueue");
	vQueueAddToRegistry(get_complete_task_queue, "GetCompleteTaskQueue");
	vQueueAddToRegistry(request_overdue_task_queue, "RequestOverdueTaskQueue");
	vQueueAddToRegistry(get_overdue_task_queue, "GetOverdueTaskQueue");
	vQueueAddToRegistry(event_queue, "EventQueue");

	// create timers
	Task_1_Generator_Timer = xTimerCreate("Task_1_Generator_Timer", pdMS_TO_TICKS(TASK1_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_1);
	Task_2_Generator_Timer = xTimerCreate("Task_2_Generator_Timer", pdMS_TO_TICKS(TASK2_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_2);
	Task_3_Generator_Timer = xTimerCreate("Task_3_Generator_Timer", pdMS_TO_TICKS(TASK3_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_3);
	Monitor_Task_Timer = xTimerCreate("Monitor_Task_Timer", pdMS_TO_TICKS(HYPER_PERIOD_MS), pdTRUE, 0, Monitor_Task_Time_Callback);

	// create F tasks ( scheduler (highest priority), monitor (second highest priority) )
	xTaskCreate(DD_Scheduler, "DD_Scheduler", configMINIMAL_STACK_SIZE, NULL, DD_SCHEDULER_PRIORITY, NULL);
	xTaskCreate(Monitor_Task, "Monitor_Task", configMINIMAL_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, NULL);

	// start timers
	xTimerStart(Task_1_Generator_Timer, 0);
	xTimerStart(Task_2_Generator_Timer, 0);
	xTimerStart(Task_3_Generator_Timer, 0);
	xTimerStart(Monitor_Task_Timer, pdMS_TO_TICKS(HYPER_PERIOD_MS));


	// release tasks right away
	DD_Task_Generator_Callback_1(Task_1_Generator_Timer);
	DD_Task_Generator_Callback_2(Task_2_Generator_Timer);
	DD_Task_Generator_Callback_3(Task_3_Generator_Timer);

	// start scheduler
	vTaskStartScheduler();

	return 0;
}

/**
 * Highest priority tasks and schedules current dd task in the lists
 */
static void DD_Scheduler(void *pvParameters) {
	// malloc active / completed / overdue lists
	struct dd_task_list* active_task_list = NULL;
	struct dd_task_list* completed_task_list = NULL;
	struct dd_task_list* overdue_task_list = NULL;

	while(1) {
		// wait for one of the scheduler's function to be called
		uint8_t event;
		xQueueReceive(event_queue, &event, portMAX_DELAY);

		// set root of active list to default priority
		if (active_task_list != NULL) {
			vTaskPrioritySet(active_task_list->task.t_handle, DD_WAIT_TASK_PRIORITY);
		}

		// non-blocking check completed queue
		struct dd_task task_to_delete;
		while (xQueueReceive(delete_task_queue, &task_to_delete, 0)) {
			if (active_task_list == NULL) {
				break;
			}
			// remove completed task from active queue and add to completed list
			struct dd_task_list* completed_task = remove_from_list(&active_task_list, task_to_delete);
			completed_task->task.completion_time = task_to_delete.completion_time;
			// If task was completed before deadline, then move to completed list, else move to overdue list
			if (completed_task->task.completion_time <= completed_task->task.absolute_deadline) {
				append_to_list(&completed_task_list, completed_task);
			} else {
				append_to_list(&overdue_task_list, completed_task);
			}
			// remove task
			vTaskDelete(completed_task->task.t_handle);
		}

		// non-blocking check for overdue tasks
		struct dd_task_list* cur = active_task_list;
		TickType_t cur_tick_count = xTaskGetTickCount();
		while (cur != NULL) {
			if (cur_tick_count >= cur->task.absolute_deadline) {
				remove_from_list(&active_task_list, cur->task);
				append_to_list(&overdue_task_list, cur);
				// remove task
				vTaskDelete(cur->task.t_handle);
			}
			cur = cur->next_task;
		}

		// non-blocking check release queue
		struct dd_task new_task;
		while (xQueueReceive(create_task_queue, &new_task, 0)) {
			// make new dd_task_list node
			struct dd_task_list* new_task_list = (struct dd_task_list *) pvPortMalloc(sizeof(struct dd_task_list));
			new_task_list->task = new_task;
			new_task_list->next_task = NULL;
			// put released tasks in active list
			insert_to_list(&active_task_list, new_task_list);

			TaskFunction_t DD_Task;
			if (new_task.id == 1) {
				DD_Task = DD_Task_1;
			}
			if (new_task.id == 2) {
				DD_Task = DD_Task_2;
			}
			if (new_task.id == 3) {
				DD_Task = DD_Task_3;
			}

			xTaskCreate(DD_Task, new_task.name, configMINIMAL_STACK_SIZE, &new_task_list->task, DD_WAIT_TASK_PRIORITY, &new_task_list->task.t_handle);
		}

		// set highest priority task's priority to active task priority
		if (active_task_list != NULL) {
			vTaskPrioritySet(active_task_list->task.t_handle, DD_RUNNING_TASK_PRIORITY);
		}

		// non-blocking check requests for active / completed / overdue
		// respond to requests
		uint8_t request;
		if (xQueueReceive(request_active_task_queue, &request, 0)) {
			xQueueSend(get_active_task_queue, &active_task_list, 0);
		}
		if (xQueueReceive(request_complete_task_queue, &request, 0)) {
			xQueueSend(get_complete_task_queue, &completed_task_list, 0);
		}
		if (xQueueReceive(request_overdue_task_queue, &request, 0)) {
			xQueueSend(get_overdue_task_queue, &overdue_task_list, 0);
		}
	}
}

static void Monitor_Task_CallBack(TimerHandle_t xTimer) {
		
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_1(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID
	char name[13];
	sprintf(name, "DD_Task_1_%i", iteration);
	create_dd_task(PERIODIC, 1, iteration, name, (xTaskGetTickCount() + pdMS_TO_TICKS(TASK1_PERIOD_MS)));
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_2(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID
	char name[13];
	sprintf(name, "DD_Task_2_%i", iteration);
	create_dd_task(PERIODIC, 2, iteration, name, (xTaskGetTickCount() + pdMS_TO_TICKS(TASK2_PERIOD_MS)));
}

/**
 * timer call back to create new dd tasks from user defined task
 */
static void DD_Task_Generator_Callback_3(TimerHandle_t xTimer) {
	// *never block*
	// on each call create a new dd_task
	uint32_t iteration = (int)pvTimerGetTimerID(xTimer); // get the ID of the timer
	vTimerSetTimerID(xTimer, (void *) (iteration + 1)); // increment the timer ID
	char name[13];
	sprintf(name, "DD_Task_3_%i", iteration);
	create_dd_task(PERIODIC, 3, iteration, name, (xTaskGetTickCount() + pdMS_TO_TICKS(TASK3_PERIOD_MS)));
}

static void DD_Task_1(void *pvParameters) {
	while (1) {
		int end = (int)(xTaskGetTickCount() + pdMS_TO_TICKS(TASK1_EXECUTION_MS));
		while (end > (int)xTaskGetTickCount()); // burn time.
		struct dd_task* task = (struct dd_task*) pvParameters;
		delete_dd_task(*task);
		vTaskSuspend(NULL); // block forever
	}
}

static void DD_Task_2(void *pvParameters) {
	while (1) {
		int end = (int)(xTaskGetTickCount() + pdMS_TO_TICKS(TASK2_EXECUTION_MS));
		while (end > (int)xTaskGetTickCount()); // burn time.
		struct dd_task* task = (struct dd_task*) pvParameters;
		delete_dd_task(*task);
		vTaskSuspend(NULL); // block forever
	}
}

static void DD_Task_3(void *pvParameters) {
	while (1) {
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK3_EXECUTION_MS);
		while (end > xTaskGetTickCount()); // burn time.
		struct dd_task* task = (struct dd_task*) pvParameters;
		delete_dd_task(*task);
		vTaskSuspend(NULL); // block forever
	}
}
/**
 * TODO:
 * - Monitor task never runs on test bench 3. Probably because there is no downtime between tasks. Maybe timer interupt for monitor task instead of task?
 * - No overdue tasks? set DEBUG to 1 for timings.
 */
static void Monitor_Task(void *pvParameters) {
	TickType_t last_print = xTaskGetTickCount();
	struct dd_task_list* active_list = NULL;
	struct dd_task_list* completed_list = NULL;
	struct dd_task_list* overdue_list = NULL;
	while (1) {
		if (last_print + pdMS_TO_TICKS(HYPER_PERIOD_MS) < xTaskGetTickCount()) {
			// request active / complete / overdue lists TODO: need to use help functions for scheduler...
			active_list = get_active_dd_task_list();
			completed_list = get_complete_dd_task_list();
			overdue_list = get_overdue_dd_task_list();

			vTaskPrioritySet(xTaskGetCurrentTaskHandle(), MONITOR_TASK_PRIORITY_PRINTING);

			char* buffer = (char *)pvPortMalloc(sizeof(char)*1024);
			// print everything we have
			printf("Tick Time: %i\n", (int)xTaskGetTickCount());
			if (DEBUG) {
				printf("active_list:\n");
			}
			int length = 0;
			while (active_list != NULL) {
				if (DEBUG) {
					printf("\tname: %s, release: %i, deadline: %i\n",\
							active_list->task.name, active_list->task.release_time, active_list->task.absolute_deadline);
				}
				active_list = active_list->next_task;
				length++;
			}
			printf("active_list_length: %i\n", length);
			length = 0;
			if (DEBUG) {
				printf("completed_list:\n");
			}
			while (completed_list != NULL) {
				if (DEBUG) {
					printf("\tname: %s, release: %i, completed: %i, deadline: %i\n",\
							completed_list->task.name, completed_list->task.release_time, completed_list->task.completion_time, completed_list->task.absolute_deadline);
				}
				completed_list = completed_list->next_task;
				length++;
			}
			printf("completed_list_length: %i\n", length);
			length = 0;
			if (DEBUG) {
				printf("overdue_list:\n");
			}
			while (overdue_list != NULL) {
				if (DEBUG) {
					printf("\tname: %s,  release: %i, completed: %i, deadline: %i\n", \
							overdue_list->task.name, overdue_list->task.release_time, overdue_list->task.completion_time, overdue_list->task.absolute_deadline);
				}
				overdue_list = overdue_list->next_task;
				length++;
			}
			printf("overdue_list_length: %i\n", length);
			active_list = NULL;
			completed_list = NULL;
			overdue_list = NULL;
			last_print = xTaskGetTickCount();
			vPortFree(buffer);
			vTaskPrioritySet(xTaskGetCurrentTaskHandle(), MONITOR_TASK_PRIORITY);
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
	printf("vApplicationMallocFailedHook\n");
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
	printf("vApplicationStackOverflowHook\n");
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
