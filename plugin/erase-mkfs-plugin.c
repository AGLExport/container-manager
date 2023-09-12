/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	container-workqueue-worker.c
 * @brief	This file include implementation of erase and mkfs plugin.
 */

#include "worker-plugin-interface.h"
#include "libs/cm-worker-utils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

/**
 * @struct	s_erase_mkfs_plugin
 * @brief	The data structure for container manager workqueue worker instance.
 */
struct s_erase_mkfs_plugin {
	char *blkdev_path;
	int cancel_request;
};
typedef struct s_erase_mkfs_plugin erase_mkfs_plugin_t;	/**< typedef for struct s_cm_worker_instance. */

/**
 * @var		cstr_option_device
 * @brief	default signal to use guest container termination.
 */
static const char *cstr_option_device = "device=";
/**
 * @brief Function for argument set to erase and mkfs plugin.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @param [in]	arg_str		Pointer to the argument string.
 * @param [in]	arg_length	Length for the argument string.
 * @return Description for return value
 * @retval 0	Success to set argument.
 * @retval -1	Fail to set argument.
 */
static int cm_worker_set_args(cm_worker_handle_t handle, const char *arg_str, size_t arg_length)
{
	erase_mkfs_plugin_t *permkfs = NULL;
	char *substr = NULL, *saveptr = NULL;
	size_t cstr_option_device_length = 0;
	char strbuf[1024];
	int result = -1;

	if ((handle == NULL) || (arg_str == NULL) || (arg_length >= 1024u)) {
		return -1;
	}

	permkfs = (erase_mkfs_plugin_t*)handle;

	(void) strncpy(strbuf, arg_str, (arg_length + 1u));

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"erase-mkfs-plugin: cm_worker_set_args %s (%zu)\n", arg_str, arg_length);
	#endif

	cstr_option_device_length = strlen(cstr_option_device);
	substr = strtok_r(strbuf, " ", &saveptr);
	for(int i=0; i < 1024;i++) {
		if (substr != NULL) {
			if (strncmp(substr, cstr_option_device, cstr_option_device_length) == 0) {
				if (cstr_option_device_length < strlen(substr)) {
					char *device = &substr[cstr_option_device_length];
					size_t len = strlen(device);
					if (len > 0u) {
						permkfs->blkdev_path = strdup(device);
						result = 0;
						#ifdef _PRINTF_DEBUG_
						(void) fprintf(stdout,"erase-mkfs-plugin: cm_worker_set_args set device = %s\n", permkfs->blkdev_path);
						#endif
						break;
					}
				}
			}
			substr = strtok_r(NULL, " ", &saveptr);
		} else {
			result = -1;
			break;
		}
	}

	return result;
}
/**
 * @var		g_erase_buff
 * @brief	Work buffer for disk erase.
 */
static const uint64_t g_erase_buff[1024u*1024u/sizeof(uint64_t)];	// 1MByte buffer
/**
 * @brief Function for disk erase execution.
 *
 * @param [in]	permkfs		Initialized erase_mkfs_plugin_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int cm_worker_exec_erase(erase_mkfs_plugin_t *permkfs)
{
	int fd = -1;
	int ret = -1;

	fd = open(permkfs->blkdev_path, O_CLOEXEC | O_SYNC | O_WRONLY);
	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec_erase fd=%d\n",fd);
	#endif
	if (fd >= 0) {
		ssize_t sret = -1;

		do {
			if (permkfs->cancel_request == 1) {
				// Got cancel request.
				ret = 1;
				goto do_return;
			}
			sret = write(fd, g_erase_buff, sizeof(g_erase_buff));
		} while((sret > 0) && (errno != EINTR));

		// Finally, sret = -1 and errno = 28(ENOSPC).
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"container_worker_test: write end for /dev/mmcblk1p7 ret = %d(%d)\n",(int)sret, errno);
		#endif

		(void) close(fd);

		ret = 0;
	}

do_return:

	return ret;
}
/**
 * @brief Function for disk format execution.
 *
 * @param [in]	permkfs		Initialized erase_mkfs_plugin_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int cm_worker_exec_mkfs(erase_mkfs_plugin_t *permkfs)
{
	struct pollfd waiter[1];
	siginfo_t child_info;
	int result = -1;
	int ret = -1;
	int exit_code = -1;
	int child_fd = -1;
	pid_t child_pid = -1;

	child_pid = fork();
	if (child_pid < 0) {
		// Fail to fork
		result = -1;
		goto do_return;
	}

	if (child_pid == 0) {
		// exec /sbin/fsck.ext4 -p
		(void) execlp("/sbin/mkfs.ext4", "/sbin/mkfs.ext4", "-I", "256", permkfs->blkdev_path, (char*)NULL);

		// Shall not return execlp
		(void) _exit(128);
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec fork and exec mkfs.ext4 pid=%d\n",(int)child_pid);
	#endif

	child_fd = cm_pidfd_open(child_pid);
	if (child_fd < 0) {
		// Fail to fork
		goto do_return;
	}

	(void) memset(waiter, 0, sizeof(struct pollfd)*1u);
	waiter[0].fd = child_fd;
	waiter[0].events = POLLIN;

	do {
		ret = poll(waiter, 1, 100);	//100ms timeout
		if (ret > 0) {
			// Got a event
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec got a exit mkfs.ext4.\n");
			#endif
			break;
		} else if (ret == 0) {
			// Timeout
			if (permkfs->cancel_request == 1) {
				// Need to cancel
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec got a cancel request.\n");
				#endif
				ret = cm_pidfd_send_signal(child_fd, SIGTERM, NULL, 0);
				if (ret < 0) {
					(void) kill(child_pid, SIGTERM);
				}
				break;
			}
		} else {
			if (errno != EINTR) {
				// Can't wait child process.
				int wait_status = 0;

				(void) kill(child_pid, SIGTERM);
				(void) waitpid(child_pid, &wait_status, 0);
				result = -1;
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec fail (%d)\n", (int)errno);
				#endif
				goto do_return;
			}
		}
	} while(1);

	(void) memset(&child_info, 0, sizeof(child_info));

	ret = waitid(P_PID, (id_t)child_pid, &child_info, WEXITED);
	if (ret == 0) {
		if (child_info.si_code == CLD_EXITED) {
			// Exited
			exit_code = child_info.si_status;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec mkfs.ext4 exit = %d\n", exit_code);
			#endif
		} else {
			// Canceled
			exit_code = 0;	// Set success
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec mkfs.ext4 may canceled.\n");
			#endif
		}
	} else {
		result = -1;
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout, "erase-mkfs-plugin: cm_worker_exec waitid fail (%d)\n", (int)errno);
		#endif
		goto do_return;
	}

	if (permkfs->cancel_request == 1) {
		result = 1;
	} else {
		result = 0;
	}

do_return:
	if (child_fd >= 0) {
		(void) close(child_fd);
	}

	return result;
}

/**
 * @brief Function for erase and mkfs plugin worker execution.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to execute worker.
 * @retval -1	Fail to execute worker.
 */
static int cm_worker_exec(cm_worker_handle_t handle)
{
	erase_mkfs_plugin_t *permkfs = NULL;
	int result = -1;
	int ret = -1;

	if (handle == NULL) {
		result = -1;
		goto do_return;
	}
	permkfs = (erase_mkfs_plugin_t*)handle;

	// Do erase
	ret = cm_worker_exec_erase(permkfs);
	if (ret < 0) {
		result = -1;
		goto do_return;
	} else if (ret == 1) {
		// Canceled
		result = 1;
		goto do_return;
	} else {
		result = 0;
	}

	// Do mkfs
	ret = cm_worker_exec_mkfs(permkfs);
	if (ret < 0) {
		result = -1;
	} else if (ret == 1) {
		// Canceled
		result = 1;
		goto do_return;
	} else {
		result = 0;
	}

do_return:

	return result;
}
/**
 * @brief Function pointer for cancel to container workqueue worker.
 *
 * @param [in]	handle		Initialized cm_worker_handle_t.
 * @return Description for return value
 * @retval 0	Success to cancel request to worker.
 * @retval -1	Fail to cancel request to worker.
 */
int cm_worker_cancel(cm_worker_handle_t handle)
{
	erase_mkfs_plugin_t *permkfs = NULL;

	if (handle == NULL) {
		return -1;
	}

	permkfs = (erase_mkfs_plugin_t*)handle;

	permkfs->cancel_request = 1;

	return 0;
}

/*
 *  cm_worker_new for fsck plugin
 */
int cm_worker_new(cm_worker_instance_t **instance)
{
	cm_worker_instance_t *inst = NULL;
	erase_mkfs_plugin_t *plug = NULL;
	int result = -1;

	inst = (cm_worker_instance_t*)malloc(sizeof(cm_worker_instance_t));
	if (inst == NULL) {
		result = -1;
		goto err_return;
	}

	(void)memset(inst,0,sizeof(cm_worker_instance_t));

	plug = (erase_mkfs_plugin_t*)malloc(sizeof(erase_mkfs_plugin_t));
	if (plug == NULL) {
		result = -1;
		goto err_return;
	}
	(void)memset(plug,0,sizeof(erase_mkfs_plugin_t));

	inst->handle = (cm_worker_handle_t)plug;
	inst->set_args = cm_worker_set_args;
	inst->exec = cm_worker_exec;
	inst->cancel = cm_worker_cancel;

	(*instance) = inst;

	return 0;

err_return:
	if (plug != NULL) {
		(void)free(plug);
	}

	if (inst != NULL) {
		(void)free(inst);
	}

	return result;
}
/*
 *  cm_worker_delete for fsck plugin
 */
int cm_worker_delete(cm_worker_instance_t *instance)
{
	int result = -1;

	if (instance == NULL) {
		result = -1;
		goto err_return;
	}

	if (instance->handle != NULL) {
		(void)free(instance->handle);
	}

	(void)free(instance);

	return 0;
err_return:
	return result;
}