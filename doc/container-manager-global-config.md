# Container Manager Global Configuration Guide

This document describes the global configuration file (`container-manager.json`) used by Container Manager.

## Overview

The **Container Manager Configuration File** (`container-manager.json`) contains global settings that apply to the entire Container Manager instance, including network bridges and system-wide mount operations.

---

## Container Manager Configuration File

Global configuration for the entire Container Manager.

### Minimum Configuration Example

```json
{
	"configdir": "/opt/container/conf/",
	"etherbridge": [
		{
			"name": "lxcbr0"
		}
	],
	"operation": {
		"mount": []
	}
}
```

### Configuration Items

#### `configdir` (Required)
- **Type**: String
- **Description**: Directory containing container configuration files
- **Example**: `"/opt/container/conf/"`

#### `etherbridge` (Optional)
- **Type**: Array
- **Description**: Array of network bridge configurations
- **Elements**:
  - `name` (Required): Bridge name (string)
  
- **Example**:
```json
"etherbridge": [
	{
		"name": "lxcbr0"
	},
	{
		"name": "lxcbr1"
	}
]
```

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

#### 3. Bridge not found
- Verify that the bridge name exists on the system
- Check available bridges: `ip link show type bridge`

#### 4. Mount failure
- Verify the filesystem is correctly specified
- Ensure mount options are valid
- Check device existence: `ls -la /dev/`

---

## References

- **Network Bridges**: Virtual network interfaces for container connectivity
- **Mount Operations**: System-wide filesystem mounting with redundancy options
- **cJSON**: JSON parser library used internally
