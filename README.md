# micro_ros_ev3

micro-ROS for LEGO Mindstorms EV3 running **ev3dev Stretch** (ARM926EJ-S, ARMv5TE, glibc 2.24).

This repository provides transport implementations and ready-to-build examples to run
micro-ROS nodes on the EV3 brick and communicate with a ROS 2 system over serial or UDP.

## System overview

```mermaid
graph LR
    subgraph PC["PC / Host"]
        ROS2["ROS 2 (Jazzy)"]
        Agent["micro-ROS agent\n(Docker)"]
        ROS2 <--> Agent
    end
    subgraph EV3["EV3 / ev3dev Stretch"]
        App["micro-ROS app\n(linked against libmicroros.a)"]
    end
    Agent <-->|"Serial\nor UDP"| App
```

## Repository structure

```
micro_ros_ev3/
├── ev3_toolchain.cmake
├── third_party/
│   └── microros/                             ← vendored libmicroros.a + micro-ROS headers (committed)
├── transport/
│   ├── time_compat.c                         ← glibc 2.24 compatibility shim (always required)
│   ├── serial_transport.c                    ← POSIX serial (termios, 115200 baud)
│   └── udp_transport.c                       ← POSIX UDP (sockets)
└── examples/
    ├── micro_ros_publisher_serial/           ← publisher over serial
    ├── micro_ros_publisher_udp/             ← publisher over UDP
    ├── micro_ros_subscriber/                ← subscriber over UDP
    ├── micro_ros_addtwoints_service/        ← AddTwoInts service server over UDP
    └── micro_ros_time_sync/                 ← time synchronisation with the agent over UDP
```

---

## Prerequisites

- PC with Docker installed (64-bit Linux)
- LEGO Mindstorms EV3 with [ev3dev Stretch](https://github.com/ev3dev/ev3dev/releases/download/ev3dev-stretch-2020-04-10/ev3dev-stretch-ev3-generic-2020-04-10.zip)
- Serial or network connection between PC and EV3

---

## Step 1 — Get the micro-ROS static library (Optional)

The static library (`libmicroros.a`) and headers are generated with a modified
[micro-ROS static library builder](https://github.com/racarla96/micro-ROS-docker/tree/feature/lego-ev3-support)
and the EV3 platform support added to
[micro_ros_arduino](https://github.com/racarla96/micro_ros_arduino/tree/feature/lego-ev3-support).

They are already vendored under [`third_party/microros/`](third_party/microros/) in this
repository, so **most users can skip straight to [Step 2](#step-2--set-up-the-cross-compilation-environment)**.
Regenerate them only if you need a different micro-ROS/ROS 2 distro or platform change.

### Option A — Pull the pre-built builder image from Docker Hub (recommended)

```bash
docker pull racarla96/micro_ros_static_library_builder:jazzy-ev3
```

Then skip to [Step 1.3](#13-generate-the-library).

### Option B — Build the image yourself

```bash
git clone -b feature/lego-ev3-support https://github.com/racarla96/micro-ROS-docker.git
cd micro-ROS-docker
git clone -b feature/lego-ev3-support https://github.com/racarla96/micro_ros_arduino.git

cd micro-ROS-static-library-builder
docker build -t racarla96/micro_ros_static_library_builder:jazzy-ev3 .
```

> First build downloads several ARM toolchains and compiles micro-ROS with colcon.
> Expect 20–40 minutes.

### 1.3 Generate the library

Run from inside `micro-ROS-docker/micro-ROS-static-library-builder/`:

```bash
docker run -it \
  -v ./micro_ros_arduino:/project \
  -e MICROROS_LIBRARY_FOLDER=extras \
  --net=host \
  racarla96/micro_ros_static_library_builder:jazzy-ev3 \
  -p ev3
```

After completion, copy the generated tree into this repo's `third_party/microros/`
so examples can build against it without any external mount:

```bash
cp -r micro_ros_arduino/src/* /path/to/micro_ros_ev3/third_party/microros/
```

```
third_party/microros/
├── ev3/
│   └── libmicroros.a     ← static library
├── rcl/
├── rclc/
├── std_msgs/
├── example_interfaces/
└── ...                   ← all micro-ROS headers
```

---

## Step 2 — Set up the cross-compilation environment

ev3dev provides a Docker image with `arm-linux-gnueabi-gcc` pre-configured for ev3dev Stretch:

```bash
docker pull ev3dev/debian-stretch-cross
docker tag ev3dev/debian-stretch-cross ev3cc
```

> Reference: https://www.ev3dev.org/docs/tutorials/using-docker-to-cross-compile/

CMake (3.7.2) is already included in the image, so no extra setup is needed before
building.

---

## Step 3 — Clone this repository

```bash
# Place it next to micro-ROS-docker/
git clone https://github.com/racarla96/micro_ros_ev3.git
cd micro_ros_ev3
```

---

## Step 4 — Build an example

Each example mounts a single volume into the container:
- `$(pwd)` → `/src` (this repo, including the vendored `third_party/microros/`)

`MICROROS_DIR` defaults to `third_party/microros` (relative to the repo root), so it
does not need to be passed unless you generated the library into a different location.

Adjust `agent_ip` in `main.c` before building UDP examples.

### Publisher — serial

Edit `examples/micro_ros_publisher_serial/main.c` and set the serial device
(see [serial devices table](#serial-devices-on-ev3dev)):

```c
(void *)"/dev/ttyS1",
```

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -w /src ev3cc bash -c \
  "mkdir -p build/publisher_serial && cd build/publisher_serial && \
   cmake ../../examples/micro_ros_publisher_serial -DCMAKE_TOOLCHAIN_FILE=../../ev3_toolchain.cmake && \
   cmake --build ."
```

### Publisher — UDP

Edit `agent_ip` in `examples/micro_ros_publisher_udp/main.c`, then:

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -w /src ev3cc bash -c \
  "mkdir -p build/publisher_udp && cd build/publisher_udp && \
   cmake ../../examples/micro_ros_publisher_udp -DCMAKE_TOOLCHAIN_FILE=../../ev3_toolchain.cmake && \
   cmake --build ."
```

### Subscriber — UDP

Subscribes to `ev3_topic` (std_msgs/Int32) and prints received values.

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -w /src ev3cc bash -c \
  "mkdir -p build/subscriber && cd build/subscriber && \
   cmake ../../examples/micro_ros_subscriber -DCMAKE_TOOLCHAIN_FILE=../../ev3_toolchain.cmake && \
   cmake --build ."
```

Test from the PC:
```bash
ros2 topic pub /ev3_topic std_msgs/msg/Int32 "{data: 42}"
```

### AddTwoInts service server — UDP

Exposes `/addtwoints` (example_interfaces/srv/AddTwoInts).

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -w /src ev3cc bash -c \
  "mkdir -p build/service && cd build/service && \
   cmake ../../examples/micro_ros_addtwoints_service -DCMAKE_TOOLCHAIN_FILE=../../ev3_toolchain.cmake && \
   cmake --build ."
```

Test from the PC:
```bash
ros2 service call /addtwoints example_interfaces/srv/AddTwoInts "{a: 3, b: 5}"
```

### Time synchronisation — UDP

Synchronises the EV3 clock with the agent and prints the current UTC time every second.

```bash
docker run --rm -it \
  -v $(pwd):/src \
  -w /src ev3cc bash -c \
  "mkdir -p build/time_sync && cd build/time_sync && \
   cmake ../../examples/micro_ros_time_sync -DCMAKE_TOOLCHAIN_FILE=../../ev3_toolchain.cmake && \
   cmake --build ."
```

---

## Step 5 — Copy the binary to the EV3

Default ev3dev Stretch credentials: **user** `robot`, **password** `maker`.

```bash
# Copy and set executable permissions in one step
scp build/publisher_udp/micro_ros_publisher_udp robot@ev3dev.local:~
ssh robot@ev3dev.local chmod +x micro_ros_publisher_udp
```

The binary can then be launched directly from the EV3 screen using the
ev3dev Stretch file manager, or from an SSH/serial terminal.

---

## Step 6 — Run the micro-ROS agent on the PC

### Serial agent

```bash
docker run -it --rm --privileged -v /dev:/dev \
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

## Step 7 — Run on the EV3

### From an SSH or serial terminal

```bash
ssh robot@ev3dev.local
./micro_ros_publisher_udp
```

### From the EV3 screen

If the binary has executable permissions (set in Step 5), it can be launched
directly from the ev3dev Stretch file manager on the EV3 screen without needing
a PC terminal open.

### Verify on the PC

```bash
ros2 topic echo /ev3_topic
```

---

## Serial devices on ev3dev

| Connection | Device on EV3 |
|---|---|
| USB cable (micro-USB ↔ USB-A) | `/dev/ttyGS0` |
| EV3 sensor port 1 (UART) | `/dev/ttyS1` |
| Bluetooth serial | `/dev/ttyS2` |

Check available devices: `ls /dev/tty*`

---

## Notes

### glibc compatibility (`transport/time_compat.c`)

The library is built on Ubuntu 24.04 (glibc 2.39), which redirects `clock_gettime`
to `__clock_gettime64` on 32-bit ARM. ev3dev Stretch (glibc 2.24) lacks that symbol.
`time_compat.c` provides the missing wrapper and **must always be included** in the build.

### Debug build

```bash
# Add -DCMAKE_C_FLAGS=-g to the cmake invocation, then on the EV3:
gdbserver :1234 ./my_node

# On the PC:
arm-linux-gnueabi-gdb build/.../my_node
(gdb) target remote ev3dev.local:1234
```
