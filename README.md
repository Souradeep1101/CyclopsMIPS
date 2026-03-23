# CyclopsMIPS

CyclopsMIPS is a cross-platform MIPS IDE and simulator designed for educational purposes. It provides a visual and interactive way for students to understand the inner workings of a MIPS processor, including single-cycle, multi-cycle, and pipelined architectures.

## Features

- **Integrated Assembler**: Write MIPS assembly code and instantly compile it into machine code.
- **Cycle-Accurate Simulation**: Simulate the execution of MIPS instructions cycle by cycle.
- **5-Stage Pipeline Visualization**: Real-time monitoring of the IF, ID, EX, MEM, and WB stages.
- **Hazard Handling**: Visual feedback on data hazards, load-use stalls, and branch flushes.
- **Register & Memory Inspection**: Live view and modification of the MIPS register file and data memory.
- **Customizable Layout**: Flexible docking interface powered by Dear ImGui.
- **Advanced Text Editor**: Syntax highlighting and execution line markers.

## Architecture

The project is divided into several core components:

- **Core Engine**: A high-performance MIPS emulator implementing a 5-stage pipeline with forwarding and hazard detection units.
- **Assembler**: A custom two-pass assembler that translates MIPS assembly into executable binary.
- **UI System**: A modular desktop application built using C++, OpenGL3, and Dear ImGui.

## Development Status

CyclopsMIPS is currently in active development. The core execution engine and basic UI layout are functional. Upcoming features include:

- **Interactive CPU Architecture Diagram**: A detailed, real-time simulation of the datapath following Paterson and Hennessy conventions.
- **Signal Highlighting**: Bi-directional highlighting between the control path signals and the architecture diagram.
- **Pipeline Execution Table**: A comprehensive history of instruction flow through the pipeline.

## Prerequisites

To build CyclopsMIPS, you will need:

- C++23 compliant compiler
- CMake (version 3.21 or higher)
- GLFW 3.4
- OpenGL 3.3+

## Building the Project

1. Clone the repository:
   ```bash
   git clone https://github.com/user/CyclopsMIPS.git
   cd CyclopsMIPS
   ```

2. Initialize submodules:
   ```bash
   git submodule update --init --recursive
   ```

3. Build using CMake:
   ```bash
   cmake -B build
   cmake --build build --config Release
   ```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
