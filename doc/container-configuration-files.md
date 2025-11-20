# Container Configuration Files Guide

This document describes the per-container configuration files used by Container Manager.

## Overview

**Container Configuration Files** contain detailed configuration for individual containers, specifying root filesystem, devices, network interfaces, resource limits, and other container-specific settings.

Each container has its own JSON configuration file (e.g., `cluster-demo.json`) stored in the directory specified by the global `configdir` setting.

Container manager recommend to crate these directories per container.

For rootfs mount point:
```
/opt/container/guests/my-container/rootfs
```
For R/W partition mount point:
```
/opt/container/guests/my-container/nv
```
For temporary directory for dynamic mount:
```
/opt/container/guests/my-container/shmounts
```

Container manager supports A/B boot.  It controls by 'aglabboot' in kernel commandline.  If it sets '0', container manager select 1st device.  If it sets '1', container manager select 2nd device.

---

## Container Configuration Files

Detailed configuration for individual containers.

### Minimum Configuration Example

```json
{
	"name": "my-container",
	"role": "service",
	"base": {
		"autoboot": true,
		"bootpriority": 100,
		"rootfs": {
			"path": "/opt/container/guests/my-container/rootfs",
			"filesystem": "ext4",
			"mode": "rw",
			"blockdev": [
				"/dev/mmcblk1p2",
				"/dev/mmcblk1p2"
			]
		}
	}
}
```

### Top-level Configuration Items

#### `name` (Required)
- **Type**: String
- **Description**: Container name
- **Example**: `"cluster-demo"`,`"momi-ivi-demo"`

#### `role` (Optional)
- **Type**: String
- **Description**: Container role (user-defined)
- **Note**: When it not set, role is set container name
- **Example**: `"cluster"`, `"ivi"`, `"rse"`

#### `base` (Required)
- **Type**: Object
- **Description**: Basic container configuration

#### `fs` (Optional)
- **Type**: Object
- **Description**: Filesystem configuration

#### `device` (Optional)
- **Type**: Object
- **Description**: Device configuration

#### `network` (Optional)
- **Type**: Object
- **Description**: Network configuration

#### `resource` (Optional)
- **Type**: Array
- **Description**: Resource limit configuration

---

## Section 1: base (Basic Configuration)

```json
"base": {
	"autoboot": true,
	"bootpriority": 1,
	"rootfs": { ... },
	"extradisk": [ ... ],
	"extended": { ... },
	"lifecycle": { ... },
	"cap": { ... },
	"tty": { ... },
	"idmap": { ... },
	"environment": [ ... ]
}
```

### base Configuration Items

#### `autoboot`
- **Type**: Boolean
- **Default**: `false`
- **Description**: Whether to auto-start during boot
- **Example**: `true`, `false`

#### `bootpriority`
- **Type**: Number
- **Default**: `1000`
- **Description**: Boot priority (lower values have higher priority)
- **Example**: `1`, `100`, `1000`

#### `rootfs` (Required)
- **Type**: Object
- **Description**: Root filesystem configuration

**rootfs Details**:

Root filesystem use partition.

```json
"rootfs": {
	"path": "/opt/container/guests/my-container/rootfs",
	"filesystem": "ext4",
	"mode": "rw",
	"option": "defaults",
	"blockdev": [
		"/dev/mmcblk1p2",
		"/dev/mmcblk1p2"
	]
}
```

Or host filesystem bind mount:

```json
"rootfs": {
	"path": "/container/rootfs",
	"mode": "ro",
	"hostpath": [
		"/host/rootfs",
		null
	]
}
```
##### If you want to use block device mode.
| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `path` | String | Required | Mount destination path | `"/opt/container/guests/xxx/rootfs"` |
| `filesystem` | String | Required | Filesystem type | `"ext4"`, `"erofs"` |
| `mode` | String | Optional | Mount mode | `"ro"`, `"rw"`, default: `"ro"` |
| `option` | String | Optional | Mount options | `"defaults"`, `"noatime"`, default: not set |
| `blockdev` | Array | Required | Block devices (up to 2 elements) | `["/dev/mmcblk1p2"]` or `["/dev/mmcblk1p2", "/dev/mmcblk1p3"]` |
| `hostpath` | Array | Forbidden | Shall not set this value in case of block device mode. | |

Block device selection of `blockdev` is depend on 'aglabboot' settings.

If you do not use A/B mode, blockdev value should set 2nd value same as 1st value.



##### If you want to use host filesystem bind mode.
| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `path` | String | Required | Mount destination path | `"/opt/container/guests/xxx/rootfs"` |
| `filesystem` | String | Not set | Filesystem type | Not needed in case of host filesystem bind mode |
| `mode` | String | Optional | Mount mode | `"ro"`, `"rw"`, default: `"ro"` |
| `option` | String | Optional | Mount options | Not needed in case of host filesystem bind mode |
| `blockdev` | Array | Forbidden | Shall not set this value in case of host filesystem bind mode | |
| `hostpath` | Array | Required | Host paths (up to 2 elements, mutually exclusive with blockdev) | `["/host/path"]` |


#### `extradisk` (Optional)
- **Type**: Array
- **Description**: Array of additional disk configurations

```json
"extradisk": [
	{
		"from": "/opt/container/guests/my-container/nv",
		"to": "var/nonvolatile",
		"filesystem": "ext4",
		"mode": "rw",
		"option": "defaults",
		"redundancy": "failover",
		"blockdev": [
			"/dev/mmcblk1p4",
			null
		]
	}
]
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `from` | String | Required | Mount point for host root filesystem | `"/opt/container/guests/xxx/nv"` |
| `to` | String | Required | Mount point at guest root filesystem (Should remove '/' at top) | `"var/nonvolatile"` |
| `filesystem` | String | Required | Filesystem type | `"ext4"`, `"erofs"` |
| `mode` | String | Optional | Mount mode | `"ro"`, `"rw"`, default: `"ro"` |
| `option` | String | Optional | Mount options | `"defaults"`, `"noatime"`, default: not set |
| `redundancy` | String | Optional | Redundancy setting | `"failover"`, `"ab"`, `"fsck"`, `"mkfs"`, default: `"failover"` |
| `blockdev` | Array | Required | Block devices (up to 2 elements, first is required) | `["/dev/mmcblk1p4"]` or `["/dev/mmcblk1p4", "/dev/mmcblk1p5"]` |

If you set `failover` to `redundancy`:  
If mount operation is failed, it failover. It uses only first device on `blockdev`.

If you set `ab` to `redundancy`:  
Block device selection of `blockdev` is depend on 'aglabboot' settings.

If you set `fsck` to `redundancy`:  
If mount operation is failed, it execs fsck. It uses only first device on `blockdev`.

If you set `mkfs` to `redundancy`:  
If mount operation is failed, it execs mkfs. It uses only first device on `blockdev`.

#### `extended` (Optional)
- **Type**: Object
- **Description**: Extended configuration

```json
"extended": {
	"shmounts": "/opt/container/guests/xxx/shmounts"
}
```

**shmounts**: It sets to temporary directory for dynamic mount.  If it is set `"disable"`, the dynamic mount is disabled.

#### `lifecycle` (Optional)
- **Type**: Object
- **Description**: Container startup and shutdown signal configuration

```json
"lifecycle": {
	"halt": "SIGRTMIN+3",
	"reboot": "SIGTERM",
	"timeout": 5000
}
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `halt` | String | Optional | Set stop signal | if you use systemd with glibc, shall set `"SIGRTMIN+3"`, default: `"SIGTERM"` |
| `reboot` | String | Optional | Set reboot signal | default: `"SIGTERM"` |
| `timeout` | Number | Optional | Set timeout (milliseconds) | default: `10000` |

#### `cap` (Optional)
- **Type**: Object
- **Description**: Linux capability configuration

```json
"cap": {
	"drop": "sys_module mac_admin mac_override sys_time",
	"keep": "net_admin sys_admin"
}
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `drop` | String | Optional | Set capabilities to drop (space-separated) | `"sys_module"`,`"sys_time"`, default: not set |
| `keep` | String | Optional | Set capabilities to keep (space-separated) | `"net_admin"`,`"sys_admin"`, default: not set |

Refer to [man page for capabilities](https://man7.org/linux/man-pages/man7/capabilities.7.html).

#### `tty` (Optional)
- **Type**: Object
- **Description**: TTY/PTY configuration

```json
"tty": {
	"tty": 1,
	"pty": 1
}
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `tty` | Number | Optional | Maximum number of TTYs | `1`, `2`, ...,  default: `1` |
| `pty` | Number | Optional | Maximum number of PTYs | `1`, `2`, ...,  default: `1` |

#### `idmap` (Optional)
- **Type**: Object
- **Description**: UID/GID mapping configuration

```json
"idmap": {
	"uid": {
		"guestroot": 0,
		"hostidstart": 100000,
		"num": 65536
	},
	"gid": {
		"guestroot": 0,
		"hostidstart": 100000,
		"num": 65536
	}
}
```

##### `uid` (Optional)
- **Type**: Object
- **Description**: UID mapping

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `guestroot` | Number | Required | Root ID in guest | `0`, Typically `0` |
| `hostidstart` | Number | Required | Host starting ID | `100000`, `200000`, ... |
| `num` | Number | Required | Number of mapping IDs | `10000`, `65536`, ... |

##### `gid` (Optional)
- **Type**: Object
- **Description**: GID mapping

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `guestroot` | Number | Required | Root ID in guest | `0`, Typically `0` |
| `hostidstart` | Number | Required | Host starting ID | `100000`, `200000`, ... |
| `num` | Number | Required | Number of mapping IDs | `10000`, `65536`, ... |


#### `environment` (Optional)
- **Type**: Array
- **Description**: Environment variable configuration

```json
"environment": [
	"HOME=/home/root",
	"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin"
]
```

The array for environment variable to set in guest.


---

## Section 2: fs (Filesystem Configuration)

```json
"fs": {
	"mount": [
		{
			"type": "filesystem",
			"from": "tmpfs",
			"to": "run",
			"fstype": "tmpfs",
			"option": "defaults 0 0"
		},
		{
			"type": "directory",
			"from": "/sys/kernel/security",
			"to": "sys/kernel/security",
			"fstype": "none",
			"option": "ro,bind,optional 0 0"
		},
		{
			"type": "delayed",
			"from": "/run/user/1000",
			"to": "run/user"
		}
	]
}
```

### mount Configuration Items

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `type` | String | Required | Mount method | `"filesystem"`, `"directory"`, `"delayed"` |
| `from` | String | Required | Mount filesystem if you use `"filesystem"` type | `"tmpfs"`, Not recommend to set other filesystem type. Recommend to use extradisk. |
|  |  |  | Mount source directory if you use `"directory"`, `"delayed"` type | `"/var/nvlog"`, `"/sys/kernel/security"` |
| `to` | String | Required | Mount destination (in container, should remove '/' at top) | `"opt/log"`, `"sys/kernel/security"` |
| `fstype` | String | Required | Filesystem type if you use `"filesystem"` type | `"tmpfs"` |
|  |  | Required | Shall set `"none"` if you use `"directory"` type | `"none"` |
|  |  | No | Do not need to set if you use `"delayed"` type | (not needed for delayed) |
| `option` | String | Required | Mount options if you use `"filesystem"` type | `"defaults"`, etc... |
|  |  | Required | Mount options if you use `"directory"` type | `"ro,bind,optional"`, `"rw,bind,optional"` |
|  |  | No | Do not need to set if you use `"delayed"` type | (not needed for delayed) |

**type values**:
- `"filesystem"`: Mount a new filesystem
- `"directory"`: Bind mount a directory
- `"delayed"`: Delayed mount (If 'from' directory will be detected, it mount at that time.). If you want to use it, you shall enable `"shmounts"` in `"extended"`.

---

## Section 3: device (Device Configuration)

```json
"device": {
	"protection": "disable",
	"static": [ ... ],
	"dynamic": [ ... ]
}
```

### `protection`
- **Type**: String
- **Default**: Enabled (cgroup device protection)
- **Value**: Set to `"disable"` to disable. Set to `"enable"` to enable.

### 3.1 static (Static Devices)

```json
"static": [
	{
		"type": "devnode",
		"from": "/dev/pvr_sync",
		"to": "dev/pvr_sync",
		"devnode": "/dev/pvr_sync",
		"optional": 0,
		"wideallow": 0,
		"exclusive": 0
	},
	{
		"type": "devdir",
		"from": "/dev/dri",
		"to": "dev/dri",
		"devnode": "/dev/dri/card0",
		"optional": 0,
		"wideallow": 1,
		"exclusive": 0
	},
	{
		"type": "gpio",
		"port": 42,
		"direction": "out",
		"from": "/sys/devices/platform/soc/e6055400.gpio/gpiochip6/gpio/gpio381",
		"to": "sys/devices/platform/soc/e6055400.gpio/gpiochip6/gpio/gpio381"
	},
	{
		"type": "iio",
		"sysfrom": "/sys/bus/iio/devices/iio:device0",
		"systo": "sys/bus/iio/devices/iio:device0",
		"devfrom": "/dev/iio:device0",
		"devto": "dev/iio:device0",
		"devnode": "/dev/iio:device0",
		"optional": 0
	}
]
```

**Details by type**:

#### devnode (Device Node)
- Single device node

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `type` | String | Required | Device type | `"devnode"` |
| `from` | String | Required | Host device file name | `"/dev/pvr_sync"`, `"/dev/ttyS0"` |
| `to` | String | Required | Guest device file name. Typically same as `from`(Should remove '/' at top). | `"dev/pvr_sync"` |
| `devnode` | String | Required | Device node. Same as `from`. | `"/dev/pvr_sync"`, `"/dev/ttyS0"` |
| `optional` | Number | Optional | Optional (0 or 1). If `from` is not available, 0 is 'fail for container launch', 1 is 'fail through'. | `0` (default) |
| `wideallow` | Number | Optional | Wide allow (0 or 1). If it set 0, gest get acceptance same major:miner number device. If it set 1, gest get acceptance same major number device. | `0` (default) |
| `exclusive` | Number | Optional | Exclusive access (0 or 1). Do not use it now. | `0` (default) |

#### devdir (Device Directory)
- Entire device directory
- 
| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `type` | String | Required | Device type | `"devdir"` |
| `from` | String | Required | Host device file name | `"/dev/dri"`, `"/dev/input"` |
| `to` | String | Required | Guest device file name. Typically same as `from`(Should remove '/' at top). | `"dev/dri"`, `"dev/input"` |
| `devnode` | String | Required | Representative device node. | `"/dev/dri/card0"`, `"/dev/input/event0"` |
| `optional` | Number | Optional | Optional (0 or 1). If `from` is not available, 0 is 'fail for container launch', 1 is 'fail through'. | `0` (default) |
| `wideallow` | Number | Optional | Wide allow (0 or 1). If it set 0, gest get acceptance same major:miner number device of `devnode`. If it set 1, gest get acceptance same major number device of `devnode`. | `0` (default) |
| `exclusive` | Number | Optional | Exclusive access (0 or 1). Do not use it now. | `0` (default) |

#### gpio (GPIO)
- GPIO pin assignment

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `type` | String | Required | Device type | `"gpio"` |
| `port` | Number | Required* | GPIO port number | `381`, `404` |
| `direction` | String | Optional | GPIO port direction | `"in"` (input, default), `"out"` (output), `"low"` (output & initial low), `"high"` (output & initial high) |
| `from` | String | Required | GPIO sysfs path in host | `"/sys/devices/platform/soc/e6055400.gpio/gpiochip6/gpio/gpio381"` |
| `to` | String | Required | GPIO sysfs path in guest (Should remove '/' at top) | `"sys/devices/platform/soc/e6055400.gpio/gpiochip6/gpio/gpio381"` |

#### iio (Industrial I/O)
- Industrial I/O device

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|----------------|
| `type` | String | Required | Device type | `"iio"` |
| `sysfrom` | String | Required | IIO sysfs path in host | `"/sys/devices/platform/soc/e66d0000.i2c/i2c-3/3-006a/iio:device0/"` |
| `systo` | String | Required | IIO sysfs path in guest (Should remove '/' at top) | `"sys/devices/platform/soc/e66d0000.i2c/i2c-3/3-006a/iio:device0/"` |
| `devfrom` | String | Required | Device file path in host | `"/dev/iio:device0"` |
| `devto` | String | Required | Device file path in guest (Should remove '/' at top)  | `"dev/iio:device0"` |
| `devnode` | String | Required | Device node. Same as `from` | `"/dev/iio:device0"` |
| `optional` | Number | Optional | Optional (0 or 1). If `from` is not available, 0 is 'fail for container launch', 1 is 'fail through'. | `0` (default) |

### 3.2 dynamic (Dynamic Devices)

T.B.D.

---

## Section 4: network (Network Configuration)

```json
"network": {
	"static": [
		{
			"type": "veth",
			"param": {
				"name": "veth0",
				"link": "lxcbr0",
				"flags": "up",
				"hwaddr": "00:16:3e:xx:xx:xx",
				"mode": "bridge",
				"address": "192.168.1.100/24",
				"gateway": "192.168.1.1"
			}
		},
		{
			"type": "vxcan",
			"param": {
				"name": "can0",
				"upstream": "can0"
			}
		}
	]
}
```

### static Network Interfaces

#### veth (Virtual Ethernet)

```json
{
	"type": "veth",
	"param": {
		"name": "veth0",
		"link": "lxcbr0",
		"flags": "up",
		"hwaddr": "00:16:3e:xx:xx:xx",
		"mode": "bridge",
		"address": "192.168.1.100/24",
		"gateway": "192.168.1.1"
	}
}
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `name` | String | Required | Interface name to create a guest | `"eth0"`, `"veth0"` |
| `link` | String | Required | Name of linking target interface | `"lxcbr0"`, `"br0"` |
| `flags` | String | Optional | Initial state for interface | `"up"`, `"down"` |
| `hwaddr` | String | Optional | MAC address for veth interface | `"00:16:3e:xx:xx:xx"` xx is a random |
| `mode` | String | Optional | Mode for veth | `"bridge"`, `"router"` (Only tested to bridge) |
| `address` | String | Optional | IP address (CIDR notation) | `"192.168.1.100/24"` |
| `gateway` | String | Optional | Default gateway | `"192.168.1.1"` |

#### vxcan (Virtual CAN)

```json
{
	"type": "vxcan",
	"param": {
		"name": "can0",
		"upstream": "can0"
	}
}
```

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `name` | String | Required | Interface name at guest | |
| `upstream` | String | Required | Interface name at host to connect | |

TODO: Support CAN gateway filter.

---

## Section 5: resource (Resource Limits)

```json
"resource": [
	{
		"type": "cgroup",
		"object": "memory.max",
		"value": "512M"
	},
	{
		"type": "cgroupv2",
		"object": "memory.max",
		"value": "1G"
	},
	{
		"type": "prlimit",
		"object": "RLIMIT_NOFILE",
		"value": "4096"
	},
	{
		"type": "sysctl",
		"object": "net.ipv4.ip_forward",
		"value": "1"
	}
]
```

### resource Configuration Items

| Item | Type | Required | Description | Example Values |
|------|------|----------|-------------|-----------------|
| `type` | String | Required | Resource control type | `"cgroup"`, `"cgroupv1"`, `"cgroupv2"`, `"prlimit"`, `"sysctl"` |
| `object` | String | Required | Control target | `"memory.max"`, `"RLIMIT_NOFILE"`, `"net.ipv4.ip_forward"` |
| `value` | String | Required | Limit value | `"512M"`, `"4096"`, `"1"` |

**type values**:
- `"cgroup"` / `"cgroupv1"`: cgroup v1 limits
- `"cgroupv2"`: cgroup v2 limits
- `"prlimit"`: Process limits (getrlimit/setrlimit)
- `"sysctl"`: sysctl parameters

---

## Complete Container Configuration Example

Refer to test/agl-data/*.json

---

## Troubleshooting

### Common Errors

#### 1. "Json file error"
- JSON file format is invalid
- Verify that the file encoding is UTF-8
- Use a JSON validator to check the schema

#### 2. "mandatory value"
- Required items are missing
- Check the "Required" column in the documentation

#### 3. Device not found
- Verify the paths in `blockdev`, `from`, and `to`
- Check device existence: `ls -la /dev/`

#### 4. Mount failure
- Verify the filesystem is correctly specified
- Ensure mount options are valid

#### 5. Container not starting
- Verify the rootfs path exists and is accessible
- Check container name and role are set
- Verify boot priority is set correctly (lower = higher priority)

---

## References

- **Root Filesystem**: The basic filesystem for containers
- **Device Mapping**: Exposing host devices to containers
- **Network Isolation**: Communication isolation via veth/vxcan
- **Resource Limits**: CPU/memory limits via cgroup
- **cJSON**: JSON parser library used internally
