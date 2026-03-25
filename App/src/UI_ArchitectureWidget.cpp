// ---------------------------------------------------------------------------
// UI_ArchitectureWidget.cpp  —  Programmatic Vector MIPS Datapath (Early Branch)
// Infinite canvas with zoom/pan and dynamic Manhattan wire routing.
// ---------------------------------------------------------------------------
#include "UI_Widgets.h"
#include <imgui.h>
#include <cstdio>
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <functional>
#include <cfloat>
#include <algorithm>

namespace MIPS::UI {

SchematicSelection g_schematicSelection;

// ---- Canvas Camera State --------------------------------------------------
static ImVec2 s_canvasScroll = {0.0f, 0.0f};
static float  s_canvasZoom   = 1.0f;
constexpr float kZoomMin = 0.3f;
constexpr float kZoomMax = 4.0f;

// ---- Palette --------------------------------------------------------------
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
    constexpr ImU32 ControlBase    = IM_COL32(50, 130, 180, 180);
    constexpr ImU32 DataBase       = IM_COL32(100, 103, 115, 200);
    constexpr ImU32 WireLabelBg    = IM_COL32(18,  19,  22, 210);
    constexpr ImU32 WireLabelText  = IM_COL32(160, 165, 180, 230);
    constexpr ImU32 BitWidthText   = IM_COL32(180, 140, 100, 220);
}

// ---- Enums & Structs ------------------------------------------------------
enum class Shape { Rect, Ellipse, Trapezoid, Latch };
enum class WireType { Data, Control };
enum class Port { Top, Bottom, Left, Right, Center };
enum class RouteStyle {
    Direct,   // 1 segment
    L_HV,     // horizontal then vertical
    L_VH,     // vertical then horizontal
    Z_HVH,    // 3 segments via channelY
    Z_VHV,    // 3 segments via channelX
    U_Top,    // up via channelY, across, back down (4 segments)
    U_Bottom, // down via channelY, across, back up
    Custom    // manual waypoints with enforced endpoints
};

struct SchematicNode {
    const char* id;
    const char* label;
    float x, y, w, h;  // normalised bounding box
    Shape shape;
    std::function<std::string(const CPU&)> dataFmt;
};

struct WireAnchor {
    const char* nodeId;
    Port port;
    float offset; // [-0.5, 0.5] along the port edge, 0 = center
};

struct SchematicWire {
    const char* id;
    const char* signalName;
    WireAnchor source;
    WireAnchor dest;
    RouteStyle route;
    float channel;                       // Y for HVH/U_Top/U_Bottom, X for VHV
    std::vector<ImVec2> customWaypoints; // only for RouteStyle::Custom (normalised UV)
    std::function<bool(const CPU&)> isActive;
    std::function<std::string(const CPU&)> tooltip;
    WireType type = WireType::Data;
    int bitWidth  = 0;
};

// ---- Node Definitions -----------------------------------------------------
static const std::array<SchematicNode, 29> kNodes = {{
    // --- IF Stage ---
    { "Const80000180", "80000180",0.005f, 0.450f, 0.035f, 0.030f, Shape::Rect, nullptr },
    { "ExcMux",   "Mux",     0.045f, 0.380f, 0.022f, 0.120f, Shape::Trapezoid, nullptr },
    { "PC",       "PC",      0.080f, 0.400f, 0.035f, 0.080f, Shape::Rect,
      [](const CPU& c) { char b[20]; snprintf(b,sizeof(b),"0x%04X",c.getState().pc); return std::string(b); } },
    { "InstrMem", "Instruction\nMemory", 0.130f, 0.300f, 0.070f, 0.260f, Shape::Rect, nullptr },
    { "AddPC4",   "+4",      0.145f, 0.150f, 0.035f, 0.055f, Shape::Rect, nullptr },
    { "IF_ID",    "IF/ID",   0.220f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- ID Stage ---
    { "HDU",      "Hazard\nDetection",  0.250f, 0.035f, 0.070f, 0.055f, Shape::Rect, nullptr },
    { "Control",  "Control", 0.260f, 0.180f, 0.050f, 0.080f, Shape::Ellipse, nullptr },
    { "IDFlushOR","OR",      0.300f, 0.080f, 0.025f, 0.035f, Shape::Ellipse, nullptr },
    { "CtrlMux",  "Mux",     0.320f, 0.180f, 0.020f, 0.080f, Shape::Trapezoid, nullptr },
    { "AddBr",    "Add",     0.360f, 0.110f, 0.035f, 0.055f, Shape::Rect, nullptr },
    { "ShiftL2",  "Shift\nLeft 2", 0.310f, 0.130f, 0.040f, 0.040f, Shape::Rect, nullptr },
    { "Regs",     "Registers", 0.320f, 0.350f, 0.085f, 0.200f, Shape::Rect,
      [](const CPU& c) { if(!c.id_ex.valid) return std::string();
                          char b[40]; snprintf(b,sizeof(b),"Rs:%d Rt:%d",c.id_ex.rs,c.id_ex.rt); return std::string(b); } },
    { "EqCmp",    "=",       0.415f, 0.400f, 0.025f, 0.045f, Shape::Ellipse, nullptr },
    { "BranchAND","AND",     0.415f, 0.180f, 0.025f, 0.035f, Shape::Ellipse, nullptr },
    { "SignExt",  "Sign-\nextend", 0.315f, 0.620f, 0.060f, 0.050f, Shape::Ellipse, nullptr },
    { "ID_EX",    "ID/EX",   0.460f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- EX Stage ---
    { "CauseReg", "Cause",   0.530f, 0.140f, 0.040f, 0.040f, Shape::Rect, nullptr },
    { "EPCReg",   "EPC",     0.530f, 0.195f, 0.040f, 0.040f, Shape::Rect, nullptr },
    { "EXFlushMux","Mux",    0.485f, 0.080f, 0.020f, 0.080f, Shape::Trapezoid, nullptr },
    { "FwdMuxA",  "Mux",     0.510f, 0.360f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "FwdMuxB",  "Mux",     0.510f, 0.520f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "ALUSrcMux","Mux",     0.565f, 0.470f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "ALU",      "ALU",     0.620f, 0.360f, 0.055f, 0.190f, Shape::Trapezoid,
      [](const CPU& c) { if(!c.ex_mem.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"0x%X",c.ex_mem.aluResult); return std::string(b); } },
    { "RegDstMux","Mux",     0.565f, 0.680f, 0.025f, 0.085f, Shape::Trapezoid, nullptr },
    { "EX_MEM",   "EX/MEM",  0.725f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- MEM Stage ---
    { "DataMem",  "Data\nMemory", 0.780f, 0.330f, 0.075f, 0.230f, Shape::Rect,
      [](const CPU& c) { if(!c.mem_wb.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"R:0x%X",c.mem_wb.readData); return std::string(b); } },
    { "MEM_WB",   "MEM/WB",  0.885f, 0.040f, 0.014f, 0.850f, Shape::Latch, nullptr },

    // --- WB Stage ---
    { "WBMux",    "Mux",     0.920f, 0.380f, 0.025f, 0.085f, Shape::Trapezoid, nullptr }
}};

static const SchematicNode kFwdUnit =
    { "FwdUnit", "Forwarding\nUnit", 0.620f, 0.800f, 0.085f, 0.060f, Shape::Rect, nullptr };

// ---- Node Lookup ----------------------------------------------------------
// Returns pointer to node or nullptr if not found
static const SchematicNode* FindNode(const char* id) {
    for (const auto& n : kNodes)
        if (std::string(n.id) == id) return &n;
    if (std::string(kFwdUnit.id) == id) return &kFwdUnit;
    return nullptr;
}

// ---- Anchor Resolution ----------------------------------------------------
// Returns normalised UV coordinate for a port on a node's bounding box.
// Falls back to (0,0) if node not found.
static ImVec2 ResolveAnchorUV(const WireAnchor& a) {
    const SchematicNode* n = FindNode(a.nodeId);
    if (!n) return ImVec2(0, 0); // safe fallback

    float x0 = n->x, y0 = n->y;
    float x1 = n->x + n->w, y1 = n->y + n->h;
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f;
    float off = a.offset; // [-0.5, 0.5]

    switch (a.port) {
    case Port::Left:   return ImVec2(x0, cy + off * (y1 - y0));
    case Port::Right:  return ImVec2(x1, cy + off * (y1 - y0));
    case Port::Top:    return ImVec2(cx + off * (x1 - x0), y0);
    case Port::Bottom: return ImVec2(cx + off * (x1 - x0), y1);
    case Port::Center: return ImVec2(cx, cy);
    }
    return ImVec2(cx, cy);
}

// ---- Manhattan Routing Engine ---------------------------------------------
// Generates normalised UV waypoints (including src and dst) based on RouteStyle.
static std::vector<ImVec2> ResolveWirePointsUV(const SchematicWire& wire) {
    ImVec2 src = ResolveAnchorUV(wire.source);
    ImVec2 dst = ResolveAnchorUV(wire.dest);
    std::vector<ImVec2> pts;

    switch (wire.route) {
    case RouteStyle::Direct:
        pts = { src, dst };
        break;
    case RouteStyle::L_HV:
        pts = { src, ImVec2(dst.x, src.y), dst };
        break;
    case RouteStyle::L_VH:
        pts = { src, ImVec2(src.x, dst.y), dst };
        break;
    case RouteStyle::Z_HVH:
        pts = { src, ImVec2(src.x, wire.channel), ImVec2(dst.x, wire.channel), dst };
        break;
    case RouteStyle::Z_VHV:
        pts = { src, ImVec2(wire.channel, src.y), ImVec2(wire.channel, dst.y), dst };
        break;
    case RouteStyle::U_Top: {
        float ch = wire.channel;
        pts = { src, ImVec2(src.x, ch), ImVec2(dst.x, ch), dst };
        break;
    }
    case RouteStyle::U_Bottom: {
        float ch = wire.channel;
        pts = { src, ImVec2(src.x, ch), ImVec2(dst.x, ch), dst };
        break;
    }
    case RouteStyle::Custom: {
        // Enforce: always starts at resolved srcAnchor, ends at resolved dstAnchor
        pts.push_back(src);
        for (const auto& wp : wire.customWaypoints)
            pts.push_back(wp);
        pts.push_back(dst);
        break;
    }
    }
    return pts;
}

// ---- Wire Definitions (Port-Anchored) -------------------------------------
#define A(node, port, off) WireAnchor{node, Port::port, off}

static const std::vector<SchematicWire> kWires = {
    // =====================================================================
    // IF Stage
    // =====================================================================
    { "PC_out", "PC",
      A("PC", Right, 0), A("InstrMem", Left, 0.1f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "PC_to_Add", "PC",
      A("PC", Top, 0.3f), A("AddPC4", Bottom, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "PC4_out", "PC+4",
      A("AddPC4", Right, 0), A("IF_ID", Left, -0.35f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "InstrMem_out", "Instr",
      A("InstrMem", Right, 0.1f), A("IF_ID", Left, 0.08f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },

    // =====================================================================
    // ID Stage: Early Branch
    // =====================================================================
    { "Reg1_to_EqCmp", "Rs",
      A("Regs", Right, -0.15f), A("EqCmp", Left, -0.2f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "Reg2_to_EqCmp", "Rt",
      A("Regs", Right, 0.25f), A("EqCmp", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "EqCmp_to_AND", "Zero",
      A("EqCmp", Top, 0), A("BranchAND", Bottom, 0.1f),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Control, 1 },
    { "Branch_wire", "Branch",
      A("Control", Right, 0.1f), A("BranchAND", Left, 0),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.branch; }, nullptr, WireType::Control, 1 },
    { "PCSrc_wire", "PCSrc",
      A("BranchAND", Right, 0), A("ExcMux", Top, 0.2f),
      RouteStyle::Custom, 0, {{ {0.450f,0.195f}, {0.450f,0.028f}, {0.038f,0.028f}, {0.038f,0.390f} }},
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Control, 1 },
    { "BrTarget_wire", "Br Target",
      A("AddBr", Right, 0), A("ExcMux", Left, -0.1f),
      RouteStyle::Custom, 0, {{ {0.405f,0.138f}, {0.405f,0.015f}, {0.025f,0.015f}, {0.025f,0.400f} }},
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Data, 32 },
    { "SignExt_to_SL2", "Imm",
      A("SignExt", Top, 0.3f), A("ShiftL2", Bottom, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "SL2_to_AddBr", "SL2",
      A("ShiftL2", Right, 0), A("AddBr", Left, 0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "PC4_to_AddBr", "PC+4",
      A("IF_ID", Right, -0.38f), A("AddBr", Left, -0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },

    // =====================================================================
    // ID Stage: Hazard Detection Unit
    // =====================================================================
    { "HDU_rs", "IF/ID.Rs",
      A("IF_ID", Right, -0.18f), A("HDU", Bottom, -0.25f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableHazardDetection && c.if_id.valid; }, nullptr, WireType::Control, 5 },
    { "HDU_rt", "IF/ID.Rt",
      A("IF_ID", Right, -0.15f), A("HDU", Bottom, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableHazardDetection && c.if_id.valid; }, nullptr, WireType::Control, 5 },
    { "HDU_idex_rt", "ID/EX.Rt",
      A("ID_EX", Right, 0.38f), A("HDU", Right, 0),
      RouteStyle::Custom, 0, {{ {0.480f,0.730f}, {0.480f,0.055f}, {0.320f,0.055f} }},
      [](const CPU& c){ return c.enableHazardDetection && c.id_ex.valid; }, nullptr, WireType::Control, 5 },
    { "HDU_memread", "MemRead",
      A("ID_EX", Right, -0.32f), A("HDU", Top, 0.3f),
      RouteStyle::Custom, 0, {{ {0.478f,0.170f}, {0.478f,0.045f}, {0.310f,0.045f} }},
      [](const CPU& c){ return c.enableHazardDetection && c.id_ex.valid && c.id_ex.memRead; }, nullptr, WireType::Control, 1 },
    { "HDU_stall", "Stall",
      A("HDU", Bottom, 0.15f), A("IDFlushOR", Left, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },
    { "HDU_pc_disable", "PCWrite",
      A("HDU", Left, 0), A("PC", Top, 0),
      RouteStyle::Custom, 0, {{ {0.240f,0.060f}, {0.240f,0.020f}, {0.080f,0.020f} }},
      [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },

    // =====================================================================
    // Flushes
    // =====================================================================
    { "ID_Flush_Out", "ID.Flush",
      A("IDFlushOR", Right, 0), A("CtrlMux", Top, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },
    { "IF_Flush", "IF.Flush",
      A("BranchAND", Top, 0), A("IF_ID", Top, 0),
      RouteStyle::U_Top, 0.020f, {}, [](const CPU& c){ return (c.hazardFlags & 0x2) != 0; }, nullptr, WireType::Control, 1 },
    { "EX_Flush", "EX.Flush",
      A("BranchAND", Top, 0.3f), A("EXFlushMux", Top, 0),
      RouteStyle::U_Top, 0.025f, {}, [](const CPU& c){ return (c.hazardFlags & 0x4) != 0; }, nullptr, WireType::Control, 1 },

    // =====================================================================
    // Control Signal Bundles
    // =====================================================================
    { "Ctrl_to_Mux", "Ctrl",
      A("Control", Right, 0.15f), A("CtrlMux", Left, 0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlWB_to_IDEX", "WB",
      A("CtrlMux", Right, -0.3f), A("ID_EX", Left, -0.38f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlM_to_IDEX", "M",
      A("CtrlMux", Right, -0.1f), A("ID_EX", Left, -0.32f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlEX_to_IDEX", "EX",
      A("CtrlMux", Right, 0.1f), A("ID_EX", Left, -0.26f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
    { "WB_IDEX_to_EXMEM", "WB",
      A("ID_EX", Right, -0.42f), A("EX_MEM", Left, -0.42f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Control, 0 },
    { "M_IDEX_to_EXMEM", "M",
      A("ID_EX", Right, -0.37f), A("EX_MEM", Left, -0.37f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Control, 0 },
    { "WB_EXMEM_to_MEMWB", "WB",
      A("EX_MEM", Right, -0.42f), A("MEM_WB", Left, -0.42f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Control, 0 },
    { "M_to_DataMem", "M",
      A("EX_MEM", Right, -0.37f), A("DataMem", Top, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.ex_mem.valid && (c.ex_mem.memRead || c.ex_mem.memWrite); }, nullptr, WireType::Control, 0 },
    { "WB_to_WBMux", "WB",
      A("MEM_WB", Right, -0.42f), A("WBMux", Top, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Control, 0 },
    { "ALUSrc_wire", "ALUSrc",
      A("ID_EX", Right, -0.26f), A("ALUSrcMux", Top, 0),
      RouteStyle::Z_VHV, 0.550f, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.aluSrc; }, nullptr, WireType::Control, 1 },

    // =====================================================================
    // Data Paths
    // =====================================================================
    { "RegDst_wire", "RegDst",
      A("ID_EX", Right, -0.26f), A("RegDstMux", Top, 0),
      RouteStyle::Z_VHV, 0.490f, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.regDst; }, nullptr, WireType::Control, 1 },
    { "DestReg_out", "Dst Reg",
      A("RegDstMux", Right, 0.15f), A("EX_MEM", Left, 0.35f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 5 },
    { "RegData1_out", "RdData1",
      A("ID_EX", Right, -0.08f), A("FwdMuxA", Left, 0),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "RegData2_out", "RdData2",
      A("ID_EX", Right, 0.06f), A("FwdMuxB", Left, 0),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "SignExt_out", "Sign-Ext",
      A("SignExt", Right, 0), A("ID_EX", Left, 0.30f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "SignExt_IDEX_out", "Imm",
      A("ID_EX", Right, 0.30f), A("ALUSrcMux", Bottom, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.aluSrc; }, nullptr, WireType::Data, 32 },
    { "MuxA_to_ALU", "Op1",
      A("FwdMuxA", Right, 0), A("ALU", Left, -0.2f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "MuxB_to_ALUSrc", "Op2",
      A("FwdMuxB", Right, 0), A("ALUSrcMux", Left, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "ALUSrc_to_ALU", "ALU In",
      A("ALUSrcMux", Right, 0), A("ALU", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "ALU_out", "ALU Res",
      A("ALU", Right, 0), A("EX_MEM", Left, 0.05f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },
    { "RegData2_passthru", "Write Data",
      A("FwdMuxB", Right, 0.3f), A("EX_MEM", Left, 0.22f),
      RouteStyle::Custom, 0, {{ {0.500f,0.620f}, {0.725f,0.620f} }},
      [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "DataMem_out", "Read Data",
      A("DataMem", Right, 0.15f), A("MEM_WB", Left, 0.08f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
    { "ALU_passthru", "ALU",
      A("EX_MEM", Right, 0.05f), A("MEM_WB", Left, -0.05f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },
    { "MemData_to_WBMux", "Mem",
      A("MEM_WB", Right, 0.08f), A("WBMux", Left, 0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
    { "ALU_to_WBMux", "ALU",
      A("MEM_WB", Right, -0.05f), A("WBMux", Left, -0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
    { "WB_out", "WB Data",
      A("WBMux", Right, 0), A("Regs", Bottom, 0.2f),
      RouteStyle::Custom, 0, {{ {0.970f,0.423f}, {0.970f,0.920f}, {0.300f,0.920f}, {0.300f,0.520f} }},
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Data, 32 },
    { "RegWrite_wire", "RegWrite",
      A("MEM_WB", Right, -0.42f), A("Regs", Left, -0.15f),
      RouteStyle::Custom, 0, {{ {0.960f,0.108f}, {0.960f,0.940f}, {0.280f,0.940f}, {0.280f,0.390f} }},
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Control, 1 },
    // EX/MEM -> DataMem write data
    { "EXMEM_WriteData", "Wr Data",
      A("EX_MEM", Right, 0.22f), A("DataMem", Left, 0.15f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid && c.ex_mem.memWrite; }, nullptr, WireType::Data, 32 },
    // EX/MEM -> DataMem address (ALU result)
    { "EXMEM_Addr", "Addr",
      A("EX_MEM", Right, 0.05f), A("DataMem", Left, -0.1f),
      RouteStyle::Direct, 0, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },

    // =====================================================================
    // Forwarding Unit
    // =====================================================================
    { "IDEX_rs_to_Fwd", "Rs",
      A("ID_EX", Right, 0.38f), A("FwdUnit", Left, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableForwarding && c.id_ex.valid; }, nullptr, WireType::Data, 5 },
    { "IDEX_rt_to_Fwd", "Rt",
      A("ID_EX", Right, 0.40f), A("FwdUnit", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableForwarding && c.id_ex.valid; }, nullptr, WireType::Data, 5 },
    { "Fwd_EX_MEM", "EX/MEM.Rd",
      A("EX_MEM", Right, 0.35f), A("FwdUnit", Right, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableForwarding && c.ex_mem.valid && c.ex_mem.regWrite && c.ex_mem.destReg != 0; }, nullptr, WireType::Control, 5 },
    { "Fwd_MEM_WB", "MEM/WB.Rd",
      A("MEM_WB", Right, 0.35f), A("FwdUnit", Right, 0.2f),
      RouteStyle::Custom, 0, {{ {0.910f,0.720f}, {0.910f,0.850f} }},
      [](const CPU& c){ return c.enableForwarding && c.mem_wb.valid && c.mem_wb.regWrite && c.mem_wb.destReg != 0; }, nullptr, WireType::Control, 5 },
    { "FwdUnit_to_MuxA", "FwdA",
      A("FwdUnit", Top, -0.2f), A("FwdMuxA", Bottom, 0),
      RouteStyle::Custom, 0, {{ {0.640f,0.760f}, {0.500f,0.760f}, {0.500f,0.445f} }},
      [](const CPU& c){ return c.forwardA != 0; }, nullptr, WireType::Control, 0 },
    { "FwdUnit_to_MuxB", "FwdB",
      A("FwdUnit", Top, 0.2f), A("FwdMuxB", Bottom, 0),
      RouteStyle::Custom, 0, {{ {0.680f,0.780f}, {0.490f,0.780f}, {0.490f,0.605f} }},
      [](const CPU& c){ return c.forwardB != 0; }, nullptr, WireType::Control, 0 },
    { "Fwd_EXMEM_data", "EX Fwd",
      A("EX_MEM", Right, 0.05f), A("FwdMuxA", Right, 0),
      RouteStyle::Custom, 0, {{ {0.745f,0.455f}, {0.745f,0.690f}, {0.500f,0.690f} }},
      [](const CPU& c){ return c.forwardA == 2 || c.forwardB == 2; }, nullptr, WireType::Data, 32 },
    { "Fwd_MEMWB_data", "MEM Fwd",
      A("MEM_WB", Right, -0.05f), A("FwdMuxB", Right, 0),
      RouteStyle::Custom, 0, {{ {0.915f,0.423f}, {0.915f,0.700f}, {0.500f,0.700f} }},
      [](const CPU& c){ return c.forwardA == 1 || c.forwardB == 1; }, nullptr, WireType::Data, 32 },
};

#undef A

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

static float SegmentLenSq(ImVec2 a, ImVec2 b) {
    float dx = b.x - a.x, dy = b.y - a.y;
    return dx * dx + dy * dy;
}

// ---- Shape Renderers ------------------------------------------------------
static void DrawNodeShape(ImDrawList* dl, const SchematicNode& n,
                          ImVec2 origin, float W, float H, float zoom, ImVec2 scroll,
                          bool hovered, bool selected) {
    float x0 = origin.x + n.x * W * zoom + scroll.x;
    float y0 = origin.y + n.y * H * zoom + scroll.y;
    float x1 = x0 + n.w * W * zoom;
    float y1 = y0 + n.h * H * zoom;

    ImU32 fill   = (n.shape == Shape::Latch) ? Pal::LatchFill : Pal::NodeFill;
    ImU32 border = selected ? Pal::NodeSelected : (hovered ? Pal::NodeHover : (n.shape == Shape::Latch ? Pal::LatchBorder : Pal::NodeBorder));
    float thick  = ((hovered || selected) ? 2.5f : 1.2f) * zoom;

    switch (n.shape) {
    case Shape::Rect:
    case Shape::Latch:
        dl->AddRectFilled(ImVec2(x0,y0), ImVec2(x1,y1), fill, 3.0f * zoom);
        dl->AddRect(ImVec2(x0,y0), ImVec2(x1,y1), border, 3.0f * zoom, 0, thick);
        if (n.shape == Shape::Latch) {
            float divY[] = { y0 + (y1-y0)*0.08f, y0 + (y1-y0)*0.16f, y0 + (y1-y0)*0.24f };
            for(int i=0; i<3; ++i)
                dl->AddLine(ImVec2(x0, divY[i]), ImVec2(x1, divY[i]), border, 1.5f * zoom);
        }
        break;
    case Shape::Ellipse: {
        float cx = (x0+x1)*0.5f, cy = (y0+y1)*0.5f;
        ImVec2 radii((x1-x0)*0.5f, (y1-y0)*0.5f);
        dl->AddEllipseFilled(ImVec2(cx,cy), radii, fill);
        dl->AddEllipse(ImVec2(cx,cy), radii, border, 0.0f, 0, thick);
        break;
    }
    case Shape::Trapezoid: {
        float inset = (x1 - x0) * 0.30f;
        ImVec2 pts[4] = { ImVec2(x0+inset, y0), ImVec2(x1-inset, y0), ImVec2(x1, y1), ImVec2(x0, y1) };
        dl->AddConvexPolyFilled(pts, 4, fill);
        dl->AddPolyline(pts, 4, border, ImDrawFlags_Closed, thick);
        break;
    }
    }

    // --- Label with font scaling (scaled by zoom) ---
    float nodeW = x1 - x0, nodeH = y1 - y0;
    ImFont* font = ImGui::GetFont();
    float baseFontSize = ImGui::GetFontSize();

    if (n.shape == Shape::Latch) {
        float fontSize = baseFontSize * zoom * 0.85f;
        dl->AddText(font, fontSize, ImVec2(x0 + 2.0f * zoom, y0 + 4.0f * zoom), Pal::LabelColor, n.label);
    } else {
        ImVec2 naturalSize = font->CalcTextSizeA(baseFontSize, FLT_MAX, 0.0f, n.label);
        float padFactor = 0.80f;
        float scaleX = (nodeW * padFactor) / (naturalSize.x > 0 ? naturalSize.x : 1.0f);
        float scaleY = (nodeH * padFactor) / (naturalSize.y > 0 ? naturalSize.y : 1.0f);
        float scale = (scaleX < scaleY) ? scaleX : scaleY;
        if (scale < 0.4f) scale = 0.4f;
        if (scale > 3.0f) scale = 3.0f;
        float fontSize = baseFontSize * scale;
        ImVec2 scaledSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, n.label);
        float tx = x0 + (nodeW - scaledSize.x) * 0.5f;
        float ty = y0 + (nodeH - scaledSize.y) * 0.5f;
        dl->AddText(font, fontSize, ImVec2(tx, ty), Pal::LabelColor, n.label);
    }
}

static void DrawNodeData(ImDrawList* dl, const SchematicNode& n,
                         ImVec2 origin, float W, float H, float zoom, ImVec2 scroll,
                         const CPU& cpu) {
    if (!n.dataFmt) return;
    std::string data = n.dataFmt(cpu);
    if (data.empty()) return;

    float x0 = origin.x + n.x * W * zoom + scroll.x;
    float y0 = origin.y + n.y * H * zoom + scroll.y;
    float nw = n.w * W * zoom, nh = n.h * H * zoom;

    ImFont* font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize() * zoom * 0.85f;
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, data.c_str());
    float tx = x0 + (nw - textSize.x) * 0.5f, ty = y0 + nh - textSize.y - 4.0f * zoom;

    float px = 3.0f * zoom, py = 1.0f * zoom;
    dl->AddRectFilled(ImVec2(tx-px, ty-py), ImVec2(tx+textSize.x+px, ty+textSize.y+py), IM_COL32(10,11,14,200), 3.0f * zoom);
    dl->AddText(font, fontSize, ImVec2(tx, ty), Pal::DataColor, data.c_str());
}

// ---- Public API -----------------------------------------------------------
void DrawArchitectureWidget(const CPU& cpu) {
    ImGui::Begin("MIPS CPU Architecture Diagram");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float W = avail.x, H = avail.y;

    // Reserve the drawing area
    ImGui::InvisibleButton("canvas", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
    bool canvasHovered = ImGui::IsItemHovered();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 mouse = ImGui::GetMousePos();

    // ---- Camera Input ----
    if (canvasHovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            // Zoom toward mouse cursor
            ImVec2 mouseRel = ImVec2(mouse.x - origin.x, mouse.y - origin.y);
            float oldZoom = s_canvasZoom;
            s_canvasZoom *= (wheel > 0) ? 1.1f : (1.0f / 1.1f);
            if (s_canvasZoom < kZoomMin) s_canvasZoom = kZoomMin;
            if (s_canvasZoom > kZoomMax) s_canvasZoom = kZoomMax;
            float zoomDelta = s_canvasZoom / oldZoom;
            // Adjust scroll so the point under mouse stays fixed
            s_canvasScroll.x = mouseRel.x - zoomDelta * (mouseRel.x - s_canvasScroll.x);
            s_canvasScroll.y = mouseRel.y - zoomDelta * (mouseRel.y - s_canvasScroll.y);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            s_canvasScroll.x += delta.x;
            s_canvasScroll.y += delta.y;
        }
        // Double-click middle to reset
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Middle)) {
            s_canvasScroll = {0, 0};
            s_canvasZoom = 1.0f;
        }
    }

    float zoom = s_canvasZoom;
    ImVec2 scroll = s_canvasScroll;

    // Background
    dl->AddRectFilled(origin, ImVec2(origin.x + W, origin.y + H), Pal::Background);
    dl->PushClipRect(origin, ImVec2(origin.x + W, origin.y + H), true);

    // Coordinate transform: normalised UV -> screen
    auto toScreen = [&](ImVec2 uv) -> ImVec2 {
        return ImVec2(origin.x + uv.x * W * zoom + scroll.x,
                      origin.y + uv.y * H * zoom + scroll.y);
    };

    // Node rect in screen space (for hit testing)
    auto nodeScreenRect = [&](const SchematicNode& n, ImVec2& outMin, ImVec2& outMax) {
        outMin = ImVec2(origin.x + n.x * W * zoom + scroll.x,
                        origin.y + n.y * H * zoom + scroll.y);
        outMax = ImVec2(outMin.x + n.w * W * zoom, outMin.y + n.h * H * zoom);
    };

    ImFont* font = ImGui::GetFont();
    float baseFontSize = ImGui::GetFontSize();

    // ---- Phase 1: Wires ----
    const float hoverThresholdSq = 64.0f;

    for (const auto& wire : kWires) {
        bool active = wire.isActive(cpu);
        bool selected = (g_schematicSelection.selectedWireId == wire.id);
        bool hovered = false;

        // Resolve dynamic points
        std::vector<ImVec2> uvPts = ResolveWirePointsUV(wire);
        std::vector<ImVec2> screenPts;
        screenPts.reserve(uvPts.size());
        for (const auto& uv : uvPts)
            screenPts.push_back(toScreen(uv));

        // Hit test
        if (screenPts.size() >= 2) {
            for (size_t i = 0; i + 1 < screenPts.size(); ++i) {
                if (PointToSegmentDistSq(mouse, screenPts[i], screenPts[i+1]) < hoverThresholdSq) {
                    hovered = true;
                    break;
                }
            }
        }

        // Color: Control vs Data distinction
        ImU32 baseCol = (wire.type == WireType::Control) ? Pal::ControlBase : Pal::DataBase;
        ImU32 col; float thick;
        if (selected)                                                          { col = Pal::WireSelected; thick = 3.5f; }
        else if (hovered)                                                      { col = Pal::NodeHover;    thick = 3.0f; }
        else if ((std::string(wire.id) == "IF_Flush" || std::string(wire.id) == "EX_Flush") && active)
                                                                               { col = Pal::WireFlush;    thick = 2.5f; }
        else if (active)                                                       { col = Pal::WireActive;   thick = 2.0f; }
        else                                                                   { col = baseCol;           thick = 1.2f; }

        // Scale thickness by zoom
        thick *= zoom;

        // Draw segments
        if (screenPts.size() >= 2) {
            for (size_t i = 0; i + 1 < screenPts.size(); ++i)
                dl->AddLine(screenPts[i], screenPts[i+1], col, thick);
            for (size_t i = 1; i + 1 < screenPts.size(); ++i)
                dl->AddCircleFilled(screenPts[i], 3.0f * zoom, col);
        }

        // Signal Name Label on longest segment
        if (wire.signalName && wire.signalName[0] != '\0' && screenPts.size() >= 2) {
            size_t bestSeg = 0;
            float bestLen = 0.0f;
            for (size_t i = 0; i + 1 < screenPts.size(); ++i) {
                float len = SegmentLenSq(screenPts[i], screenPts[i+1]);
                if (len > bestLen) { bestLen = len; bestSeg = i; }
            }

            ImVec2 a = screenPts[bestSeg], b = screenPts[bestSeg + 1];
            ImVec2 mid = ImVec2((a.x+b.x)*0.5f, (a.y+b.y)*0.5f);
            float labelFontSize = baseFontSize * 0.75f * zoom;
            ImVec2 labelSize = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, wire.signalName);

            bool isVertical = std::abs(b.x - a.x) < std::abs(b.y - a.y);
            float offX = isVertical ? 4.0f * zoom : -labelSize.x * 0.5f;
            float offY = isVertical ? -labelSize.y * 0.5f : -labelSize.y - 3.0f * zoom;
            ImVec2 labelPos = ImVec2(mid.x + offX, mid.y + offY);

            float px = 2.0f * zoom, py = 1.0f * zoom;
            dl->AddRectFilled(ImVec2(labelPos.x-px, labelPos.y-py),
                              ImVec2(labelPos.x+labelSize.x+px, labelPos.y+labelSize.y+py),
                              Pal::WireLabelBg, 2.0f * zoom);
            dl->AddText(font, labelFontSize, labelPos, Pal::WireLabelText, wire.signalName);

            // Bit-width indicator
            if (wire.bitWidth > 0) {
                float slashX = isVertical ? mid.x : mid.x + labelSize.x * 0.5f + 8.0f * zoom;
                float slashY = isVertical ? mid.y - 8.0f * zoom : mid.y;
                float slashLen = 5.0f * zoom;
                dl->AddLine(ImVec2(slashX-slashLen, slashY+slashLen),
                            ImVec2(slashX+slashLen, slashY-slashLen), col, 1.5f * zoom);
                char bwBuf[8]; snprintf(bwBuf, sizeof(bwBuf), "%d", wire.bitWidth);
                float bwFontSize = baseFontSize * 0.65f * zoom;
                dl->AddText(font, bwFontSize,
                    ImVec2(slashX + slashLen + 1.0f, slashY - slashLen - 2.0f * zoom),
                    Pal::BitWidthText, bwBuf);
            }
        }

        // Click detection
        if (hovered && ImGui::IsMouseClicked(0)) {
            g_schematicSelection.selectedNodeId.clear();
            g_schematicSelection.selectedWireId = wire.id;
        }
    }

    // ---- Phase 2: Nodes ----
    auto drawOneNode = [&](const SchematicNode& n) {
        ImVec2 nMin, nMax;
        nodeScreenRect(n, nMin, nMax);
        bool hovered = canvasHovered && (mouse.x >= nMin.x && mouse.x <= nMax.x && mouse.y >= nMin.y && mouse.y <= nMax.y);
        bool selected = (g_schematicSelection.selectedNodeId == n.id);

        DrawNodeShape(dl, n, origin, W, H, zoom, scroll, hovered, selected);
        DrawNodeData(dl, n, origin, W, H, zoom, scroll, cpu);

        if (hovered && ImGui::IsMouseClicked(0)) {
            g_schematicSelection.selectedWireId.clear();
            g_schematicSelection.selectedNodeId = n.id;
        }
    };

    for (const auto& n : kNodes) drawOneNode(n);
    drawOneNode(kFwdUnit);

    // ---- Phase 3: Stage Labels ----
    const char* stageLabels[] = { "IF", "ID", "EX", "MEM", "WB" };
    float stageX[] = { 0.12f, 0.32f, 0.58f, 0.78f, 0.93f };
    float stageFontSize = baseFontSize * 1.3f * zoom;
    for (int i = 0; i < 5; ++i) {
        ImVec2 pos = toScreen(ImVec2(stageX[i], 0.005f));
        dl->AddText(font, stageFontSize, pos, Pal::ControlActive, stageLabels[i]);
    }

    // ---- Zoom indicator (bottom-right) ----
    {
        char zoomBuf[16]; snprintf(zoomBuf, sizeof(zoomBuf), "%.0f%%", zoom * 100.0f);
        ImVec2 zts = ImGui::CalcTextSize(zoomBuf);
        ImVec2 zp = ImVec2(origin.x + W - zts.x - 8.0f, origin.y + H - zts.y - 4.0f);
        dl->AddText(zp, IM_COL32(120,125,140,180), zoomBuf);
    }

    dl->PopClipRect();
    ImGui::End();
}

void CleanupArchitectureWidget() {}

} // namespace MIPS::UI