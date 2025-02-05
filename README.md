# HAL DS301 CANopen Interface Layer

This is a driver for LinuxCNC. With this driver, you can modify the `.hal` file and send commands via the CAN socket.

## Installation
To install the driver, run the following command:
```sh
halcompile --install ds301.c
```

## Usage
In the `.hal` file, you have to load the driver:
```sh
loadrt ds301
addf ds301.write_all          servo-thread
```

After installing the driver, you can configure CAN commands within the `.hal` file. The CAN commands follow this format:

### Example CAN Command via Terminal
```sh
cansend can0 601#23001a0108010060
```

### Equivalent Command in HAL File
```sh
setp ds301.can_command_x_id 0x610
setp ds301.can_command_x_data_high 0x23001a01
setp ds301.can_command_x_data_low  0x08010060
```
> **Note**: Replace `x` with the command number (0-49). For example, if the command number is `0`, the setup will be:
```sh
setp ds301.can_command_0_id 0x610
setp ds301.can_command_0_data_high 0x23001a01
setp ds301.can_command_0_data_low  0x08010060
```

### Sending Multiple Commands
You can send multiple CAN commands (up to 50) by specifying the number of commands:
```sh
setp ds301.command_count 1
```
To send the configured messages in order, enable sending:
```sh
setp ds301.send 1
```

### Message Length Configuration
By default, the message length is **8 Bytes**, but you can specify a shorter length using `can_command_x_dlc`. Example of a **2-Byte message**:
```sh
setp ds301.can_command_x_dlc 2
setp ds301.can_command_x_id 0x000
setp ds301.can_command_x_data_low 0x8010
setp ds301.can_command_x_data_high 0x00000000
```

## Examples
For more examples, check the `example` folder in the repository.

---

**License & Contributions**
Feel free to contribute to the project! If you encounter any issues, submit a pull request or open an issue in the repository.

