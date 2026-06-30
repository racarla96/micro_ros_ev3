# micro_ros_ev3

micro-ROS for LEGO Mindstorms EV3 running **ev3dev Buster** (ARM926EJ-S, ARMv5TE, glibc 2.28).

This repository provides transport implementations and examples to run micro-ROS nodes on the EV3 brick and communicate with a ROS 2 system over serial or UDP.

## System overview

```
[PC / Host]                              [EV3 / ev3dev Buster]
┌──────────────────────┐                 ┌──────────────────────┐
│  ROS 2 (Jazzy)       │                 │  micro-ROS app       │
│  micro-ROS agent     │ ←─ Serial/UDP ─→│  linked against      │
│  (Docker)            │                 │  libmicroros.a       │
└──────────────────────┘                 └──────────────────────┘
```

## Repository structure

```
micro_ros_ev3/
├── ev3_toolchain.cmake                       ← CMake toolchain for cross-compilation
├── transport/
│   ├── time_compat.c                         ← glibc 2.28 compatibility shim
│   ├── serial_transport.c                    ← POSIX serial (termios)
│   └── udp_transport.c                       ← POSIX UDP (sockets)
└── examples/
    ├── micro_ros_publisher_serial/           ← publisher over serial port
    │   ├── CMakeLists.txt
    │   └── main.c
    └── micro_ros_publisher_udp/             ← publisher over UDP
        ├── CMakeLists.txt
        └── main.c
```

---

## Prerequisites

- PC with Docker installed (64-bit Linux)
- LEGO Mindstorms EV3 with [ev3dev Buster](https://www.ev3dev.org/)
- Serial or network connection between PC and EV3

---

## Step 1 — Generate the micro-ROS static library

The static library (`libmicroros.a`) and headers are generated using a modified
version of the [micro-ROS static library builder](https://github.com/racarla96/micro-ROS-docker/tree/feature/lego-ev3-support)
together with the [micro_ros_arduino](https://github.com/racarla96/micro_ros_arduino/tree/feature/lego-ev3-support)
platform definitions.

### 1.1 Clone the repos

```bash
git clone -b feature/lego-ev3-support https://github.com/racarla96/micro-ROS-docker.git
cd micro-ROS-docker
git clone -b feature/lego-ev3-support https://github.com/racarla96/micro_ros_arduino.git
```

### 1.2 Build the Docker image

```bash
cd micro-ROS-static-library-builder
docker build -t microros/micro_ros_static_library_builder:jazzy-ev3 .
```

> This step downloads several ARM toolchains and compiles micro-ROS with colcon.
> It can take 20–40 minutes on first build.

### 1.3 Generate the library

Run the container from inside `micro-ROS-static-library-builder/`, with
`micro_ros_arduino` mounted as the project:

```bash
docker run -it \
  -v ./micro_ros_arduino:/project \
  -e MICROROS_LIBRARY_FOLDER=extras \
  --net=host \
  microros/micro_ros_static_library_builder:jazzy-ev3 \
  -p ev3
```

After completion the library and headers are in:

```
micro_ros_arduino/src/
├── ev3/
│   └── libmicroros.a     ← static library for EV3
├── rcl/
├── rclc/
├── std_msgs/
└── ...                   ← all micro-ROS headers
```

---

## Step 2 — Set up the cross-compilation environment

ev3dev provides a Docker image with the `arm-linux-gnueabi-gcc` toolchain
pre-installed and configured for ev3dev Buster:

```bash
docker pull ev3dev/debian-buster-cross
docker tag ev3dev/debian-buster-cross ev3cc
```

> Reference: https://www.ev3dev.org/docs/tutorials/using-docker-to-cross-compile/

---

## Step 3 — Build an example

Clone this repository next to `micro-ROS-docker/`:

```bash
# From the parent folder of micro-ROS-docker
git clone https://github.com/racarla96/micro_ros_ev3.git
```

### Serial example

Edit `examples/micro_ros_publisher_serial/main.c` and set the serial device:

```c
(void *)"/dev/ttyS1",   /* adjust to your EV3 device — see table below */
```

Then build from the root of this repo:

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -v $(pwd)/../micro-ROS-docker/micro_ros_arduino/src:/microros \
  -w /src \
  ev3cc \
  bash -c "cmake -B build/serial examples/micro_ros_publisher_serial \
    -DCMAKE_TOOLCHAIN_FILE=ev3_toolchain.cmake \
    -DMICROROS_DIR=/microros \
    && cmake --build build/serial"
```

### UDP example

Edit `examples/micro_ros_publisher_udp/main.c` and set the agent IP:

```c
static UDPTransportArgs udp_args = { .agent_ip = "192.168.1.100", .agent_port = 8888 };
```

Then build:

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -v $(pwd)/../micro-ROS-docker/micro_ros_arduino/src:/microros \
  -w /src \
  ev3cc \
  bash -c "cmake -B build/udp examples/micro_ros_publisher_udp \
    -DCMAKE_TOOLCHAIN_FILE=ev3_toolchain.cmake \
    -DMICROROS_DIR=/microros \
    && cmake --build build/udp"
```

---

## Step 4 — Copy the binary to the EV3

```bash
scp build/serial/micro_ros_publisher_serial robot@ev3dev.local:~
# or
scp build/udp/micro_ros_publisher_udp robot@ev3dev.local:~
```

---

## Step 5 — Run the micro-ROS agent on the PC

### Serial agent

```bash
docker run -it --rm \
  --privileged -v /dev:/dev \
  microros/micro-ros-agent:jazzy \
  serial --dev /dev/ttyUSB0 -b 115200
```

### UDP agent

```bash
docker run -it --rm --net=host \
  microros/micro-ros-agent:jazzy \
  udp4 --port 8888
```

---

## Step 6 — Run on the EV3

```bash
# SSH into the EV3
ssh robot@ev3dev.local

# Run the node
./micro_ros_publisher_serial
# or
./micro_ros_publisher_udp
```

On the PC you should see the agent printing received messages.
Verify the topic from a separate ROS 2 terminal:

```bash
ros2 topic echo /ev3_topic
```

---

## Serial devices on ev3dev

| Connection | Device on EV3 |
|---|---|
| USB cable (micro-USB to USB-A) | `/dev/ttyGS0` |
| EV3 sensor port 1 (UART) | `/dev/ttyS1` |
| Bluetooth serial | `/dev/ttyS2` |

Check available devices on the EV3 with `ls /dev/tty*`.

---

## Notes

### glibc compatibility shim (`transport/time_compat.c`)

The library is built on Ubuntu 24.04 (glibc 2.39), which routes `clock_gettime`
to `__clock_gettime64` on 32-bit ARM targets. ev3dev Buster has glibc 2.28 which
lacks this symbol. `time_compat.c` provides a compatibility wrapper and must
always be included in the build.

### Debug build

Add `-DCMAKE_C_FLAGS=-g` to the cmake invocation and use `gdbserver` on the EV3
for remote debugging without exhausting its memory:

```bash
# On the EV3
gdbserver :1234 ./micro_ros_publisher_serial

# On the PC
arm-linux-gnueabi-gdb build/serial/micro_ros_publisher_serial
(gdb) target remote ev3dev.local:1234
```

### Installing additional libraries

If your application needs extra libraries, install them inside the `ev3cc`
container with the `:armel` suffix:

```bash
docker run --rm -it ev3cc bash
sudo apt-get install libsomething-dev:armel
```
