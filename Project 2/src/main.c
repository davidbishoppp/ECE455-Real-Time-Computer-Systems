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

#define SCHEDULER_EVENT_GROUP 0

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
	uint32_t task_id;
	uint32_t task_iteration;
	TickType_t release_time;
	TickType_t absolute_deadline;
	TickType_t completion_time;
};

struct dd_task_list {
	struct dd_task* task;
	struct dd_task_list* next_task;
};

struct completed_dd_task_info {
	uint32_t task_id;
	uint32_t task_iteration;
	TickType_t completion_time;
};

void create_dd_task( TaskHandle_t t_handle, enum task_type type, uint32_t task_id, uint32_t absolute_deadline) {
	// create a dd_task
	struct dd_task new_task;
	new_task.absolute_deadline = absolute_deadline;
	new_task.release_time = xTaskGetTickCount();
	new_task.t_handle = t_handle;
	new_task.type = type;
	new_task.task_id = task_id;

	// send the dd_task to queue
	xQueueSend(create_task_queue, &new_task, 500);

	// set bit 0 in scheduler event group
	xEventGroupSetBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP);
}

void delete_dd_task(uint32_t task_id, uint32_t task_iteration, TickType_t completion_time) {
	// send task_id and task_iteration to queue
	struct completed_dd_task_info completed_task;
	completed_task.task_id = task_id;
	completed_task.task_iteration = task_iteration;
	completed_task.completion_time = completion_time;

	xQueueSend(delete_task_queue, &completed_task, 500);

	// set bit 1 in scheduler event group
	xEventGroupSetBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP);
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

void append_to_list(struct dd_task_list* root, struct dd_task_list* node) {
	// if list is empty, then just copy over data into root
	if (root->task == NULL) {
		root->task = node->task;
		root->next_task = NULL;
		return;
	}

	struct dd_task_list* cur = root;
	while (cur->next_task != NULL) {
		cur = cur->next_task;
	}
	cur->next_task = node;
	node->next_task = NULL;
}

void insert_to_list(struct dd_task_list* root, struct dd_task_list* node) {
	// if list empty, then just copy over data into root
	if (root->task == NULL) {
		root->task = node->task;
		root->next_task = NULL;
		return;
	}

	TickType_t new_node_deadline = node->task->absolute_deadline;
	struct dd_task_list* cur = root;
	for (;;) {

		// If we are on the last node of the list
		if (cur->next_task == NULL) {
			// Insert after last node if deadline after or same
			if (cur->task->absolute_deadline <= new_node_deadline) {
				cur->next_task = node;
				node->next_task = NULL;
			} else { // Insert before last node if deadline before
				struct dd_task_list temp = {cur->task, cur->next_task};
				cur->task = node->task;
				cur->next_task = node;
				node->task = temp.task;
				node->next_task = NULL;
			}
			break;
		}

		// If this code runs we know we have at least two nodes left to traverse

		// If new deadline is before the rest of the nodes, insert before
		if (cur->task->absolute_deadline > new_node_deadline) {
			struct dd_task_list temp = {node->task, node->next_task};
			node->task = cur->task;
			node->next_task = cur->next_task;
			cur->task = temp.task;
			cur->next_task = node;
			break;

		}
		// If new deadline is before the next node, insert in-between
		else if (cur->next_task->task->absolute_deadline > new_node_deadline) {
			node->next_task = cur->next_task;
			cur->next_task = node;
			break;

		}
		// New deadline after the next two nodes, traverse to next node
		else {
			cur = cur->next_task;
		}

	}
}

struct dd_task_list remove_from_list(struct dd_task_list* root, char task_id_iteration[2]) {
	// If list singular, then check only root and remove if matches
	struct dd_task_list removed_node = {NULL, NULL};
	if (root->next_task == NULL) {
		if (root->task->task_id == task_id_iteration[0] && root->task->task_iteration == task_id_iteration[1]) {
			removed_node.task = root->task;
			removed_node.next_task = root->next_task;
			root->task = NULL;
			root->next_task = NULL;
		}
		return removed_node;
	}

	// If this code executes, we know the list is at length two

	struct dd_task_list* cur = root;
	for (;;) {
		struct dd_task_list next= {cur->next_task->task, cur->next_task->next_task};
		// If current node matches
		if (cur->task->task_id == task_id_iteration[0] && cur->task->task_iteration == task_id_iteration[1]) {
			// Return current node as returned node
			removed_node.task = cur->task;
			removed_node.next_task = cur->next_task;
			// Move next node's data to current node
			cur->task = next.task;
			cur->next_task = next.next_task;
			// Nullify next node
			cur->next_task->task = NULL;
			cur->next_task->next_task = NULL;
			return removed_node;
		}
		// If next node matches
		else if (cur->next_task->task->task_id == task_id_iteration[0] \
				&& cur->next_task->task->task_iteration == task_id_iteration[1]) {
			// Return next node as returned node
			removed_node.task = next.task;
			removed_node.next_task = next.next_task;
			// Move next node's next node to current node's next
			cur->next_task = next.next_task;
			// Nullify next node
			cur->next_task->task = NULL;
			cur->next_task->next_task = NULL;
			return removed_node;
		}
		// No matches for these two nodes, move to the next so long as it is not last
		else if (next.next_task != NULL) {
			cur = cur->next_task;
		}
		// Next node is the last node and since it did not match earlier we have no matches
		else {
			return removed_node;
		}
	}
}

/**
 * Sets the last character of the task name to the iteration we're at.
 */
void set_task_name_iteration(char* task_name, uint32_t iteration) {
	char c = '0' + iteration;
	if (c > '9') {
		c += 7; // skip some ascii characters to get to A-Z after numbers
	}
	task_name[sizeof(task_name)-1] = c; // set the iteration
}

int main(void) {
	// create queues
	create_task_queue = xQueueCreate(10, sizeof(struct dd_task));

	delete_task_queue = xQueueCreate(10, sizeof(struct completed_dd_task_info));

	request_active_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_active_task_queue = xQueueCreate(1, sizeof(uint8_t));

	request_complete_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_complete_task_queue = xQueueCreate(1, sizeof(uint8_t));

	request_overdue_task_queue = xQueueCreate(1, sizeof(uint8_t));
	get_overdue_task_queue = xQueueCreate(1, sizeof(uint8_t));

	// add queues to registry
	vQueueAddToRegistry(create_task_queue, "CreateTaskQueue");
	vQueueAddToRegistry(delete_task_queue, "DeleteTaskQueue");
	vQueueAddToRegistry(request_active_task_queue, "RequestActiveTaskQueue");
	vQueueAddToRegistry(get_active_task_queue, "GetActiveTaskQueue");
	vQueueAddToRegistry(request_complete_task_queue, "RequestCompleteTaskQueue");
	vQueueAddToRegistry(get_complete_task_queue, "GetCompleteTaskQueue");
	vQueueAddToRegistry(request_overdue_task_queue, "RequestOverdueTaskQueue");
	vQueueAddToRegistry(get_overdue_task_queue, "GetOverdueTaskQueue");

	// create timers
	Task_1_Generator_Timer = xTimerCreate("Task_1_Generator_Timer", pdMS_TO_TICKS(TASK1_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_1);
	Task_2_Generator_Timer = xTimerCreate("Task_2_Generator_Timer", pdMS_TO_TICKS(TASK2_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_2);
	Task_3_Generator_Timer = xTimerCreate("Task_3_Generator_Timer", pdMS_TO_TICKS(TASK3_PERIOD_MS), pdTRUE, 0, DD_Task_Generator_Callback_3);

	// create F tasks ( scheduler (highest priority), monitor (second highest priority) )
	xTaskCreate(DD_Scheduler, "DD_Scheduler", configMINIMAL_STACK_SIZE, NULL, DD_SCHEDULER_PRIORITY, NULL);
	//xTaskCreate(Monitor_Task, "Monitor_Task", configMINIMAL_STACK_SIZE, NULL, MONITOR_TASK_PRIORITY, NULL);

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
		xEventGroupWaitBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP, pdTRUE, pdFALSE, portMAX_DELAY);

		// set root of active list to default priority
		struct dd_task* root_task = active_task_list.task;

		if (root_task != NULL) {
			TaskHandle_t root_task_handle = root_task->t_handle;
			vTaskPrioritySet(root_task_handle, DD_WAIT_TASK_PRIORITY);
		}

		// non-blocking check completed queue
		struct completed_dd_task_info completed_task;
		while (xQueueReceive(delete_task_queue, &completed_task, 0)) {
			// remove completed task from active queue and add to completed list
			char task_id_iteration[2] = {completed_task.task_id, completed_task.task_iteration};
			struct dd_task_list removed_task = remove_from_list(&active_task_list, task_id_iteration);
			removed_task.task->completion_time = completed_task.completion_time;

			// If task was completed before deadline, then move to completed list, else move to overdue list
			if (removed_task.task->completion_time <= removed_task.task->release_time) {
				append_to_list(&completed_task_list, &removed_task);
				count_completed_tasks++;
			} else {
				append_to_list(&overdue_task_list, &removed_task);
				count_overdue_tasks++;
			}
			count_active_tasks--;

		}

		// non-blocking check for overdue tasks
		struct dd_task_list* cur = &active_task_list;
		TickType_t curr_tick_count = xTaskGetTickCount();
		while (cur != NULL && cur->task != NULL) {
			if (cur->task->absolute_deadline >= curr_tick_count) {
				char task_id_iteration[2] = {cur->task->task_id, cur->task->task_iteration};
				remove_from_list(&active_task_list, task_id_iteration);
				append_to_list(&overdue_task_list, cur);
				count_active_tasks--;
				count_overdue_tasks++;
			}
			cur = cur->next_task;
		}

		// non-blocking check release queue
		struct dd_task new_task;
		TaskHandle_t new_task_handle;
		while (xQueueReceive(create_task_queue, &new_task, 0)) {
			// make new dd_task_list node
			struct dd_task_list new_task_list = {&new_task, NULL};
			new_task.t_handle = new_task_handle;
			// put released tasks in active list
			insert_to_list(&active_task_list, &new_task_list);

			char* name;
			TaskFunction_t DD_Task;
			if (new_task.task_id == 1) {
				name = "DD_Task_1_ ";
				DD_Task = DD_Task_1;
				set_task_name_iteration(name, new_task.task_iteration);
			}
			if (new_task.task_id == 2) {
				name = "DD_Task_2_ ";
				DD_Task = DD_Task_2;
				set_task_name_iteration(name, new_task.task_iteration);
			}
			if (new_task.task_id == 3) {
				name = "DD_Task_3_ ";
				DD_Task = DD_Task_3;
				set_task_name_iteration(name, new_task.task_iteration);
			}

			xTaskCreate(DD_Task, name, configMINIMAL_STACK_SIZE, &new_task, DD_WAIT_TASK_PRIORITY, &new_task_handle);
		}

		// set highest priority task's priority to active task priority
		root_task = active_task_list.task;

		if (root_task != NULL) {
			TaskHandle_t root_task_handle = root_task->t_handle;
			vTaskPrioritySet(root_task_handle, DD_RUNNING_TASK_PRIORITY);
		}

		// non-blocking check requests for active / completed / overdue
		// respond to requests
		uint8_t request;
		if (xQueueReceive(request_active_task_queue, &request, 0)) {
			xQueueSend(get_active_task_queue, &count_active_tasks, 0);
		}
		if (xQueueReceive(request_complete_task_queue, &request, 0)) {
			xQueueSend(get_complete_task_queue, &count_completed_tasks, 0);
		}
		if (xQueueReceive(request_overdue_task_queue, &request, 0)) {
			xQueueSend(get_overdue_task_queue, &count_overdue_tasks, 0);
		}
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
	new_task.task_id = 1;
	new_task.task_iteration = iteration;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK1_PERIOD_MS);
	new_task.completion_time = 0;

	printf("Creating Task 1\n");
	// place dd_task on create_task_queue
	xQueueSend(create_task_queue, &new_task, 0);
	xEventGroupSetBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP);
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
	new_task.task_id = 2;
	new_task.task_iteration = iteration;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK2_PERIOD_MS);
	new_task.completion_time = 0;

	printf("Creating Task 2\n");
	// place dd_task on create_task_queue
	xQueueSend(create_task_queue, &new_task, 0);
	xEventGroupSetBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP);
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
	new_task.task_id = 3;
	new_task.task_iteration = iteration;
	new_task.release_time = tick_count;
	new_task.absolute_deadline = tick_count + pdMS_TO_TICKS(TASK3_PERIOD_MS);
	new_task.completion_time = 0;

	// place dd_task on create_task_queue
	printf("Creating Task 3\n");
	xQueueSend(create_task_queue, &new_task, 0);
	xEventGroupSetBits(DD_Scheduler_Event_Group, SCHEDULER_EVENT_GROUP);
}

static void DD_Task_1(void *pvParameters) {
	while (1) {
		printf("DD_Task_1 start\n");
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK1_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
		printf("DD_Task_1 end\n");
		struct dd_task* task_desc = (struct dd_task*) pvParameters;
		delete_dd_task(task_desc->task_id, task_desc->task_iteration, xTaskGetTickCount());
	}
}

static void DD_Task_2(void *pvParameters) {
	while (1) {
		printf("DD_Task_2 start\n");
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK2_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
		printf("DD_Task_2 end\n");
		struct dd_task* task_desc = (struct dd_task*) pvParameters;
		delete_dd_task(task_desc->task_id, task_desc->task_iteration, xTaskGetTickCount());
	}
}

static void DD_Task_3(void *pvParameters) {
	while (1) {
		printf("DD_Task_3 start\n");
		// burn time.
		TickType_t end = xTaskGetTickCount() + pdMS_TO_TICKS(TASK3_EXECUTION_MS);
		while (xTaskGetTickCount() < end);
		printf("DD_Task_3 end\n");
		struct dd_task* task_desc = (struct dd_task*) pvParameters;
		delete_dd_task(task_desc->task_id, task_desc->task_iteration, xTaskGetTickCount());
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
/* TESTING --------------------------------------------------*/

void test_name_creation() {
	char* name = "DD_Task_1_ ";
	set_task_name_iteration(name, 5);
	printf("name: %s\n", name);
}

void test_list_functions() {

}

/* TESTING --------------------------------------------------*/
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
	printf("vApplicationIdleHook\n");
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
