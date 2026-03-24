// ---------------------------------------------------------------------------
// UI_ArchitectureWidget.cpp  —  Programmatic Vector MIPS Datapath
// ---------------------------------------------------------------------------
// Draws the Patterson & Hennessy Figure 4.66 MIPS pipelined datapath using
// ImDrawList geometric primitives.  Every component and wire is defined with
// normalised [0,1] coordinates, producing a resolution-independent schematic
// that scales with the panel.  Supports hover tooltips and two-way click
// selection with the Pipeline Signals table via g_schematicSelection.
// ---------------------------------------------------------------------------
#include "UI_Widgets.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <functional>

namespace MIPS::UI {

// ---- Global selection state (extern declared in UI_Widgets.h) -------------
SchematicSelection g_schematicSelection;

// ---- Design Palette -------------------------------------------------------
namespace Pal {
    constexpr ImU32 NodeFill       = IM_COL32(30,  32,  40, 255);
    constexpr ImU32 NodeBorder     = IM_COL32(90,  95, 110, 255);
    constexpr ImU32 NodeHover      = IM_COL32(68, 216, 241, 255);
    constexpr ImU32 NodeSelected   = IM_COL32(120, 220, 119, 255);
    constexpr ImU32 LatchFill      = IM_COL32(22,  24,  30, 255);
    constexpr ImU32 LatchBorder    = IM_COL32(60,  65,  80, 255);
    constexpr ImU32 LabelColor     = IM_COL32(210, 212, 220, 255);
    constexpr ImU32 DataColor      = IM_COL32(255, 215, 153, 255);
    constexpr ImU32 WireInactive   = IM_COL32(55,  58,  70, 200);
    constexpr ImU32 WireActive     = IM_COL32(68, 216, 241, 220);
    constexpr ImU32 WireSelected   = IM_COL32(120, 220, 119, 255);
    constexpr ImU32 WireFlush      = IM_COL32(255,  80,  80, 220);
    constexpr ImU32 ControlActive  = IM_COL32(68, 216, 241, 200);
    constexpr ImU32 Background     = IM_COL32(18,  19,  22, 255);
}

// ---- Data Structures ------------------------------------------------------

enum class Shape { Rect, Ellipse, Trapezoid, Latch };

struct SchematicNode {
    const char* id;
    const char* label;
    float x, y, w, h;      // normalised bounding box
    Shape shape;
    std::function<std::string(const CPU&)> dataFmt; // optional data inside node
};

struct SchematicWire {
    const char* id;
    const char* signalName;      // human-readable signal name for table sync
    std::vector<ImVec2> points;  // normalised waypoints (orthogonal routing)
    std::function<bool(const CPU&)> isActive;
    std::function<std::string(const CPU&)> tooltip;
};

// ---- Node Definitions -----------------------------------------------------

static const std::array<SchematicNode, 22> kNodes = {{
    // --- Far left ---
    { "ExcMux",   "Mux",     0.020f, 0.380f, 0.022f, 0.090f, Shape::Trapezoid, nullptr },
    { "PC",       "PC",      0.055f, 0.400f, 0.035f, 0.080f, Shape::Rect,
      [](const CPU& c) { char b[20]; snprintf(b,sizeof(b),"0x%04X",c.getState().pc); return std::string(b); } },
    { "InstrMem", "Instruction\nMemory", 0.100f, 0.300f, 0.070f, 0.260f, Shape::Rect, nullptr },
    { "AddPC4",   "+4",      0.105f, 0.150f, 0.035f, 0.055f, Shape::Rect, nullptr },

    // --- IF/ID ---
    { "IF_ID",    "IF/ID",   0.190f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- Decode stage ---
    { "HDU",      "Hazard\nDetection",  0.215f, 0.035f, 0.080f, 0.055f, Shape::Rect, nullptr },
    { "Control",  "Control", 0.270f, 0.115f, 0.060f, 0.055f, Shape::Ellipse, nullptr },
    { "ShiftL2",  "Shift\nLeft 2",     0.325f, 0.175f, 0.045f, 0.045f, Shape::Rect, nullptr },
    { "Regs",     "Registers",         0.305f, 0.270f, 0.085f, 0.200f, Shape::Rect,
      [](const CPU& c) { if(!c.id_ex.valid) return std::string();
                          char b[40]; snprintf(b,sizeof(b),"Rs:%d Rt:%d",c.id_ex.rs,c.id_ex.rt); return std::string(b); } },
    { "SignExt",  "Sign-\nextend",     0.315f, 0.560f, 0.060f, 0.050f, Shape::Ellipse, nullptr },
    { "AddBr",    "Add",     0.400f, 0.150f, 0.035f, 0.055f, Shape::Rect, nullptr },

    // --- ID/EX ---
    { "ID_EX",    "ID/EX",   0.445f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- Execute stage ---
    { "CauseReg", "Cause",   0.530f, 0.140f, 0.040f, 0.040f, Shape::Rect, nullptr },
    { "EPCReg",   "EPC",     0.530f, 0.195f, 0.040f, 0.040f, Shape::Rect, nullptr },
    { "FwdMuxA",  "Mux",     0.510f, 0.310f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "FwdMuxB",  "Mux",     0.510f, 0.470f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "ALUSrcMux","Mux",     0.565f, 0.420f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "ALU",      "ALU",     0.620f, 0.310f, 0.055f, 0.190f, Shape::Trapezoid,
      [](const CPU& c) { if(!c.ex_mem.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"0x%X",c.ex_mem.aluResult); return std::string(b); } },

    // --- EX/MEM ---
    { "EX_MEM",   "EX/MEM",  0.725f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- Memory stage ---
    { "DataMem",  "Data\nMemory", 0.780f, 0.280f, 0.075f, 0.230f, Shape::Rect,
      [](const CPU& c) { if(!c.mem_wb.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"R:0x%X",c.mem_wb.readData); return std::string(b); } },

    // --- MEM/WB ---
    { "MEM_WB",   "MEM/WB",  0.885f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- Writeback ---
    { "WBMux",    "Mux",     0.920f, 0.330f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
}};

// A separate node list for bottom control units (drawn later, on top)
static const SchematicNode kFwdUnit =
    { "FwdUnit", "Forwarding\nUnit", 0.620f, 0.800f, 0.085f, 0.060f, Shape::Rect, nullptr };

// ---- Wire Definitions -----------------------------------------------------
// Each wire is a series of normalised waypoints forming orthogonal segments.

static const std::vector<SchematicWire> kWires = {
    // --- Datapath wires ---
    { "PC_out", "PC", {{ {0.090f,0.440f}, {0.100f,0.440f} }},
      [](const CPU& c){ return true; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"PC=0x%04X",c.getState().pc); return std::string(b); } },

    { "InstrMem_out", "Instruction", {{ {0.170f,0.430f}, {0.190f,0.430f} }},
      [](const CPU& c){ return c.if_id.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%08X",c.if_id.instr.data); return std::string(b); } },

    { "PC4_out", "PC+4", {{ {0.140f,0.178f}, {0.190f,0.178f} }},
      [](const CPU& c){ return true; }, nullptr },

    // --- Control signals ---
    { "RegDst_wire", "RegDst", {{ {0.300f,0.130f}, {0.445f,0.130f}, {0.445f,0.095f}, {0.490f,0.095f} }},
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.regDst; },
      [](const CPU& c){ return c.id_ex.regDst ? std::string("RegDst=1") : std::string("RegDst=0"); } },

    { "ALUSrc_wire", "ALUSrc", {{ {0.300f,0.140f}, {0.420f,0.140f}, {0.420f,0.460f}, {0.565f,0.460f} }},
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.aluSrc; },
      [](const CPU& c){ return c.id_ex.aluSrc ? std::string("ALUSrc=1 (Imm)") : std::string("ALUSrc=0 (Reg)"); } },

    { "MemWrite_wire", "MemWrite", {{ {0.445f,0.110f}, {0.725f,0.110f}, {0.725f,0.370f}, {0.780f,0.370f} }},
      [](const CPU& c){ return c.ex_mem.valid && c.ex_mem.memWrite; },
      [](const CPU& c){ return c.ex_mem.memWrite ? std::string("MemWrite=1") : std::string("MemWrite=0"); } },

    { "MemRead_wire", "MemRead", {{ {0.445f,0.100f}, {0.735f,0.100f}, {0.735f,0.350f}, {0.780f,0.350f} }},
      [](const CPU& c){ return c.ex_mem.valid && c.ex_mem.memRead; },
      [](const CPU& c){ return c.ex_mem.memRead ? std::string("MemRead=1") : std::string("MemRead=0"); } },

    { "RegWrite_wire", "RegWrite", {{ {0.885f,0.085f}, {0.950f,0.085f}, {0.950f,0.030f}, {0.350f,0.030f}, {0.350f,0.270f} }},
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; },
      [](const CPU& c){ return c.mem_wb.regWrite ? std::string("RegWrite=1") : std::string("RegWrite=0"); } },

    { "MemToReg_wire", "MemToReg", {{ {0.885f,0.095f}, {0.920f,0.095f}, {0.920f,0.330f} }},
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.memToReg; },
      [](const CPU& c){ return c.mem_wb.memToReg ? std::string("MemToReg=1") : std::string("MemToReg=0"); } },

    // --- Forwarding paths ---
    { "FwdA_wire", "ForwardA", {{ {0.660f,0.830f}, {0.510f,0.830f}, {0.510f,0.395f} }},
      [](const CPU& c){ return c.enableForwarding && c.ex_mem.valid && c.ex_mem.regWrite; },
      [](const CPU& c){ return std::string("EX/MEM Forward"); } },

    { "FwdB_wire", "ForwardB", {{ {0.705f,0.830f}, {0.900f,0.830f}, {0.900f,0.860f}, {0.510f,0.860f}, {0.510f,0.555f} }},
      [](const CPU& c){ return c.enableForwarding && c.mem_wb.valid && c.mem_wb.regWrite; },
      [](const CPU& c){ return std::string("MEM/WB Forward"); } },

    // --- ALU result data wire ---
    { "ALU_out", "ALU Result", {{ {0.675f,0.405f}, {0.725f,0.405f} }},
      [](const CPU& c){ return c.ex_mem.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.ex_mem.aluResult); return std::string(b); } },

    // --- DataMem to WB mux ---
    { "DataMem_out", "MemData", {{ {0.855f,0.400f}, {0.885f,0.400f} }},
      [](const CPU& c){ return c.mem_wb.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.mem_wb.readData); return std::string(b); } },

    { "ALU_to_WB", "ALU->WB", {{ {0.885f,0.360f}, {0.920f,0.360f} }},
      [](const CPU& c){ return c.mem_wb.valid && !c.mem_wb.memToReg; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.mem_wb.aluResult); return std::string(b); } },

    // --- Register data out wires ---
    { "RegData1_out", "Read Data 1", {{ {0.390f,0.340f}, {0.445f,0.340f} }},
      [](const CPU& c){ return c.id_ex.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.id_ex.regData1); return std::string(b); } },

    { "RegData2_out", "Read Data 2", {{ {0.390f,0.410f}, {0.445f,0.410f} }},
      [](const CPU& c){ return c.id_ex.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.id_ex.regData2); return std::string(b); } },

    { "SignExt_out", "Sign-Extended Imm", {{ {0.375f,0.585f}, {0.445f,0.585f} }},
      [](const CPU& c){ return c.id_ex.valid; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"0x%X",c.id_ex.signExtImm); return std::string(b); } },

    // --- Flush wires ---
    { "IF_Flush", "IF.Flush", {{ {0.020f,0.020f}, {0.960f,0.020f} }},
      [](const CPU& c){ return (c.hazardFlags & 0x2) != 0; },
      [](const CPU& c){ return std::string("Pipeline Flush Active"); } },

    { "HDU_Stall", "Hazard Stall", {{ {0.215f,0.090f}, {0.190f,0.090f} }},
      [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; },
      [](const CPU& c){ return std::string("Load-Use Stall"); } },

    // --- WB mux output (writeback data) ---
    { "WB_out", "WB Data", {{ {0.945f,0.373f}, {0.970f,0.373f}, {0.970f,0.920f}, {0.305f,0.920f}, {0.305f,0.470f} }},
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; },
      [](const CPU& c){ char b[20]; snprintf(b,sizeof(b),"WB:R%d",c.mem_wb.destReg); return std::string(b); } },
};

// ---- Geometry Helpers -----------------------------------------------------

static float PointToSegmentDistSq(ImVec2 p, ImVec2 a, ImVec2 b) {
    ImVec2 ab = { b.x - a.x, b.y - a.y };
    ImVec2 ap = { p.x - a.x, p.y - a.y };
    float dot = ab.x * ap.x + ab.y * ap.y;
    float lenSq = ab.x * ab.x + ab.y * ab.y;
    float t = (lenSq > 0.0f) ? (dot / lenSq) : 0.0f;
    t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t;
    float cx = a.x + t * ab.x - p.x;
    float cy = a.y + t * ab.y - p.y;
    return cx * cx + cy * cy;
}

// ---- Shape Renderers ------------------------------------------------------

static void DrawNodeShape(ImDrawList* dl, const SchematicNode& n,
                          ImVec2 origin, float W, float H,
                          bool hovered, bool selected) {
    float x0 = origin.x + n.x * W;
    float y0 = origin.y + n.y * H;
    float x1 = x0 + n.w * W;
    float y1 = y0 + n.h * H;

    ImU32 fill   = (n.shape == Shape::Latch) ? Pal::LatchFill : Pal::NodeFill;
    ImU32 border = selected ? Pal::NodeSelected : (hovered ? Pal::NodeHover :
                   (n.shape == Shape::Latch ? Pal::LatchBorder : Pal::NodeBorder));
    float thick  = (hovered || selected) ? 2.5f : 1.2f;

    switch (n.shape) {
    case Shape::Rect:
    case Shape::Latch:
        dl->AddRectFilled(ImVec2(x0,y0), ImVec2(x1,y1), fill, 3.0f);
        dl->AddRect(ImVec2(x0,y0), ImVec2(x1,y1), border, 3.0f, 0, thick);
        break;
    case Shape::Ellipse: {
        float cx = (x0+x1)*0.5f, cy = (y0+y1)*0.5f;
        ImVec2 radii((x1-x0)*0.5f, (y1-y0)*0.5f);
        dl->AddEllipseFilled(ImVec2(cx,cy), radii, fill);
        dl->AddEllipse(ImVec2(cx,cy), radii, border, 0.0f, 0, thick);
        break;
    }
    case Shape::Trapezoid: {
        // Narrow at top, wide at bottom (like a mux selector)
        float inset = (x1 - x0) * 0.30f;
        ImVec2 pts[4] = {
            ImVec2(x0 + inset, y0),  // top-left (inset)
            ImVec2(x1 - inset, y0),  // top-right (inset)
            ImVec2(x1, y1),          // bottom-right
            ImVec2(x0, y1),          // bottom-left
        };
        dl->AddConvexPolyFilled(pts, 4, fill);
        dl->AddPolyline(pts, 4, border, ImDrawFlags_Closed, thick);
        break;
    }
    }

    // --- Label ---
    ImVec2 textSize = ImGui::CalcTextSize(n.label);
    float tx = x0 + (x1 - x0 - textSize.x) * 0.5f;
    float ty = y0 + (n.shape == Shape::Latch ? 4.0f : (y1 - y0 - textSize.y) * 0.5f);
    if (n.shape == Shape::Latch) {
        // Rotate label concept: just draw it at the top, small
        tx = x0 + 2.0f;
    }
    dl->AddText(ImVec2(tx, ty), Pal::LabelColor, n.label);
}

static void DrawNodeData(ImDrawList* dl, const SchematicNode& n,
                         ImVec2 origin, float W, float H, const CPU& cpu) {
    if (!n.dataFmt) return;
    std::string data = n.dataFmt(cpu);
    if (data.empty()) return;

    float x0 = origin.x + n.x * W;
    float y0 = origin.y + n.y * H;
    float nw = n.w * W;
    float nh = n.h * H;

    ImVec2 textSize = ImGui::CalcTextSize(data.c_str());
    float tx = x0 + (nw - textSize.x) * 0.5f;
    float ty = y0 + nh - textSize.y - 4.0f;

    // Background pill
    float px = 3.0f, py = 1.0f;
    dl->AddRectFilled(ImVec2(tx-px, ty-py), ImVec2(tx+textSize.x+px, ty+textSize.y+py),
                      IM_COL32(10,11,14,200), 3.0f);
    dl->AddText(ImVec2(tx, ty), Pal::DataColor, data.c_str());
}

// ---- Public API -----------------------------------------------------------

void DrawArchitectureWidget(const CPU& cpu) {
    ImGui::Begin("MIPS CPU Architecture Diagram");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float W = avail.x;
    float H = avail.y;

    // Reserve the drawing area so ImGui knows the size
    ImGui::Dummy(avail);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetMousePos();

    // Background
    dl->AddRectFilled(origin, ImVec2(origin.x + W, origin.y + H), Pal::Background);

    // Helper: normalised → screen
    auto toScreen = [&](ImVec2 uv) -> ImVec2 {
        return ImVec2(origin.x + uv.x * W, origin.y + uv.y * H);
    };

    // Helper: node bounding box in screen
    auto nodeRect = [&](const SchematicNode& n, ImVec2& outMin, ImVec2& outMax) {
        outMin = ImVec2(origin.x + n.x * W, origin.y + n.y * H);
        outMax = ImVec2(outMin.x + n.w * W, outMin.y + n.h * H);
    };

    // ---- Phase 1: Draw wires (behind nodes) --------------------------------
    const float hoverThresholdSq = 64.0f; // 8px
    std::string newHoveredWireId;

    for (const auto& wire : kWires) {
        bool active = wire.isActive(cpu);
        bool selected = (g_schematicSelection.selectedWireId == wire.id);
        bool hovered = false;

        // Hit test: point-to-segment for all segments
        if (wire.points.size() >= 2) {
            for (size_t i = 0; i + 1 < wire.points.size(); ++i) {
                ImVec2 a = toScreen(wire.points[i]);
                ImVec2 b = toScreen(wire.points[i+1]);
                float dSq = PointToSegmentDistSq(mouse, a, b);
                if (dSq < hoverThresholdSq) {
                    hovered = true;
                    newHoveredWireId = wire.id;
                    break;
                }
            }
        }

        // Determine color/thickness
        ImU32 col;
        float thick;
        if (selected) {
            col = Pal::WireSelected; thick = 3.5f;
        } else if (hovered) {
            col = Pal::NodeHover; thick = 3.0f;
        } else if (wire.id == std::string("IF_Flush") && active) {
            col = Pal::WireFlush; thick = 2.5f;
        } else if (active) {
            col = Pal::WireActive; thick = 2.0f;
        } else {
            col = Pal::WireInactive; thick = 1.2f;
        }

        // Draw polyline
        if (wire.points.size() >= 2) {
            for (size_t i = 0; i + 1 < wire.points.size(); ++i) {
                dl->AddLine(toScreen(wire.points[i]), toScreen(wire.points[i+1]),
                            col, thick);
            }
            // Draw dots at junction waypoints
            for (size_t i = 1; i + 1 < wire.points.size(); ++i) {
                dl->AddCircleFilled(toScreen(wire.points[i]), 3.0f, col);
            }
        }

        // Click detection
        if (hovered && ImGui::IsMouseClicked(0)) {
            g_schematicSelection.selectedNodeId.clear();
            g_schematicSelection.selectedWireId = wire.id;
        }

        // Tooltip
        if (hovered && wire.tooltip) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(wire.signalName);
            std::string tip = wire.tooltip(cpu);
            if (!tip.empty()) {
                ImGui::Separator();
                ImGui::TextUnformatted(tip.c_str());
            }
            ImGui::EndTooltip();
        }
    }

    // ---- Phase 2: Draw nodes (on top of wires) -----------------------------
    // Draw all kNodes + the forwarding unit
    auto drawOneNode = [&](const SchematicNode& n) {
        ImVec2 nMin, nMax;
        nodeRect(n, nMin, nMax);

        bool hovered = (mouse.x >= nMin.x && mouse.x <= nMax.x &&
                        mouse.y >= nMin.y && mouse.y <= nMax.y);
        bool selected = (g_schematicSelection.selectedNodeId == n.id);

        DrawNodeShape(dl, n, origin, W, H, hovered, selected);
        DrawNodeData(dl, n, origin, W, H, cpu);

        if (hovered && ImGui::IsMouseClicked(0)) {
            g_schematicSelection.selectedWireId.clear();
            g_schematicSelection.selectedNodeId = n.id;
        }

        if (hovered) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", n.label);
            if (n.dataFmt) {
                std::string data = n.dataFmt(cpu);
                if (!data.empty()) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(1,0.84f,0.6f,1), "%s", data.c_str());
                }
            }
            ImGui::EndTooltip();
        }
    };

    for (const auto& n : kNodes)
        drawOneNode(n);
    drawOneNode(kFwdUnit);

    // ---- Phase 3: Stage labels across the top ------------------------------
    const char* stageLabels[] = { "IF", "ID", "EX", "MEM", "WB" };
    float stageX[] = { 0.12f, 0.30f, 0.55f, 0.78f, 0.93f };
    for (int i = 0; i < 5; ++i) {
        ImVec2 pos = ImVec2(origin.x + stageX[i] * W, origin.y + 4.0f);
        dl->AddText(pos, Pal::ControlActive, stageLabels[i]);
    }

    // ---- Phase 4: Active signal indicator badges ---------------------------
    struct Badge { const char* label; float u,v; std::function<bool(const CPU&)> active; };
    static const Badge badges[] = {
        {"FWD",  0.640f, 0.780f, [](const CPU& c){ return c.enableForwarding; }},
        {"HDU",  0.230f, 0.025f, [](const CPU& c){ return c.enableHazardDetection; }},
        {"BTB",  0.105f, 0.085f, [](const CPU& c){ return c.enableBranchPrediction; }},
    };
    for (const auto& b : badges) {
        ImVec2 pos = toScreen(ImVec2(b.u, b.v));
        bool on = b.active(cpu);
        ImU32 col = on ? Pal::ControlActive : Pal::WireInactive;
        ImVec2 ts = ImGui::CalcTextSize(b.label);
        float px = 5.0f;
        dl->AddRectFilled(ImVec2(pos.x-px, pos.y-1), ImVec2(pos.x+ts.x+px, pos.y+ts.y+2),
                          IM_COL32(15,16,20, on ? 220 : 150), 3.0f);
        dl->AddText(pos, col, b.label);
    }

    ImGui::End();
}

// No GPU resources to clean up in the programmatic renderer.
void CleanupArchitectureWidget() {
    // No-op: no textures to release.
}

} // namespace MIPS::UI
