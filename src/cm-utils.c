/**
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file	cm-utils.c
 * @brief	container manager utility functions
 */
#include "cm-utils.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

int pidfd_open_syscall_wrapper(pid_t pid)
{
	return syscall(SYS_pidfd_open, pid, 0);
}
int pidfd_send_signal_syscall_wrapper(int pidfd, int sig, siginfo_t *info, unsigned int flags)
{
	return syscall(SYS_pidfd_send_signal, pidfd, sig, info, flags);
}
/**
 * INTR safe write util.
 * This function support only to less than 4KByte (atomic op limit size) operation.
 *
 * @param [in]	fd		File descriptor.
 * @param [in]	data	Pointer to write data buffer.
 * @param [in]	size	Write data size.
 * @return int
 * @retval  0 Success.
 * @retval -1 Write error.
 */
int intr_safe_write(int fd, const void* data, size_t size)
{
	ssize_t ret = -1;

	do {
		ret = write(fd, data, size);
	} while ((ret == -1) && (errno == EINTR));

	return (int)ret;
}
/**
 * Once write util.
 * This function do three operation by one call. 'open, write and close'
 * This function support only to less than 4KByte operation.
 *
 * @param [in]	path	File path
 * @param [in]	data	Pointer to write data buffer.
 * @param [in]	size	Write data size.
 * @return int
 * @retval  0 Success.
 * @retval -1 Write error.
 */
int once_write(const char *path, const void* data, size_t size)
{
	int fd = -1;
	ssize_t ret = -1;

	fd = open(path, (O_WRONLY | O_CLOEXEC | O_TRUNC));
	if (fd < 0) {
		return -1;
	}

	do {
		ret = write(fd, data, size);
	} while ((ret == -1) && (errno == EINTR));

	(void) close(fd);

	return 0;
}
/**
 * Once read util.
 * This function do three operation by one call. 'open, read and close'
 * This function support only to less than 4KByte operation.
 *
 * @param [in]	path	File path
 * @param [in]	data	Pointer to read data buffer.
 * @param [in]	size	Read data buffer size
 * @return int
 * @retval  0 Success.
 * @retval -1 read error.
 */
int once_read(const char *path, void* data, size_t size)
{
	int fd = -1;
	ssize_t ret = -1;

	fd = open(path, (O_RDONLY | O_CLOEXEC));
	if (fd < 0) {
		return -1;
	}

	do {
		ret = read(fd, data, size);
	} while ((ret == -1) && (errno == EINTR));

	(void) close(fd);

	return 0;
}
/**
 * File node check.
 * Test to path is existing or not.
 * This function is not checked that is directory or file. Only to check existing or not.
 *
 * @param [in]	path	File path
 * @return int
 * @retval  0 Find node.
 * @retval -1 Not find node.
 */
int node_check(const char *path)
{
	int ret = -1;
	struct stat sb = {0};

	ret = stat(path, &sb);
	if (ret < 0) {
		return -1;
	}

	return 0;
}
/**
 * Recursive make dir.
 * This function create directory '/a/b/c/..'.  Similar to  mkdir -p command.
 * When directory '/a' was exist in case of dir equal '/a/b/c', this function is not return exist error.
 *
 * @param [in]	dir		Path of creating directory.
 * @param [in]	mode	Access permission of creating directory. Must be set X flag.
 * @return int
 * @retval  0 Success to create directory.
 * @retval -1 Fail to create directory.
 */
int mkdir_p(const char *dir, mode_t mode)
{
	int ret = -1, result = 0;
	size_t len = 0;
	char path[PATH_MAX];

	(void) memset(path, 0, sizeof(path));

	len = strnlen(dir, PATH_MAX-1);

	for(size_t i=1; i < len; i++) {
		if (dir[i] == '/') {
			(void) strncpy(path, dir, i);
			ret = mkdir(path, mode);
			if((ret < 0) && (errno != EEXIST)) {
				result = -1;
				break;
			}
		}
	}

	return result;
}
/**
 * Wait to exit child process by pid.
 *
 * @param [in]	pid	A pid of waiting child process.
 * @return int
 * @retval  0 A child process was normal exit without error.
 * @retval -1 A child process was abnormal exit such as SIGSEGV, SIGABRT etc.
 * @retval -2 A child process was normal exit with error.
 */
int wait_child_pid(pid_t pid)
{
	pid_t ret = -1;
	int status = 0;

	do {
		ret = waitpid(pid, &status, 0);

	} while ((ret < 0) && (errno == EINTR));

	if (!WIFEXITED(status)) {
		return -1;
	}

	if (WEXITSTATUS(status) != 0) {
		return -2;
	}

	return 0;
}
/**
 * Get monotonic time counter value by ms resolutions.
 *
 * @return int64_t
 * @retval  >0 current time.
 * @retval -1 Critical error.
 */
int64_t get_current_time_ms(void)
{
	int64_t ms = -1;
	struct timespec t = {0,0};
	int ret = -1;

	ret = clock_gettime(CLOCK_MONOTONIC, &t);
	if (ret == 0) {
		ms = ((int64_t)t.tv_sec * 1000) + ((int64_t)t.tv_nsec / 1000 / 1000);
	}

	return ms;
}
/**
 * Short time sleep.
 *
 * @param [in]	wait_time	sleep time(ms).
 * @return void
 */
void sleep_ms_time(int64_t wait_time)
{
	int ret = -1;
	struct timespec req, rem;

	if (wait_time < 0) {
		return;
	}

	req.tv_sec = wait_time / 1000;	//ms to sec
	req.tv_nsec = (wait_time % 1000) * 1000 * 1000;	//ms to nsec
	rem.tv_sec = 0;
	rem.tv_nsec = 0;

	for(int i=0;i < 10; i++) {	// INTR recover is 10 times.  To avoid no return.
		ret = nanosleep(&req, &rem);
		if ((ret < 0) && (errno == EINTR)) {
			req.tv_sec = rem.tv_sec;
			req.tv_nsec = rem.tv_nsec;
			rem.tv_sec = 0;
			rem.tv_nsec = 0;
			continue;
		}
		break;
	}

	return;
}

/**
 * Disk mount procedure for failover.
 *
 * @param [in]	devs	Array of disk block device. A and B.
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @param [in]	option	Filesystem specific option.
 * @return int
 * @retval  1 Success - secondary.
 * @retval  0 Success - primary.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Arg. error.
 */
int mount_disk_failover(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option)
{
	int ret = -1;
	int mntdisk = -1;
	const char * dev = NULL;

	for (int i=0; i < 2; i++) {
		dev = devs[i];

		ret = mount(dev, path, fstype, mntflag, option);
		if (ret < 0) {
			if (errno == EBUSY) {
				// already mounted
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"mount_disk_failover: %s is already mounted.\n", path);
				#endif
				ret = umount2(path, MNT_DETACH);
				if (ret < 0) {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"mount_disk_failover: %s unmount fail.\n", path);
					#endif
					continue;
				}

				ret = mount(dev, path, fstype, mntflag, option);
				if (ret < 0) {
					#ifdef _PRINTF_DEBUG_
					(void) fprintf(stdout,"mount_disk_failover: %s re-mount fail.\n", path);
					#endif
					continue;
				}
				mntdisk = i;
				break;
			} else {
				//error - try to mount secondary disk
				;
			}
		} else {
			// success to mount
			mntdisk = i;
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"mount_disk_failover: mounted %s to %s.\n", dev, path);
			#endif
			break;
		}
	}

	return mntdisk;
}
/**
 * Disk mount procedure for a/b.
 *
 * @param [in]	devs	Array of disk block device. A and B.
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @param [in]	option	Filesystem specific option.
 * @param [in]	side	Mount side a(=0) or b(=1).
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Arg. error.
 */
int mount_disk_ab(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option, int side)
{
	int ret = 1;
	const char * dev = NULL;

	if (side < 0 || side >= 2) {
		//side is 0 or 1 only
		return -3;
	}

	dev = devs[side];

	ret = mount(dev, path, fstype, mntflag, option);
	if (ret < 0) {
		if (errno == EBUSY) {
			// already mounted
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"mount_disk_ab: %s is already mounted.\n", path);
			#endif
			ret = umount2(path, MNT_DETACH);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"mount_disk_ab: %s unmount fail.\n", path);
				#endif
				return -1;
			}

			ret = mount(dev, path, fstype, mntflag, option);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"mount_disk_ab: %s re-mount fail.\n", path);
				#endif
				return -1;
			}
		} else {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"mount_disk_ab: %s mount fail to %s (%d).\n", dev, path, errno);
			#endif
			return -1;
		}
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"mount_disk_ab(%d): %s mount to %s (%s)\n", side, dev, path, fstype);
	#endif

	return 0;
}
/**
 * Disk mount procedure for once.
 *
 * @param [in]	devs	Array of disk block device. only to use primary side = devs[0].
 * @param [in]	path	Mount path.
 * @param [in]	fstype	Name of file system. When fstype == NULL, file system is auto.
 * @param [in]	mntflag	Mount flag.
 * @param [in]	option	Filesystem specific option.
 * @return int
 * @retval  0 Success.
 * @retval -1 mount error.
 * @retval -2 Syscall error.
 * @retval -3 Arg. error.
 */
int mount_disk_once(char **devs, const char *path, const char *fstype, unsigned long mntflag, char* option)
{
	int ret = 1;
	const char * dev = NULL;

	// Only to use primary side.
	dev = devs[0];

	ret = mount(dev, path, fstype, mntflag, option);
	if (ret < 0) {
		if (errno == EBUSY) {
			// already mounted
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"mount_disk_once: %s is already mounted.\n", path);
			#endif
			ret = umount2(path, MNT_DETACH);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"mount_disk_once: %s unmount fail.\n", path);
				#endif
				return -1;
			}

			ret = mount(dev, path, fstype, mntflag, option);
			if (ret < 0) {
				#ifdef _PRINTF_DEBUG_
				(void) fprintf(stdout,"mount_disk_once: %s re-mount fail.\n", path);
				#endif
				return -1;
			}
		} else {
			#ifdef _PRINTF_DEBUG_
			(void) fprintf(stdout,"mount_disk_once: %s mount fail to %s (%d).\n", dev, path, errno);
			#endif
			return -1;
		}
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"mount_disk_once: %s mount to %s (%s)\n", dev, path, fstype);
	#endif

	return 0;
}
/**
 * This function exec unmount operation.
 *
 * @param [in]	path		Unmount path.
 * @param [in]	timeout_at	The timeout (ms) -  relative.  When timeout is less than 1, it will not do internal retry.
 * @param [in]	retry_max	Max etry count to avoid no return.
 * @return int
 * @retval  0 Success.
 * @retval -1 unmount error.(Reserve)
 * @retval -2 Syscall error.(Reserve)
 */
int unmount_disk(const char *path, int64_t timeout_at, int retry_max)
{
	int ret = -1;
	int umount_complete = 0;
	int retry_count = 0;

	// unmount rootfs
	umount_complete = 0;

	for (retry_count = 0; retry_count < retry_max; retry_count++) {
		ret = umount(path);
		if (ret < 0) {
			if (errno == EBUSY) {
				// need to retry.
				if (timeout_at < get_current_time_ms()) {
					// retry timeout
					umount_complete = 0;
					break;
				}
				sleep_ms_time(50);	//wait
				continue;
			} else {
				// not mounted at mount point
				umount_complete = 1;
				break;
			}
		}
		// Success to unmount
		umount_complete = 1;
		break;
	}

	#ifdef _PRINTF_DEBUG_
	(void) fprintf(stdout,"container_cleanup_unmountdisk: retry = %d for unmount at %s.\n", retry_count, path);
	#endif

	if (umount_complete == 0) {
		// In case of unmount time out -> lazy unmount
		(void) umount2(path, MNT_DETACH);
		#ifdef _PRINTF_DEBUG_
		(void) fprintf(stdout,"container_cleanup_unmountdisk: lazy unmount at %s.\n", path);
		#endif
	}

	return 0;
}