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
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <limits.h>

#include "container.h"
#include "cm-utils.h"
#include "container-manager-operations.h"

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
 * @struct	struct s_worker_request
 * @brief	The request data packet.
 */
struct s_worker_request {
	int request;	/**< Operation request. 0: none, 1: cancel.*/
};
typedef struct s_worker_request worker_request_t;	/**< typedef for struct s_worker_request. */
/**
 * @struct	struct s_worker_response
 * @brief	The response data packet.
 */
struct s_worker_response {
	int result;		/**< Result code for task completed object. 0: full complete, 1: cancel, -1: error. */
};
typedef struct s_worker_response worker_response_t;	/**< typedef for struct s_worker_response. */

/**
 * @def	MANAGER_WORKER_TASK_NOP
 * @brief	The no operation code for the simple manager worker task.
 */
#define MANAGER_WORKER_TASK_NOP			(0)
/**
 * @def	MANAGER_WORKER_TASK_MOUNT_FSCK
 * @brief	The mount operation code for the simple manager worker task. Fail action is fsck.
 */
#define MANAGER_WORKER_TASK_MOUNT_FSCK		(10)
/**
 * @def	MANAGER_WORKER_TASK_MOUNT_MKFS
 * @brief	The mount operation code for the simple manager worker task. Fail action is mkfs.
 */
#define MANAGER_WORKER_TASK_MOUNT_MKFS		(11)
/**
 * @def	MANAGER_WORKER_TASK_UNMOUNT
 * @brief	The unmount operation code for the simple manager worker task.
 */
#define MANAGER_WORKER_TASK_UNMOUNT		(20)
/**
 * @def	MANAGER_WORKER_TASK_ERASE
 * @brief	The erase operation for the simple manager worker task. It's combinations with mkfs.
 */
#define MANAGER_WORKER_TASK_ERASE		(30)

/**
 * @def	MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE
 * @brief	The operation type for the start time.
 */
#define MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE		(0)
/**
 * @def	MANAGER_WORKER_RUNTIME_STATE_DISPATCHED
 * @brief	The operation type for the start time.
 */
#define MANAGER_WORKER_RUNTIME_STATE_DISPATCHED		(1)
/**
 * @def	MANAGER_WORKER_RUNTIME_STATE_COMPLETED
 * @brief	The operation type for the start time.
 */
#define MANAGER_WORKER_RUNTIME_STATE_COMPLETED		(0)

struct s_worker_task_elem {
	int task_type;				/* MANAGER_WORKER_TASK_* */
	unsigned int op_type;				/* MANAGER_WORKER_OPERATION_TYPE_* */
	char *device;				/* for ALL operation */
	char *mount_point;			/* for mount operation */
	char *fs_option_str;
	unsigned long mount_flag;	/* for mount operation */

	//-- runtime use. Shall not set const table.
	int state;
	int error_count;
};
typedef struct s_worker_task_elem worker_task_elem_t;	/**< typedef for struct s_worker_task_elem. */

/**
 * @struct	s_container_manager_operation_storage
 * @brief	The data storage for manager operations.
 */
struct s_worker_operation_storage {
	worker_task_elem_t **task_states;	/**< Double link list for worker operation. */
	int num_of_elem;

	int worker_fd;		/**< Socket fd for worker. */
};
typedef struct s_worker_operation_storage worker_operation_storage_t;	/**< typedef for struct s_worker_operation_storage. */

static const worker_task_elem_t g_worker_task_elems[5] = {
	[0] = {
		.task_type = MANAGER_WORKER_TASK_MOUNT_FSCK,
		.op_type = MANAGER_WORKER_OPERATION_TYPE_START,
		.device = "/dev/mmcblk0p1",
		.mount_point = "/var/nv1",
		.fs_option_str = NULL,
		.mount_flag = (MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS),
	},
	[1] = {
		.task_type = MANAGER_WORKER_TASK_MOUNT_MKFS,
		.op_type = MANAGER_WORKER_OPERATION_TYPE_START,
		.device = "/dev/mmcblk0p2",
		.mount_point = "/var/nv2",
		.fs_option_str = NULL,
		.mount_flag = (MS_DIRSYNC | MS_NOATIME | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS),
	},
	[2] = {
		.task_type = MANAGER_WORKER_TASK_UNMOUNT,
		.op_type = MANAGER_WORKER_OPERATION_TYPE_TERMINATE,
		.device = "/dev/mmcblk0p1",
		.mount_point = "/var/nv1",
		.fs_option_str = NULL,
		.mount_flag = 0,
	},
	[3] = {
		.task_type = MANAGER_WORKER_TASK_UNMOUNT,
		.op_type = MANAGER_WORKER_OPERATION_TYPE_TERMINATE,
		.device = "/dev/mmcblk0p2",
		.mount_point = "/var/nv2",
		.fs_option_str = NULL,
		.mount_flag = 0,
	},
	[4] = {
		.task_type = MANAGER_WORKER_TASK_ERASE,
		.op_type = MANAGER_WORKER_OPERATION_TYPE_TERMINATE_EXT,
		.device = "/dev/mmcblk0p2",
		.mount_point = NULL,
		.fs_option_str = NULL,
		.mount_flag = 0,
	},
};
#define NUM_OF_WORKER_TASK_TABLE	(sizeof(g_worker_task_elems) / sizeof(g_worker_task_elems[0]))

static int manager_operation_delayed_storage_table(worker_operation_storage_t *wos);

/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 1	Got a cancel worker.
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int manager_worker_child_exec(int control_fd, int is_mkfs, const char *device)
{
	struct pollfd waiter[2];
	siginfo_t child_info;
	int result = -1;
	int ret = -1;
	int exit_code = -1;
	int child_fd = -1;
	pid_t child_pid = -1;

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "manager_worker_exec: do %s operation.\n", device);
	#endif

	child_pid = fork();
	if (child_pid < 0) {
		// Fail to fork
		result = -1;
		goto do_return;
	}

	if (child_pid == 0) {
		if (is_mkfs == 1) {
			// exec /sbin/fsck.ext4 -p
			(void) execlp("/sbin/mkfs.ext4", "/sbin/mkfs.ext4", "-F", "-I", "256", device, (char*)NULL);
		} else {
			// exec /sbin/fsck.ext4 -p
			(void) execlp("/sbin/fsck.ext4", "/sbin/fsck.ext4", "-p", device, (char*)NULL);
		}
		// Shall not return execlp
		(void) _exit(128);
	}

	#ifdef _PRINTF_DEBUG_
	if (is_mkfs == 1) {
		(void) fprintf(stdout, "manager_worker_exec mkfs fork and exec mkfs.ext4 pid=%d\n",(int)child_pid);
	} else {
		(void) fprintf(stdout, "manager_worker_exec fsck fork and exec fsck.ext4 pid=%d\n",(int)child_pid);
	}
	#endif

	child_fd = pidfd_open_syscall_wrapper(child_pid);
	// Fail to create pidfd, it handled error case of poll.

	(void) memset(waiter, 0, sizeof(waiter));
	waiter[0].fd = child_fd;
	waiter[0].events = POLLIN;
	waiter[1].fd = control_fd;
	waiter[1].events = POLLIN | POLLERR;

	do {
		ret = poll(waiter, 2, 100);	//100ms timeout
		if (ret > 0) {
			// Got a event
			if (waiter[0].revents != 0) {
				// child process was exited.
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "manager_worker_exec got a exit child process.\n");
				#endif
				result = 0;
				break;
			}

			if (waiter[1].revents != 0) {
				// Host message is only a cancel.

				// Do cancel operation
				ret = pidfd_send_signal_syscall_wrapper(child_fd, SIGTERM, NULL, 0);
				if (ret < 0) {
					(void) kill(child_pid, SIGTERM);
				}
				result = 1;
				break;
			}
		} else if (ret == 0) {
			// Timeout, test to canceled.
			worker_request_t wreq;
			ssize_t sret = -1;

			sret = read(control_fd, &wreq, sizeof(wreq));
			if (sret == 0) {
				// host fd was closed.
				// Do cancel operation
				ret = pidfd_send_signal_syscall_wrapper(child_fd, SIGTERM, NULL, 0);
				if (ret < 0) {
					(void) kill(child_pid, SIGTERM);
				}
				result = 1;
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

do_return:

	return result;
}
/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 1	Got a cancel worker.
 * @retval 0	Success to execute worker.
 * @retval -1	Need to optional worker.
 */
static int manager_mount_operation(worker_operation_storage_t *wos)
{
	int result = 0, ret = -1;

	// 1st mount try.
	for (size_t i=0; i < wos->num_of_elem; i++) {
		worker_task_elem_t *wte = wos->task_states[i];
		if (wte != NULL) {
			if (wte->state == MANAGER_WORKER_RUNTIME_STATE_DISPATCHED) {
				if ((wte->task_type == MANAGER_WORKER_TASK_MOUNT_FSCK) || (wte->task_type == MANAGER_WORKER_TASK_MOUNT_MKFS) ){
					// do mount operation
					char *devs[2];
					devs[0] = wte->device;
					devs[1] = NULL;
					ret = mount_disk_once(devs, wte->mount_point, "ext4", wte->mount_flag, wte->fs_option_str);
					if (ret == 0){
						// Success to mount
						wte->state = MANAGER_WORKER_RUNTIME_STATE_COMPLETED;
					} else {
						wte->error_count++;
					}
				}
			}
		}
	}
	// 1st mount try completed.

	// Try to recover.
	for (size_t i=0; i < wos->num_of_elem; i++) {
		worker_task_elem_t *wte = wos->task_states[i];
		if (wte != NULL) {
			if (wte->state == MANAGER_WORKER_RUNTIME_STATE_DISPATCHED) {
				if (wte->task_type == MANAGER_WORKER_TASK_MOUNT_FSCK) {
					ret = manager_worker_child_exec(wos->worker_fd, 0, wte->device);
				} else if (wte->task_type == MANAGER_WORKER_TASK_MOUNT_MKFS) {
					ret = manager_worker_child_exec(wos->worker_fd, 1, wte->device);
				} else {
					ret = 0;	//Fake success.
				}

				if (ret == 1) {
					//Need to cancel worker.
					result = 1;
					goto do_return;
				}
			}
		}
	}

	// 2nd mount try.
	for (size_t i=0; i < wos->num_of_elem; i++) {
		worker_task_elem_t *wte = wos->task_states[i];
		if (wte != NULL) {
			if (wte->state == MANAGER_WORKER_RUNTIME_STATE_DISPATCHED) {
				if ((wte->task_type == MANAGER_WORKER_TASK_MOUNT_FSCK) || (wte->task_type == MANAGER_WORKER_TASK_MOUNT_MKFS) ){
					// do mount operation
					char *devs[2];
					devs[0] = wte->device;
					devs[1] = NULL;
					ret = mount_disk_once(devs, wte->mount_point, "ext4", wte->mount_flag, wte->fs_option_str);
					if (ret == 0){
						// Success to mount
						wte->state = MANAGER_WORKER_RUNTIME_STATE_COMPLETED;
					} else {
						wte->error_count++;
						// Stop exec.
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
					}
				}
			}
		}
	}
	// 2nd mount try completed.

do_return:

	return result;
}
/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 */
static int manager_unmount_operation(worker_operation_storage_t *wos)
{
	int result = 0, ret = -1;

	// unmount try.
	for (size_t i=0; i < wos->num_of_elem; i++) {
		worker_task_elem_t *wte = wos->task_states[i];
		if (wte != NULL) {
			if (wte->state == MANAGER_WORKER_RUNTIME_STATE_DISPATCHED) {
				if (wte->task_type == MANAGER_WORKER_TASK_UNMOUNT) {
					int64_t timeout_time = 0;
					int retry_max = 0;

					// 3s timeout.
					timeout_time = get_current_time_ms() + 3000LL;
					retry_max = (3000 / 50) + 1;

					ret = unmount_disk(wte->mount_point, timeout_time, retry_max);
					if (ret == 0){
						// Success to unmount
						wte->state = MANAGER_WORKER_RUNTIME_STATE_COMPLETED;
					} else {
						wte->error_count++;
						// Stop exec.
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
					}
				}
			}
		}
	}
	// unmount try completed.

	return result;
}

static const char *cstr_block_device_test_base = "/sys/fs/ext4/";
/**
 * @brief Function for unmont wait.
 *
 * @param [in]	permkfs		Initialized erase_mkfs_plugin_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int manager_wait_unmount(worker_task_elem_t *wte)
{
	int ret = -1, result = -1;
	ssize_t slen = 0, buflen = 0;
	char test_path[PATH_MAX];
	const char *devname = NULL;
	int retry_count = (5000 / 100);	//5000ms

	// test to /sys/fs/ext4/block-device-name
	devname = trimmed_devname(wte->device);
	if (devname == NULL) {
		// permkfs->blkdev_path is not device name.
		result = -1;
		goto do_return;
	}

	test_path[0] = '\0';
	buflen = (ssize_t)sizeof(test_path) - 1;

	slen = (ssize_t)snprintf(test_path, buflen, "%s%s", cstr_block_device_test_base, devname);
	if (slen >= buflen) {
		//May not cause this error.
		result = -1;
		goto do_return;
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "manager_wait_unmount test to %s\n",test_path);
	#endif

	result = -1;
	do {
		ret = node_check(test_path);
		if (ret == -1) {
			// Already unmounted
			result = 0;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "manager_wait_unmount unmounted at %s\n",test_path);
			#endif
			break;
		}

		sleep_ms_time(100LL);
		retry_count--;
	} while(retry_count > 0);

	#ifdef _PRINTF_DEBUG_
	if (result == -1) {
		(void) fprintf(stdout, "manager_wait_unmount not unmounted at %s\n",test_path);
	}
	#endif

do_return:

	return result;
}
/**
 * @var		g_erase_buff
 * @brief	Work buffer for disk erase.
 */
#define ERASE_BUFFER_SIZE	(8u*1024u*1024u)	// 8MByte buffer
/**
 * @brief Function for disk erase execution.
 *
 * @param [in]	permkfs		Initialized erase_mkfs_plugin_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int manager_exec_erase(worker_task_elem_t *wte)
{
	int fd = -1;
	int result = -1;
	uint64_t *erase_buff = NULL;

	erase_buff = (uint64_t*)malloc(ERASE_BUFFER_SIZE);
	if (erase_buff == NULL) {
		result = -1;
		goto do_return;
	}
	(void) memset(erase_buff, 0, ERASE_BUFFER_SIZE);

	fd = open(wte->device, O_CLOEXEC | O_SYNC | O_WRONLY);
	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "manager_exec_erase: fd=%d\n",fd);
	#endif
	if (fd >= 0) {
		ssize_t sret = -1;

		do {
			sret = write(fd, erase_buff, ERASE_BUFFER_SIZE);
		} while((sret > 0) && (errno != EINTR));

		// Finally, sret = -1 and errno = 28(ENOSPC).
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"manager_exec_erase: write end for %s ret = %d(%d)\n",wte->device, (int)sret, errno);
		#endif

		result = 0;
	}

do_return:
	if (fd >= 0) {
		(void) close(fd);
	}
	(void) free(erase_buff);

	return result;
}
/**
 * @brief Function for fsck operation.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 */
static int manager_erase_operation(worker_operation_storage_t *wos)
{
	int result = 0, ret = -1;

	// unmount try.
	for (size_t i=0; i < wos->num_of_elem; i++) {
		worker_task_elem_t *wte = wos->task_states[i];
		if (wte != NULL) {
			if (wte->state == MANAGER_WORKER_RUNTIME_STATE_DISPATCHED) {
				if (wte->task_type == MANAGER_WORKER_TASK_ERASE) {
					// unmount
					ret = manager_wait_unmount(wte);
					if (ret < 0) {
						wte->error_count++;
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
						result = -1;
						continue; //Skip erase.
					}

					// erase
					ret = manager_exec_erase(wte);
					if (ret < 0) {
						wte->error_count++;
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
						result = -1;
						continue; //Skip mkfs.
					}

					// mkfs
					ret = manager_worker_child_exec(wos->worker_fd, 1, wte->device);
					if (ret == 0) {
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
						result = 1;
					} else if (ret < 0) {
						wte->error_count++;
						wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
						result = -1;
						continue;
					} else {
						; //nop
					}

					wte->state = MANAGER_WORKER_RUNTIME_STATE_COMPLETED;
				}
			}
		}
	}
	// unmount try completed.

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
	worker_response_t wres;

	if (args == NULL) {
		pthread_exit(NULL);
	}

	ret = manager_mount_operation(wos);
	if (ret == 1) {
		// need to cancel worker.
		wres.result = 1;
		goto do_exit_worker;
	}

	(void) manager_unmount_operation(wos);

	ret = manager_erase_operation(wos);
	if (ret < 0) {
		// Fail worker.
		wres.result = -1;
		goto do_exit_worker;
	}
	wres.result = 0;

do_exit_worker:
	// End of ops
	(void) intr_safe_write(wos->worker_fd, &wres, sizeof(wres));

	(void) manager_operation_delayed_storage_table(wos);
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
int manager_operation_delayed_launch(containers_t *cs, unsigned int op_type)
{
	container_manager_operation_t *cmop = NULL;
	container_manager_operation_storage_t *cmos = NULL;
	worker_operation_storage_t *wos = NULL;
	worker_task_elem_t **wtes = NULL;
	int pairfd[2] = {-1,-1};
	int result = -1, ret = -1;

	cmop = &cs->cmop;	//cmop is not NULL everywhere.
	if (cmop->storage != NULL) {
		// Already executed.
		result = -1;
		return result;
	}

	cmos = (container_manager_operation_storage_t*)malloc(sizeof(container_manager_operation_storage_t));
	if (cmos == NULL) {
		result = -2;
		goto err_return;
	}
	(void) memset(cmos, 0, sizeof(container_manager_operation_storage_t));

	wos = (worker_operation_storage_t*)malloc(sizeof(worker_operation_storage_t));
	if (wos == NULL) {
		result = -2;
		goto err_return;
	}
	(void) memset(wos, 0, sizeof(worker_operation_storage_t));

	// Create worker communication socket.
	ret = socketpair(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK, AF_UNIX, pairfd);
	if (ret < 0) {
		result = -2;
		goto err_return;
	}

	// Create dispatch task table
	wtes = (worker_task_elem_t **)malloc(sizeof(worker_task_elem_t*) * NUM_OF_WORKER_TASK_TABLE);
	if (wtes == NULL) {
		result = -2;
		goto err_return;
	}
	(void) memset(wtes, 0, (sizeof(worker_task_elem_t*) * NUM_OF_WORKER_TASK_TABLE));

	for (size_t i=0; i < NUM_OF_WORKER_TASK_TABLE; i++) {
		worker_task_elem_t *wte = (worker_task_elem_t *)malloc(sizeof(worker_task_elem_t));
		if (wte == NULL) {
			result = -2;
			goto err_return;
		}
		// Set table data
		wte->task_type = g_worker_task_elems[i].task_type;
		wte->op_type = g_worker_task_elems[i].op_type;
		wte->device = g_worker_task_elems[i].device;
		wte->mount_point = g_worker_task_elems[i].mount_point;
		wte->fs_option_str = g_worker_task_elems[i].fs_option_str;
		wte->mount_flag = g_worker_task_elems[i].mount_flag;

		// Set runtime data
		if ((wte->op_type & op_type) != 0) {
			wte->state = MANAGER_WORKER_RUNTIME_STATE_DISPATCHED;
		} else {
			wte->state = MANAGER_WORKER_RUNTIME_STATE_NO_EXECUTE;
		}
		wte->error_count = 0;

		wtes[i] = wte;
	}

	wos->task_states = wtes;
	wos->num_of_elem = NUM_OF_WORKER_TASK_TABLE;
	wos->worker_fd = pairfd[1];
	cmos->host_fd = pairfd[0];
	cmos->worker_fd = pairfd[1];

	// Create worker thread
	ret = manager_operation_thread_dispatch(cmos, wos);
	if (ret < 0) {
		result = -3;
		goto err_return;
	}

	cmop->storage = cmos;

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "Do manager_operation_delayed_launch.\n");
	#endif

	return 0;

err_return:
	cmop->storage = NULL;

	if (wtes != NULL) {
		for(size_t i=0; i < NUM_OF_WORKER_TASK_TABLE; i++) {
			free(wtes[i]);
		}
		free(wtes);
		wtes = NULL;
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
 * @retval -2	Worker fail.
 * @retval -3	Internal error.
 */
int manager_operation_delayed_poll(containers_t *cs)
{
	container_manager_operation_t *cmop = NULL;
	worker_response_t wres;
	ssize_t sret = -1;
	int result = 0;

	cmop = &cs->cmop;	//cmop is not NULL everywhere.
	if (cmop->storage == NULL) {
		// Not running worker
		result = -1;
		goto do_return;
	}

	// When running worker.
	(void) memset(&wres, 0, sizeof(wres));
	sret = read(cmop->storage->host_fd, &wres, sizeof(wres));
	if (sret < 0) {
		if ((errno == EAGAIN) || (errno == EINTR)) {
			// Not get packet
			result = 0;
			goto do_return;
		} else {
			// Abnormal error
			result = -3;
			goto do_return;
		}
	}

	if (wres.result == 0) {
		// Got a complete response from worker.
		(void)close(cmop->storage->host_fd);
		(void)free(cmop->storage);
		cmop->storage = NULL;
		result = 1;
	} else if (wres.result == 1) {
		// Got a cancel response from worker.
		(void)close(cmop->storage->host_fd);
		(void)free(cmop->storage);
		cmop->storage = NULL;
		result = 1;
	} else if ((wres.result == -1) || (sret == 0)) {
		// Got a error response from worker or Socket was closed by worker.
		(void)close(cmop->storage->host_fd);
		(void)free(cmop->storage);
		cmop->storage = NULL;
		result = 1;
	} else {
		//undefined state.
		result = -3;
	}

do_return:
	return result;
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
static int manager_operation_delayed_storage_table(worker_operation_storage_t *wos)
{
	worker_task_elem_t **wtes;

	if (wos == NULL) {
		return -1;
	}

	wtes = wos->task_states;
	if (wtes != NULL) {
		for(size_t i=0; i < NUM_OF_WORKER_TASK_TABLE; i++) {
			free(wtes[i]);
		}
		free(wtes);
	}
	wos->task_states = NULL;

	return 0;
}