/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-manager-operations.c
 * @brief	A manager operation for container manager.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <poll.h>

#include "container.h"
#include "cm-utils.h"

/**
 * @struct	s_container_manager_operation_storage
 * @brief	The data storage for manager operations.
 */
struct s_container_manager_operation_storage {
	pthread_t	worker_thread;	/**< Worker thread object.*/
	int host_fd;		/**< Socket fd for worker host. */
	int worker_fd;		/**< Socket fd for worker. */
};
typedef struct s_container_manager_operation_storage container_manager_operation_storage_t;

/**
 * @struct	s_container_manager_operation_storage
 * @brief	The data storage for manager operations.
 */
struct s_worker_operation_storage {
	struct dl_list mount_list;	/**< Double link list for worker operation. */

	int worker_fd;		/**< Socket fd for worker. */
};
typedef struct s_worker_operation_storage worker_operation_storage_t;	/**< typedef for struct s_worker_operation_storage. */

/**
 * @struct	struct s_worker_request
 * @brief	The request data packet.
 */
struct s_worker_request {
	int index;		/**< Index for task completed object. */
	int request;	/**< Operation request. 0: none, 1: cancel.*/
};
typedef struct s_worker_request worker_request_t;	/**< typedef for struct s_worker_request. */
/**
 * @struct	struct s_worker_response
 * @brief	The response data packet.
 */
struct s_worker_response {
	int index;		/**< Index for task completed object. */
	int operation;	/**< Operation code for task completed object. 0: mount, 1: unmount.*/
	int result;		/**< Result code for task completed object. 0: full complete, 1: cancel, -1: error. */
};
typedef struct s_worker_response worker_response_t;	/**< typedef for struct s_worker_response. */


static int manager_operation_mount_elem_free(container_manager_operation_mount_elem_t *celem);
static int manager_operation_delayed_storage_list_free(worker_operation_storage_t *wos);


/**
 * @brief Function for disk format execution.
 *
 * @param [in]	wos				Initialized worker_operation_storage_t.
 * @param [in]	cancel_index	Index of cancel worker object. When -1 set on cancel_index, test only.
 * @return Description for return value
 * @retval 1	All object was canceled.
 * @retval 0	Success to set cancel.
 * @retval -1	Fail to execute worker.
 */
static int manager_worker_set_and_test_cancel(worker_operation_storage_t *wos, int cancel_index)
{
	container_manager_operation_mount_elem_t *celem = NULL;
	int total_num = 0, completed_num = 0;
	int result = -1;

	dl_list_for_each(celem, &wos->mount_list, container_manager_operation_mount_elem_t, list) {
		if (celem->index == cancel_index) {
			celem->state = MANAGER_WORKER_STATE_CANCELED;
			result = 0;
		}

		total_num++;
		if ((celem->state == MANAGER_WORKER_STATE_COMPLETE) || (celem->state == MANAGER_WORKER_STATE_CANCELED)) {
			completed_num++;
		}
	}

	if (total_num == completed_num) {
		result = 1;
	}

	return result;
}
/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Need to optional worker.
 */
static int manager_mount_operation(int control_fd, worker_operation_storage_t *wos, int is_retake)
{
	container_manager_operation_mount_elem_t *celem = NULL, *celem_n = NULL;
	unsigned long mntflag = 0;
	int result = 0, ret = -1;

	dl_list_for_each_safe(celem, celem_n, &wos->mount_list, container_manager_operation_mount_elem_t, list) {
		worker_response_t wres;
		wres.index = celem->index;
		wres.result = 0;

		if (celem->is_mounted == 0) {
			// do mount operation
			wres.operation = 0;

			// Set mount flag
			if (celem->mode == MANAGER_DISKMOUNT_TYPE_RW) {
				mntflag = MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS;
			} else {
				mntflag = MS_NOATIME | MS_RDONLY;
			}

			// do mount operations
			if (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_FAILOVER) {
				ret = mount_disk_failover(celem->blockdev, celem->to, celem->filesystem, mntflag, celem->option);
				dl_list_del(&celem->list);
				celem->state = MANAGER_WORKER_STATE_COMPLETE;
				if (ret < 0) {
					wres.result = -1;
				}
				(void) manager_operation_mount_elem_free(celem);
				(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));
			} else if ((celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_FSCK) || (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_MKFS)) {
				ret = mount_disk_once(celem->blockdev, celem->to, celem->filesystem, mntflag, celem->option);
				if ((ret != -1) && (is_retake == 0)) {
					// In case of success or error without mount error, return operation result.
					dl_list_del(&celem->list);
					celem->state = MANAGER_WORKER_STATE_COMPLETE;
					if (ret < 0) {
						wres.result = -1;
					}
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "manager_mount_operation: mount_disk_once ret %d.\n", ret);
					#endif
					(void) manager_operation_mount_elem_free(celem);
					(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));
				} else {
					// In case of mount error, need to exec next step. Keep this object in list.
					result = -1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "manager_mount_operation: mount_disk_once fail do need to recover.\n");
					#endif
				}
			} else {
				// AB and other not support, remove element from list.
				dl_list_del(&celem->list);
				celem->state = MANAGER_WORKER_STATE_COMPLETE;
				wres.result = -1;
				(void) manager_operation_mount_elem_free(celem);
				(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));
			}
		} else {
			// do unmount operation.
			int64_t timeout_time = 0;
			int retry_max = 0;

			wres.operation = 1;

			// 1s timeout.
			timeout_time = get_current_time_ms() + 1000ll;
			retry_max = (1000 / 50) + 1;

			ret = unmount_disk(celem->to, timeout_time, retry_max);
			dl_list_del(&celem->list);
			celem->state = MANAGER_WORKER_STATE_COMPLETE;
			if(ret < 0) {
				wres.result = -1;
			}
			(void) manager_operation_mount_elem_free(celem);
			(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));
		}
	}

	return result;
}
/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int manager_worker_exec(int control_fd, worker_operation_storage_t *wos)
{
	struct pollfd waiter[2];
	container_manager_operation_mount_elem_t *celem = NULL, *celem_n = NULL;
	siginfo_t child_info;
	int result = -1;
	int ret = -1;
	int exit_code = -1;
	int child_fd = -1;
	pid_t child_pid = -1;

	if (wos == NULL) {
		result = -1;
		goto do_return;
	}

	dl_list_for_each_safe(celem, celem_n, &wos->mount_list, container_manager_operation_mount_elem_t, list) {
		if (celem->state != MANAGER_WORKER_STATE_QUEUED) {
			// Already canceled. Skip.
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "manager_worker_exec: %s operation was canceled.\n", celem->blockdev[0]);
			#endif
			continue;
		}

		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout, "manager_worker_exec: do %s operation.\n", celem->blockdev[0]);
		#endif

		child_pid = fork();
		if (child_pid < 0) {
			// Fail to fork
			celem->state = MANAGER_WORKER_STATE_CANCELED;
			continue;
		}

		if (child_pid == 0) {
			if (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_FSCK) {
				// exec /sbin/fsck.ext4 -p
				(void) execlp("/sbin/fsck.ext4", "/sbin/fsck.ext4", "-p", celem->blockdev[0], (char*)NULL);
			} else if (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_MKFS) {
				// exec /sbin/fsck.ext4 -p
				(void) execlp("/sbin/mkfs.ext4", "/sbin/mkfs.ext4", "-I", "256", celem->blockdev[0], (char*)NULL);
			} else {
				;	//nop
			}
			// Shall not return execlp
			(void) _exit(128);
		}

		#ifdef _PRINTF_DEBUG_
		if (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_FSCK) {
			(void) fprintf(stdout, "manager_worker_exec fsck fork and exec fsck.ext4 pid=%d\n",(int)child_pid);
		} else if (celem->redundancy == MANAGER_DISKREDUNDANCY_TYPE_MKFS) {
			(void) fprintf(stdout, "manager_worker_exec mkfs fork and exec mkfs.ext4 pid=%d\n",(int)child_pid);
		} else {
			;	//nop
		}
		#endif

		child_fd = pidfd_open_syscall_wrapper(child_pid);
		// Fail to create pidfd, it handled error case of poll.

		(void) memset(waiter, 0, sizeof(waiter));
		waiter[0].fd = child_fd;
		waiter[0].events = POLLIN;
		waiter[1].fd = control_fd;
		waiter[1].events = POLLIN;

		do {
			ret = poll(waiter, 2, 100);	//100ms timeout
			if (ret > 0) {
				// Got a event
				if (waiter[0].revents != 0) {
					// child process was exited.
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "manager_worker_exec got a exit child process.\n");
					#endif
					break;
				}

				if (waiter[1].revents != 0) {
					// Message receive from host.
					worker_request_t wreq;
					worker_response_t wres;
					ssize_t sret = -1;

					(void) memset(&wreq, 0, sizeof(wreq));
					sret = read(waiter[1].fd, &wreq, sizeof(wreq));
					if (sret >= 0) {
						if (wreq.request == 1) {
							(void) manager_worker_set_and_test_cancel(wos, wreq.index);
							// Send cancel response.
							wres.index = wreq.index;
							wres.operation = 0;
							wres.result = 1;
							(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));

							// Target is now operating?
							if (celem->index == wreq.index) {
								// Do cancel operation
								ret = pidfd_send_signal_syscall_wrapper(child_fd, SIGTERM, NULL, 0);
								if (ret < 0) {
									(void) kill(child_pid, SIGTERM);
								}
								break;
							}
						}
					}
				}
				break;
			} else if (ret == 0) {
				// Timeout, test to canceled.
				ret = manager_worker_set_and_test_cancel(wos, -1);
				if (ret == 1) {
					// All worker was canceled or completed.
					break;
				}
			} else {
				if (errno != EINTR) {
					// Can't wait child process.
					(void) kill(child_pid, SIGTERM);
					break;
				}
			}
		} while(1);

		if (child_fd >= 0) {
			(void) close(child_fd);
		}

		(void) memset(&child_info, 0, sizeof(child_info));

		ret = waitid(P_PID, (id_t)child_pid, &child_info, WEXITED);
		if (ret == 0) {
			if (child_info.si_code == CLD_EXITED) {
				// Exited
				exit_code = child_info.si_status;
			} else {
				// Canceled
				exit_code = 0;	// Set success
			}
		}

		if (celem->state != MANAGER_WORKER_STATE_CANCELED) {
			// When this elem was not canceled, it set complete state.
			celem->state = MANAGER_WORKER_STATE_COMPLETE;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "manager_worker_exec %s recovery complete.\n", celem->blockdev[0]);
			#endif
		}

		ret = manager_worker_set_and_test_cancel(wos, -1);
		if (ret == 1) {
			// All object was canceled or completed. Go out list operation.
			break;
		}
	}

	// Remove canceled elem
	dl_list_for_each_safe(celem, celem_n, &wos->mount_list, container_manager_operation_mount_elem_t, list) {
		if (celem->state == MANAGER_WORKER_STATE_CANCELED) {
			dl_list_del(&celem->list);
			(void) manager_operation_mount_elem_free(celem);
		}
	}

	result = 0;

do_return:

	return result;
}

/**
 * Thread entry point for container manager worker thread.
 *
 * @param [in]	args	Pointer to own container_workqueue_t.
 * @return void*	Will not return.
 */
static void* container_manager_worker_thread(void *args)
{
	int ret = -1;
	worker_operation_storage_t *wos = (worker_operation_storage_t*)args;

	if (args == NULL) {
		pthread_exit(NULL);
	}

	ret = manager_mount_operation(wos->worker_fd, wos, 0);
	if (ret == -1) {
		// Fail safe operation.
		(void) manager_worker_exec(wos->worker_fd, wos);
		// Retake mount
		(void) manager_mount_operation(wos->worker_fd, wos, 1);
	}

	// End of ops
	(void) manager_operation_delayed_storage_list_free(wos);
	(void) close(wos->worker_fd);
	(void) free(wos);

	pthread_exit(NULL);

	return NULL;
}
/**
 * Run container manager delayed operation.
 *
 * @param [in]	cmos	Pointer to initialized container_manager_operation_storage_t.
 * @param [in]	wos		Pointer to initialized worker_operation_storage_t.
 * @return int
 * @retval 0	Success to schedule.
 * @retval -1	Is not scheduled workqueue.
 * @retval -2	Arg. error.
 * @retval -3	Internal error.
 */
static int manager_operation_thread_dispatch(container_manager_operation_storage_t *cmos, worker_operation_storage_t *wos)
{
	int ret = -1;
	pthread_attr_t thread_attr;

	if (wos == NULL) {
		return -2;
	}

	(void) pthread_attr_init(&thread_attr);
	(void) pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&(cmos->worker_thread), &thread_attr, container_manager_worker_thread, (void*)wos);
	(void) pthread_attr_destroy(&thread_attr);
	if (ret < 0) {
		return -3;
	}

	return 0;
}
/**
 * Run scheduled per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval 1	Success to schedule (No operation).
 * @retval 0	Success to schedule.
 * @retval -1	Is not scheduled workqueue.
 * @retval -2	Arg. error.
 * @retval -3	Internal error.
 */
int manager_operation_delayed_launch(containers_t *cs)
{
	container_manager_operation_t *cmo = NULL;
	container_manager_operation_mount_elem_t *cmom_elem = NULL;
	container_manager_operation_storage_t *cmos = NULL;
	worker_operation_storage_t *wos = NULL;
	int pairfd[2] = {-1,-1};
	int result = -1, ret = -1;

	cmo = &cs->cmcfg->operation;
	if (cmo->storage != NULL) {
		// Already executed (why??)
		result = -1;
		return result;
	}

	cmos = (container_manager_operation_storage_t*)malloc(sizeof(container_manager_operation_storage_t));
	if (cmos == NULL) {
		result = -2;
		goto err_return;
	}

	wos = (worker_operation_storage_t*)malloc(sizeof(worker_operation_storage_t));
	if (wos == NULL) {
		result = -2;
		goto err_return;
	}

	(void) memset(wos, 0, sizeof(worker_operation_storage_t));
	dl_list_init(&wos->mount_list);

	// Create worker communication socket.
	ret = socketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK, AF_UNIX, pairfd);
	if (ret < 0) {
		result = -2;
		goto err_return;
	}

	dl_list_for_each(cmom_elem, &cmo->mount.mount_list, container_manager_operation_mount_elem_t, list) {
		if (cmom_elem->is_mounted == 0 && cmom_elem->state != MANAGER_WORKER_STATE_CANCELED) {
			// Not mounted
			container_manager_operation_mount_elem_t *cmom_elem_worker = NULL;

			cmom_elem_worker = (container_manager_operation_mount_elem_t*)malloc(sizeof(container_manager_operation_mount_elem_t));
			if (cmom_elem_worker == NULL) {
				// malloc error
				result = -3;
				goto err_return;
			}
			(void) memset(cmom_elem_worker, 0, sizeof(container_manager_operation_mount_elem_t));

			cmom_elem->is_dispatched = 1;

			dl_list_init(&cmom_elem_worker->list);
			cmom_elem_worker->type = cmom_elem->type;
			cmom_elem_worker->to = strdup(cmom_elem->to);
			cmom_elem_worker->filesystem = strdup(cmom_elem->filesystem);
			cmom_elem_worker->mode = cmom_elem->mode;
			if (cmom_elem->option != NULL) {
				cmom_elem_worker->option = strdup(cmom_elem->option);
			}
			cmom_elem_worker->redundancy = cmom_elem->redundancy;
			cmom_elem_worker->blockdev[0] = strdup(cmom_elem->blockdev[0]);
			if (cmom_elem->blockdev[1] != NULL) {
				cmom_elem_worker->blockdev[1] = strdup(cmom_elem->blockdev[1]);
			}
			cmom_elem_worker->index = cmom_elem->index;
			cmom_elem_worker->is_mounted = cmom_elem->is_mounted;
			cmom_elem_worker->error_count = cmom_elem->error_count;
			cmom_elem_worker->state = MANAGER_WORKER_STATE_QUEUED;

			dl_list_add_tail(&wos->mount_list, &cmom_elem_worker->list);
		}
	}

	if (dl_list_empty(&wos->mount_list)) {
		// No need operations. Success this, but need to free memory.
		result = 1;
		goto err_return;
	}

	wos->worker_fd = pairfd[1];
	cmos->host_fd = pairfd[0];
	cmos->worker_fd = pairfd[1];

	// Create worker thread
	ret = manager_operation_thread_dispatch(cmos, wos);
	if (ret < 0) {
		result = -3;
		goto err_return;
	}

	cmo->storage = cmos;

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "Do manager_operation_delayed_launch.\n");
	#endif

	return 0;

err_return:
	cmo->storage = NULL;

	if (wos != NULL) {
		(void) manager_operation_delayed_storage_list_free(wos);
	}
	free(wos);
	free(cmos);
	if (pairfd[0] != -1) {
		(void) close(pairfd[0]);
	}
	if (pairfd[1] != -1) {
		(void) close(pairfd[1]);
	}

	return result;
}
/**
 * Poll status for the container manager worker.
 *
 * @param [in]	cs	Pointer to initialized containers_t.
 * @return int
 * @retval 1	Worker is completed.
 * @retval 0	Worker is running.
 * @retval -1	Worker is not running.
 * @retval -2	Arg. error.
 * @retval -3	Internal error.
 */
int manager_operation_delayed_poll(containers_t *cs)
{
	container_manager_operation_t *cmo = NULL;
	container_manager_operation_mount_elem_t *cmom_elem = NULL;
	worker_response_t wres;
	ssize_t sret = -1;
	int result = 0;
	int num_of_elem = 0, num_of_complete = 0;

	cmo = &cs->cmcfg->operation;
	if (cmo->storage == NULL) {
		// Not running worker
		result = -1;
		goto do_return;
	}
	// When running worker.
	(void) memset(&wres, 0, sizeof(wres));
	sret = read(cmo->storage->host_fd, &wres, sizeof(wres));
	if (sret < 0) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			// Not get packet
			result = 0;
			goto do_asses;
		} else {
			// Abnormal error
			result = -3;
			goto do_return;
		}
	}

	dl_list_for_each(cmom_elem, &cmo->mount.mount_list, container_manager_operation_mount_elem_t, list) {
		if (cmom_elem->index == wres.index) {
			// Match index
			if (wres.operation == 0) {
				// Mount operation
				if (wres.result == 0) {
					// mounted
					cmom_elem->is_mounted = 1;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "manager_operation_delayed_poll got mounted %s to %s.\n", cmom_elem->blockdev[0], cmom_elem->to);
					#endif
				} else if (wres.result == 1) {
					// canceled
					cmom_elem->is_mounted = 0;
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout, "manager_operation_delayed_poll got canceled %s to %s.\n", cmom_elem->blockdev[0], cmom_elem->to);
					#endif
				} else {
					// error
					cmom_elem->error_count++;
				}
				// Already completed worker operation
				cmom_elem->is_dispatched = 0;
			} else if (wres.operation == 1) {
				// Unmount operation
				cmom_elem->is_mounted = 0;
				// Already completed worker operation
				cmom_elem->is_dispatched = 0;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "manager_operation_delayed_poll got unmounted %s from %s.\n", cmom_elem->blockdev[0], cmom_elem->to);
				#endif
			} else {
				;	//nop
			}
		}
	}

do_asses:
	// Test a complete or not complete worker operations
	if (!dl_list_empty(&cmo->mount.mount_list)) {
		dl_list_for_each(cmom_elem, &cmo->mount.mount_list, container_manager_operation_mount_elem_t, list) {
			num_of_elem++;
			if (cmom_elem->is_dispatched == 0) {
				num_of_complete++;
			}
		}
		if (num_of_elem == num_of_complete) {
			(void)close(cmo->storage->host_fd);
			(void)free(cmo->storage);
			cmo->storage = NULL;
			result = 1;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "End of all delayed mount worker.\n");
			#endif
		}
	}

do_return:
	return result;
}
/**
 * Run scheduled per container workqueue.
 *
 * @param [in]	workqueue	Pointer to initialized container_workqueue_t.
 * @return int
 * @retval 1	Success to schedule (No operation).
 * @retval 0	Success to schedule.
 * @retval -1	Already executed.
 * @retval -2	Internal error.
 * @retval -3	Internal error.
 */
int manager_operation_delayed_terminate(containers_t *cs)
{
	container_manager_operation_t *cmo = NULL;
	container_manager_operation_mount_elem_t *cmom_elem = NULL;
	container_manager_operation_storage_t *cmos = NULL;
	worker_operation_storage_t *wos = NULL;
	int pairfd[2] = {-1,-1};
	int result = -1, ret = -1;

	cmo = &cs->cmcfg->operation;
	if (cmo->storage != NULL) {
		// Already executed.
		result = -1;
		return result;
	}

	cmos = (container_manager_operation_storage_t*)malloc(sizeof(container_manager_operation_storage_t));
	if (cmos == NULL) {
		result = -2;
		goto err_return;
	}

	wos = (worker_operation_storage_t*)malloc(sizeof(worker_operation_storage_t));
	if (wos == NULL) {
		result = -2;
		goto err_return;
	}

	(void) memset(wos, 0, sizeof(worker_operation_storage_t));
	dl_list_init(&wos->mount_list);

	// Create worker communication socket.
	ret = socketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK, AF_UNIX, pairfd);
	if (ret < 0) {
		result = -2;
		goto err_return;
	}

	dl_list_for_each(cmom_elem, &cmo->mount.mount_list, container_manager_operation_mount_elem_t, list) {
		if (cmom_elem->is_mounted == 1 && cmom_elem->state != MANAGER_WORKER_STATE_CANCELED) {
			// Need unmount
			container_manager_operation_mount_elem_t *cmom_elem_worker = NULL;

			cmom_elem_worker = (container_manager_operation_mount_elem_t*)malloc(sizeof(container_manager_operation_mount_elem_t));
			if (cmom_elem_worker == NULL) {
				// malloc error
				result = -3;
				goto err_return;
			}
			(void) memset(cmom_elem_worker, 0, sizeof(container_manager_operation_mount_elem_t));

			cmom_elem->is_dispatched = 1;

			dl_list_init(&cmom_elem_worker->list);
			cmom_elem_worker->type = cmom_elem->type;
			cmom_elem_worker->to = strdup(cmom_elem->to);
			cmom_elem_worker->filesystem = strdup(cmom_elem->filesystem);
			cmom_elem_worker->mode = cmom_elem->mode;
			if (cmom_elem->option != NULL) {
				cmom_elem_worker->option = strdup(cmom_elem->option);
			}
			cmom_elem_worker->redundancy = cmom_elem->redundancy;
			cmom_elem_worker->blockdev[0] = strdup(cmom_elem->blockdev[0]);
			if (cmom_elem->blockdev[1] != NULL) {
				cmom_elem_worker->blockdev[1] = strdup(cmom_elem->blockdev[1]);
			}
			cmom_elem_worker->index = cmom_elem->index;
			cmom_elem_worker->is_mounted = cmom_elem->is_mounted;
			cmom_elem_worker->error_count = cmom_elem->error_count;
			cmom_elem_worker->state = MANAGER_WORKER_STATE_QUEUED;

			dl_list_add_tail(&wos->mount_list, &cmom_elem_worker->list);
		}
	}

	if (dl_list_empty(&wos->mount_list)) {
		// No need operations. Success this, but need to free memory.
		result = 1;
		goto err_return;
	}

	wos->worker_fd = pairfd[1];
	cmos->host_fd = pairfd[0];
	cmos->worker_fd = pairfd[1];

	// Create worker thread
	ret = manager_operation_thread_dispatch(cmos, wos);
	if (ret < 0) {
		result = -3;
		goto err_return;
	}

	cmo->storage = cmos;

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "Do manager_operation_delayed_terminate.\n");
	#endif

	return 0;

err_return:
	cmo->storage = NULL;

	if (wos != NULL) {
		(void) manager_operation_delayed_storage_list_free(wos);
	}
	free(wos);
	free(cmos);
	if (pairfd[0] != -1) {
		(void) close(pairfd[0]);
	}
	if (pairfd[1] != -1) {
		(void) close(pairfd[1]);
	}

	return result;
}

/**
 * Memory free for container_manager_operation_mount_elem_t.
 *
 * @param [in]	celem	Pointer to container_manager_operation_mount_elem_t.
 * @return int
 * @retval 0	Success to free.
 * @retval -1	Arg. error.
 * @retval -2	Internal error.
 */
static int manager_operation_mount_elem_free(container_manager_operation_mount_elem_t *celem)
{
	if (celem == NULL) {
		return -1;
	}

	(void) free(celem->to);
	(void) free(celem->filesystem);
	(void) free(celem->option);
	(void) free(celem->blockdev[0]);
	(void) free(celem->blockdev[1]);
	(void) free(celem);

	return 0;
}
/**
 * Memory free for worker_operation_storage_t.
 *
 * @param [in]	wos	Pointer to worker_operation_storage_t.
 * @return int
 * @retval 0	Success to free.
 * @retval -1	Arg. error.
 * @retval -2	Internal error.
 */
static int manager_operation_delayed_storage_list_free(worker_operation_storage_t *wos)
{
	container_manager_operation_mount_elem_t *cmom_elem_worker = NULL;

	if (wos == NULL) {
		return -1;
	}

	while(dl_list_empty(&wos->mount_list) == 0) {
		cmom_elem_worker = dl_list_last(&wos->mount_list, container_manager_operation_mount_elem_t, list);
		dl_list_del(&cmom_elem_worker->list);
		(void) manager_operation_mount_elem_free(cmom_elem_worker);
	}

	return 0;
}