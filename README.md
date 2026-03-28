# CyclopsMIPS (Version 1.0)
### The Educational MIPS Processor Simulator & IDE

![CyclopsMIPS IDE Screenshot](placeholder_for_screenshot)

**CyclopsMIPS** is a professional-grade Integrated Development Environment (IDE) and cycle-accurate simulator for the MIPS32 instruction set. Designed specifically for Computer Architecture students, it transforms the abstract concepts of instruction pipelining, hazard detection, and memory hierarchy into a vivid, interactive visual experience.

---

## 🎯 Educational Goals
*   **Visualize the Pipeline**: Observe instructions moving through IF, ID, EX, MEM, and WB stages in real-time.
*   **Debug Hazards**: Witness the Hazard Detection Unit (HDU) inject bubbles to resolve data dependencies and flushes for branch mispredictions.
*   **Master the Datapath**: Explore a programmatic, vector-based architectural diagram that highlights active signals and data values.
*   **Bridge Theory & Practice**: Transition from high-level assembly in the **Code Editor** to low-level machine state in the **Inspector**.

---

## 🚀 Key Features
*   **VS Code Inspired UX**: A professional 3-column docking architecture featuring a dedicated Execution Toolbar for cycle-by-cycle analysis.
*   **Dynamic Terminal System**: Support for multiple CLI instances and a "MIPS Console" for real-time Syscall I/O (Printing strings and reading user integers).
*   **Persistent Debugging**: Set breakpoints directly in the editor and watch the simulator halt *exactly* before the instruction is fetched.
*   **Schematic Interactivity**: Click on any wire or signal in the Diagram or Signals Table to highlight the data path with 3-second transient persistence.

---

## 🛠️ Build Instructions

### Prerequisites
*   **C++23** compatible compiler (MSVC 17.x+, GCC 13+, Clang 16+).
*   **CMake** (3.20 or higher).
*   **Windows 10/11** (for Version 1 native file dialogs).

### Compilation
```powershell
# Clone the repository
git clone https://github.com/Souradeep1101/CyclopsMIPS.git
cd CyclopsMIPS

# Configure and Build
cmake -B out/build
cmake --build out/build --config Release
```

The executable will be located in `out/build/bin/Release/CyclopsApp.exe`.

---

## 🏗️ Architectural Overview

### The Backend (CyclopsCore)
The simulation is driven by a high-performance, single-threaded clock engine.
*   **MemoryBus**: Coordinates zero-latency access to Physical RAM and MMIO.
*   **L1 Caches**: Educational Instruction and Data caches with configurable miss penalties.
*   **CPU**: A 5-stage pipeline implementation with Branch Prediction (BTB) and Forwarding logic.

### The Frontend (CyclopsApp)
Built using **Dear ImGui** and **OpenGL 3.3**, the frontend provides a high-DPI responsive interface.
*   **Docking Topology**: Programmatic workspace management via `ImGui::DockBuilder`.
*   **Threaded Sync**: Emulation runs on a dedicated background thread, communicating with the UI via thread-safe queues and `std::atomic` flags.

---

## 📸 Future Screenshots
*   `![Main_IDE_Interface]`
*   `![Pipeline_Signals_Table]`
*   `![MIPS_Architecture_Schematic]`

---

## ⚖️ License
Distributed under the MIT License. See `LICENSE` for more information.

**Developed with ❤️ for the next generation of Computer Architects.**
