/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-workqueue.c
 * @brief	This file include implementation for per container extra operation.
 */
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "container-workqueue.h"

//#undef _PRINTF_DEBUG_

/**
 * Cleanup scheduled per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval 0	Success to cleanup.
 * @retval -1	Still running.
 * @retval -2	Arg. error.
 * @retval -3	Already cleanup.
 */
int container_workqueue_cleanup(container_workqueue_t *workqueue, int *after_execute)
{
	if (workqueue == NULL || after_execute == NULL)
		return -2;

	if (workqueue->status == CONTAINER_WORKER_STARTED)
		return -1;
	else if (workqueue->status != CONTAINER_WORKER_COMPLETED)
		return -3;

	workqueue->worker_func = NULL;
	workqueue->status = CONTAINER_WORKER_INACTIVE;

	if (workqueue->state_after_execute == 1)
		(*after_execute) = 1;
	else
		(*after_execute) = 0;

	workqueue->state_after_execute = 0;

	return 0;
}

/**
 * Thread entry point for per container workqueue.
 *
 * @param [in]	args	Pointer to own container_workqueue_t.
 * @return void*	Will not return.
 */
static void* container_workqueue_thread(void *args)
{
	int ret = -1;
	container_workqueue_t *workqueue = (container_workqueue_t*)args;
	container_worker_func_t func;

	if (args == NULL)
		pthread_exit(NULL);

	func = workqueue->worker_func;
	if (func != NULL)
		ret = func();

	(void)pthread_mutex_lock(&(workqueue->workqueue_mutex));
	workqueue->status = CONTAINER_WORKER_COMPLETED;
	(void)pthread_mutex_unlock(&(workqueue->workqueue_mutex));

	pthread_exit(NULL);

	return NULL;
}

/**
 * Run scheduled per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval 0	Success to schedule.
 * @retval -1	Is not scheduled workqueue.
 * @retval -2	Arg. error.
 * @retval -3	Internal error.
 */
int container_workqueue_run(container_workqueue_t *workqueue)
{
	int ret = -1;
	pthread_attr_t thread_attr;

	if (workqueue == NULL)
		return -2;

	if (workqueue->status != CONTAINER_WORKER_SCHEDULED)
		return -1;

	(void) pthread_attr_init(&thread_attr);
	(void) pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&(workqueue->worker_thread), &thread_attr, container_workqueue_thread, (void*)workqueue);
	(void) pthread_attr_destroy(&thread_attr);
	if (ret < 0) {
		#ifdef CM_CRITICAL_ERROR_OUT_STDERROR
		(void) fprintf(stderr,"[CM CRITICAL ERROR] container_workqueue_run: Fail to create workqueue thread.\n");
		#endif
		return -3;
	}

	(void)pthread_mutex_lock(&(workqueue->workqueue_mutex));
	if (workqueue->status == CONTAINER_WORKER_SCHEDULED)
		workqueue->status = CONTAINER_WORKER_STARTED;
	(void)pthread_mutex_unlock(&(workqueue->workqueue_mutex));

	return 0;
}

/**
 * Remove scheduled per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval 0	Success to remove.
 * @retval -1	Already run.
 * @retval -2	Arg. error.
 */
int container_workqueue_remove(container_workqueue_t *workqueue)
{
	if (workqueue == NULL)
		return -2;

	if (workqueue->status == CONTAINER_WORKER_STARTED || workqueue->status == CONTAINER_WORKER_COMPLETED)
		return -1;

	workqueue->worker_func = NULL;

	if (workqueue->status != CONTAINER_WORKER_DISABLE)
		workqueue->status = CONTAINER_WORKER_INACTIVE;

	workqueue->state_after_execute = 0;

	return 0;
}

/**
 * Schedule per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @param [in]	func		Pointer to worker function.
 * @param [in]	launch_after_end	The flag of launch container after worker executed. 1: launch container, 0: keep current state.
 * @return int
 * @retval 0	Success to schedule.
 * @retval -1	Already scheduled.
 * @retval -2	Arg. error.
 */
int container_workqueue_schedule(container_workqueue_t *workqueue, container_worker_func_t func, int launch_after_end)
{
	if (workqueue == NULL || func == NULL)
		return -2;

	if (workqueue->status != CONTAINER_WORKER_INACTIVE)
		return -1;

	workqueue->worker_func = func;
	workqueue->status = CONTAINER_WORKER_SCHEDULED;

	workqueue->state_after_execute = launch_after_end;

	return 0;
}

/**
 * Get status of per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval CONTAINER_WORKER_DISABLE		Container workqueue status is disable.
 * @retval CONTAINER_WORKER_INACTIVE	Container workqueue status is inactive.
 * @retval CONTAINER_WORKER_SCHEDULED	Container workqueue status is scheduled.
 * @retval CONTAINER_WORKER_STARTED		Container workqueue status is started.
 * @retval CONTAINER_WORKER_COMPLETED	Container workqueue status is completed.
 */
int container_workqueue_get_status(container_workqueue_t *workqueue)
{
	if (workqueue == NULL)
		return CONTAINER_WORKER_DISABLE;

	return workqueue->status;
}

/**
 * Per container workqueue initialize.
 *
 * @param [in]	workqueue	Pointer to container_workqueue_t that memory is allocated.
 * @return int
 * @retval 0	Success to initialize.
 * @retval -1	Fail to initialize.
 */
int container_workqueue_initialize(container_workqueue_t *workqueue)
{
	int ret = -1;
	int result = -1;
	pthread_mutexattr_t mutex_attr;

	if (workqueue == NULL)
		return -1;

	memset(workqueue, 0, sizeof(container_workqueue_t));
	workqueue->status = CONTAINER_WORKER_DISABLE;

	(void)pthread_mutexattr_init(&mutex_attr);
	(void)pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK);

	ret = pthread_mutex_init(&(workqueue->workqueue_mutex), &mutex_attr);
	if (ret < 0)
		goto err_ret;

	workqueue->worker_func = NULL;

	workqueue->status = CONTAINER_WORKER_INACTIVE;
	workqueue->state_after_execute = 0;
err_ret:

	(void)pthread_mutexattr_destroy(&mutex_attr);

	return result;
}
/**
 * Per container workqueue deinitialize.
 *
 * @param [in]	workqueue	Pointer to container_workqueue_t that memory is allocated.
 * @return int
 * @retval 0	Success to deinitialize.
 * @retval -1	Fail to deinitialize.
 */
int container_workqueue_deinitialize(container_workqueue_t *workqueue)
{
	if (workqueue == NULL)
		return -1;

	workqueue->status = CONTAINER_WORKER_DISABLE;
	workqueue->state_after_execute = 0;
	(void)pthread_mutex_destroy(&(workqueue->workqueue_mutex));

	return 0;
}
