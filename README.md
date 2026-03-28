# CyclopsMIPS IDE (Version 1.0)

CyclopsMIPS is a professional, VS Code-inspired MIPS Processor Simulator and Educational IDE. It provides a highly interactive environment for university students to learn Computer Organization through real-time visualization of a 5-stage MIPS pipeline.

## Key Features

- **Unified Docking Layout**: A programmatic workspace featuring a Code Editor, Architecture Diagram, Pipeline Datapath, and Hardware State panels.
- **Interactive Schematic**: A zoomable, Manhattan-routed representation of the MIPS Pipeline with transient highlighting (3s) for active signals and nodes.
- **Multitasking Terminal**: A dynamic tabbed system for:
    - **Emulation Log**: Real-time diagnostics and hardware events.
    - **MIPS Console**: Syscall I/O handling (Print String, Read Integer, etc.).
    - **Spawnable CLI**: Support for multiple dynamic shell instances.
- **Advanced Debugging**:
    - **Cycle-Accurate Stepping**: Observe hardware state transitions at the clock-cycle level.
    - **Breakpoints**: Set breakpoints in the editor; the CPU will halt fetch precisely before the target instruction.
    - **Transient Highlighting**: Selected wires and nodes in the diagram or signals list remain highlighted for 3 seconds before fading, preventing UI clutter.
- **Native Project Management**: Use standard Windows 'Open' and 'Save' dialogs to manage `.s` assembly files.

## Technical Architecture

### Core Emulation
- **5-Stage Pipeline**: IF, ID, EX, MEM, WB with hazard detection and forwarding.
- **Syscall Handler**: Supports Syscall 1 (Print Int), 4 (Print String), 5 (Read Int - Blocking), and 10 (Exit).
- **Thread Safety**: The CPU runs on a dedicated background thread, communicating with the UI via atomic flags and a thread-safe `Logger`.

### UI System (Dear ImGui)
- **Docking**: Utilizes `ImGui::DockBuilder` for a persistent, professional layout.
- **Theming**: Custom "Engineering Precision" theme with high-contrast semantics for hardware states.
- **Cross-Panel Sync**: Selecting a signal in the list highlights the wire in the diagram, and vice-versa.

## Build Requirements
- C++20 Compatible Compiler
- CMake 3.20+
- OpenGL 3.3+
- Windows OS (for native file dialogs)

## Getting Started
1. **Open**: Launch the `CyclopsApp.exe`.
2. **Write**: Input MIPS assembly in the **Code Editor**.
3. **Run**: Go to `Run > Compile & Load` (F5).
4. **Step**: Use `Run > Step Cycle` (F10) to observe the pipeline flow.
5. **Debug**: Click a line number or use `Run > Toggle Breakpoint` (F9) to set breaks.

---
*Created by Antigravity - Advanced Agentic Coding Team*
