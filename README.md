# Container Manager

## What is Container Manager?

Container Manager is a lightweight container orchestration and management system designed for automotive and embedded Linux environments. It provides comprehensive functionality for managing multiple containers, including:

- **Container Lifecycle Management**: Start, stop, reboot, and monitor containers with customizable boot priorities and auto-boot capabilities
- **Rootfs Management**: Support for both block device-based and bind-mount-based root filesystems with A/B boot capability for dual-partition systems
- **Device Management**: Dynamic and static device mapping, GPIO control, Industrial I/O (IIO) device support, and SocketCAN integration
- **Network Isolation**: Virtual Ethernet (veth) and Virtual CAN (vxcan) interface configuration with bridge connectivity
- **Resource Limits**: cgroup v1/v2 integration for CPU, memory, and I/O limiting, plus process resource limits (prlimit) and sysctl tuning
- **Advanced Filesystem Features**: Extra disk mounting with redundancy modes (failover, A/B, fsck, mkfs), temporary dynamic mounts, and read-only bind mounts
- **Capability Control**: Linux capability dropping and preservation for enhanced security
- **Plugin Architecture**: Extensible worker plugin system for custom device management (e.g., filesystem checks, mkfs operations)

Container Manager is configured entirely through JSON configuration files, making it easy to integrate into automated deployment pipelines and cloud-native workflows.

---

## Developed by Automotive Grade Linux (AGL) Community

Container Manager is developed by the **Automotive Grade Linux (AGL)** community as part of the broader AGL initiative to create a secure, open-source automotive operating system platform.

For more information about AGL, visit: [https://www.automotivelinux.org/](https://www.automotivelinux.org/)

---

## Library Dependencies

Container Manager depends on the following external libraries and development tools:

### Core Runtime Dependencies

| Library | Package Name | Purpose |
|---------|--------------|---------|
| **systemd** | `libsystemd` | System service management, logging, and event notification |
| **libudev** | `libudev` | Device management and hotplug event handling |
| **libmnl** | `libmnl` | Netlink protocol communication for network device management |
| **cJSON** | `libcjson` | JSON parsing and serialization for configuration files |
| **LXC** | `lxc` | Container runtime and lifecycle management |
| **libblkid** | `blkid` | Block device identification and filesystem detection |
| **libc** | `glibc` | Standard C library |

### Third-Party Included Libraries

- **libsocketcangw**: SocketCAN gateway library bundled under `3rdparty/libsocketcangw/` for CAN bus integration
- **wpa-supplicant**: WiFi/wireless management library bundled under `3rdparty/wpa-supplicant/` for network configuration

---

## Build and Installation

### Compilation

```bash
./autogen.sh
./configure [options]
make
sudo make install
```

### Configuration Options

| Option | Description |
|--------|-------------|
| `--enable-address-sanitizer` | Enable AddressSanitizer for memory debugging |
| `--enable-gcov` | Enable code coverage instrumentation |
| `--enable-printfdebug` | Enable user printf debug output |

---

## License

Container Manager is developed as part of the Automotive Grade Linux project. Please refer to the LICENSE file for licensing information.

---

## Support and Contributing

For bug reports, feature requests, and contributions, please refer to the AGL community resources:

- **AGL Official Website**: [https://www.automotivelinux.org/](https://www.automotivelinux.org/)
- **AGL JIRA (Issue Tracking)**: [https://jira.automotivelinux.org/](https://jira.automotivelinux.org/)
- **AGL Confluence**: [https://lf-automotivelinux.atlassian.net/wiki/spaces/](https://lf-automotivelinux.atlassian.net/wiki/spaces/)

