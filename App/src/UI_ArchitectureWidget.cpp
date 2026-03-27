// ---------------------------------------------------------------------------
// UI_ArchitectureWidget.cpp  —  Programmatic Vector MIPS Datapath (Early Branch)
// Infinite canvas with zoom/pan and dynamic Manhattan wire routing.
// ---------------------------------------------------------------------------

// Uncomment to enable the blueprint tracing overlay and live node editor.
#define DEV_MODE

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

#ifdef DEV_MODE
#include "TextureLoader.h"
#include <imgui_internal.h>
static GLuint s_blueprintTex = 0;
static int    s_blueprintW = 0, s_blueprintH = 0;
static bool   s_blueprintLoaded = false;
static float  s_blueprintAlpha  = 0.35f;
static bool   s_showGrid        = true;
static int    s_selectedNodeIdx  = 0;
static int    s_selectedWireIdx  = 0;
#endif

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
enum class Shape { Rect, Ellipse, Trapezoid, Latch, Capsule, AndGate, OrGate, ALU_Shape };
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
// Under DEV_MODE, kNodes is mutable so sliders can live-edit positions.
#ifdef DEV_MODE
static std::array<SchematicNode, 30> kNodes = {{
#else
static const std::array<SchematicNode, 30> kNodes = {{
#endif
    // --- IF Stage ---
    { "Const80000180", "80000180", 0.000f, 0.560f, 0.050f, 0.020f, Shape::Rect, nullptr },
    { "ExcMux",   "M\nu\nx", 0.085f, 0.520f, 0.020f, 0.115f, Shape::Capsule, nullptr },
    { "PC",       "PC",      0.115f, 0.540f, 0.020f, 0.070f, Shape::Rect,
      [](const CPU& c) { char b[20]; snprintf(b,sizeof(b),"0x%04X",c.getState().pc); return std::string(b); } },
    { "InstrMem", "Instruction\nMemory", 0.155f, 0.475f, 0.075f, 0.205f, Shape::Rect, nullptr },
    { "AddPC4",   "+4",      0.180f, 0.375f, 0.015f, 0.075f, Shape::ALU_Shape, nullptr },
    { "IF_ID",    "IF/ID",   0.245f, 0.380f, 0.020f, 0.465f, Shape::Latch, nullptr },

    // --- ID Stage ---
    { "HDU",      "Hazard\nDetection", 0.305f, 0.100f, 0.095f, 0.090f, Shape::Capsule, nullptr },
    { "Control",  "Control", 0.330f, 0.235f, 0.045f, 0.135f, Shape::Ellipse, nullptr },
    { "IDFlushOR","OR",      0.485f, 0.170f, 0.030f, 0.060f, Shape::OrGate, nullptr },
    { "CtrlMux",  "M\nu\nx", 0.495f, 0.250f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "AddBr",    "Add",     0.410f, 0.310f, 0.015f, 0.075f, Shape::ALU_Shape, nullptr },
    { "ShiftL2",  "Shift\nLeft 2", 0.370f, 0.405f, 0.040f, 0.055f, Shape::Ellipse, nullptr },
    { "Regs",     "Registers", 0.415f, 0.405f, 0.075f, 0.205f, Shape::Rect,
      [](const CPU& c) { if(!c.id_ex.valid) return std::string();
                          char b[40]; snprintf(b,sizeof(b),"Rs:%d Rt:%d",c.id_ex.rs,c.id_ex.rt); return std::string(b); } },
    { "EqCmp",    "=",       0.505f, 0.480f, 0.025f, 0.060f, Shape::Ellipse, nullptr },
    { "BranchAND","AND",     0.450f, 0.080f, 0.025f, 0.035f, Shape::AndGate, nullptr },
    { "SignExt",  "Sign-\nextend", 0.370f, 0.635f, 0.045f, 0.095f, Shape::Ellipse, nullptr },
    { "ID_EX",    "ID/EX",   0.550f, 0.235f, 0.020f, 0.610f, Shape::Latch, nullptr },

    // --- EX Stage ---
    { "CauseReg", "Cause",   0.625f, 0.320f, 0.040f, 0.025f, Shape::Rect, nullptr },
    { "EPCReg",   "EPC",     0.625f, 0.350f, 0.040f, 0.025f, Shape::Rect, nullptr },
    { "EXFlushMuxWB","M\nu\nx", 0.670f, 0.160f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "EXFlushMuxM", "M\nu\nx", 0.700f, 0.260f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "FwdMuxA",  "M\nu\nx", 0.635f, 0.405f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "FwdMuxB",  "M\nu\nx", 0.635f, 0.550f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "ALUSrcMux","M\nu\nx", 0.635f, 0.745f, 0.020f, 0.110f, Shape::Capsule, nullptr },
    { "ALU",      "ALU",     0.710f, 0.445f, 0.035f, 0.200f, Shape::ALU_Shape,
      [](const CPU& c) { if(!c.ex_mem.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"0x%X",c.ex_mem.aluResult); return std::string(b); } },
    { "RegDstMux","M\nu\nx", 0.565f, 0.680f, 0.025f, 0.085f, Shape::Capsule, nullptr },
    { "EX_MEM",   "EX/MEM",  0.765f, 0.285f, 0.025f, 0.560f, Shape::Latch, nullptr },

    // --- MEM Stage ---
    { "DataMem",  "Data\nMemory", 0.825f, 0.500f, 0.065f, 0.205f, Shape::Rect,
      [](const CPU& c) { if(!c.mem_wb.valid) return std::string();
                          char b[20]; snprintf(b,sizeof(b),"R:0x%X",c.mem_wb.readData); return std::string(b); } },
    { "MEM_WB",   "MEM/WB",  0.910f, 0.335f, 0.020f, 0.510f, Shape::Latch, nullptr },

    // --- WB Stage ---
    { "WBMux",    "M\nu\nx", 0.970f, 0.495f, 0.020f, 0.115f, Shape::Capsule, nullptr }
}};

#ifdef DEV_MODE
static SchematicNode kFwdUnit =
    { "FwdUnit", "Forwarding\nUnit", 0.690f, 0.850f, 0.075f, 0.080f, Shape::Rect, nullptr };
#else
static const SchematicNode kFwdUnit =
    { "FwdUnit", "Forwarding\nUnit", 0.690f, 0.850f, 0.075f, 0.080f, Shape::Rect, nullptr };
#endif

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

#ifdef DEV_MODE
static std::vector<SchematicWire> kWires = {
#else
static const std::vector<SchematicWire> kWires = {
#endif
    // =====================================================================
    // IF Stage
    // =====================================================================
    { "PC_out", "PC",
      A("PC", Right, 0), A("InstrMem", Left, 0.1f),
      RouteStyle::Z_VHV, 0.145f, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "PC_to_Add", "PC",
      A("PC", Top, 0.3f), A("AddPC4", Bottom, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "PC4_out", "PC+4",
      A("AddPC4", Right, 0), A("IF_ID", Left, -0.35f),
      RouteStyle::Z_VHV, 0.220f, {}, [](const CPU& c){ return true; }, nullptr, WireType::Data, 32 },
    { "InstrMem_out", "Instr",
      A("InstrMem", Right, 0.1f), A("IF_ID", Left, 0.08f),
      RouteStyle::Z_VHV, 0.238f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },

    // =====================================================================
    // ID Stage: Splitting the Instruction Bus
    // =====================================================================
    // Trunk channel for instruction split is X = 0.280f
    { "Instr_to_Rs", "Rs",
      A("IF_ID", Right, -0.18f), A("Regs", Left, -0.2f),
      RouteStyle::Z_VHV, 0.280f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 5 },
    { "Instr_to_Rt", "Rt",
      A("IF_ID", Right, -0.15f), A("Regs", Left, 0.2f),
      RouteStyle::Z_VHV, 0.280f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 5 },
    { "Instr_to_SignExt", "Imm",
      A("IF_ID", Right, 0.3f), A("SignExt", Left, 0),
      RouteStyle::Z_VHV, 0.280f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 16 },
    { "Instr_to_Control", "Opcode",
      A("IF_ID", Right, -0.3f), A("Control", Left, 0),
      RouteStyle::Z_VHV, 0.280f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Control, 6 },

    // To HDU (Rs & Rt) - Branches cleanly off the 0.280f trunk
    { "Instr_to_HDU_Rs", "Rs",
      A("IF_ID", Right, -0.18f), A("HDU", Bottom, -0.2f),
      RouteStyle::Custom, 0, { {0.280f, 0.5288f}, {0.280f, 0.220f}, {0.3335f, 0.220f} }, 
      [](const CPU& c){ return c.enableHazardDetection && c.if_id.valid; }, nullptr, WireType::Control, 5 },
    { "Instr_to_HDU_Rt", "Rt",
      A("IF_ID", Right, -0.15f), A("HDU", Bottom, 0.2f),
      RouteStyle::Custom, 0, { {0.280f, 0.5427f}, {0.280f, 0.205f}, {0.3715f, 0.205f} }, 
      [](const CPU& c){ return c.enableHazardDetection && c.if_id.valid; }, nullptr, WireType::Control, 5 },

    // =====================================================================
    // ID Stage: Early Branch Logic & Comparators
    // =====================================================================
    { "Reg1_to_EqCmp", "Rs Data",
      A("Regs", Right, -0.15f), A("EqCmp", Left, -0.2f),
      RouteStyle::Z_VHV, 0.498f, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "Reg2_to_EqCmp", "Rt Data",
      A("Regs", Right, 0.25f), A("EqCmp", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    
    { "EqCmp_to_AND", "Zero",
      A("EqCmp", Top, 0), A("BranchAND", Bottom, 0.2f),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Control, 1 },
    { "Branch_wire", "Branch",
      A("Control", Right, -0.2f), A("BranchAND", Left, 0),
      RouteStyle::Z_VHV, 0.410f, {}, [](const CPU& c){ return c.id_ex.branch; }, nullptr, WireType::Control, 1 },
      
    // PCSrc Loop (Aligned to Top Channel Y = 0.020)
    { "PCSrc_wire", "PCSrc",
      A("BranchAND", Top, -0.2f), A("ExcMux", Top, 0.2f),
      RouteStyle::Custom, 0, { {0.4575f, 0.020f}, {0.099f, 0.020f} },
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Control, 1 },
      
    // Branch Target Loop (Aligned to Under-Top Channel Y = 0.040, and Left Channel X = 0.040)
    { "BrTarget_wire", "Br Target",
      A("AddBr", Left, -0.2f), A("ExcMux", Left, -0.1f), 
      RouteStyle::Custom, 0, { {0.390f, 0.3325f}, {0.390f, 0.040f}, {0.040f, 0.040f}, {0.040f, 0.566f} },
      [](const CPU& c){ return c.id_ex.valid && c.id_ex.isBranchTaken; }, nullptr, WireType::Data, 32 },

    { "SignExt_to_SL2", "Imm",
      A("SignExt", Top, 0.2f), A("ShiftL2", Bottom, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "SL2_to_AddBr", "SL2",
      A("ShiftL2", Top, 0), A("AddBr", Bottom, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },
    { "PC4_to_AddBr", "PC+4",
      A("IF_ID", Right, -0.38f), A("AddBr", Top, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Data, 32 },

    // =====================================================================
    // Hazard Detection & Flushes
    // =====================================================================
    { "HDU_idex_rt", "ID/EX.Rt",
      A("ID_EX", Left, 0.38f), A("HDU", Right, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableHazardDetection && c.id_ex.valid; }, nullptr, WireType::Control, 5 },
    { "HDU_memread", "MemRead",
      A("ID_EX", Left, -0.32f), A("HDU", Right, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.enableHazardDetection && c.id_ex.valid && c.id_ex.memRead; }, nullptr, WireType::Control, 1 },
    { "HDU_stall", "Stall",
      A("HDU", Bottom, 0), A("IDFlushOR", Left, 0),
      RouteStyle::L_VH, 0, {}, [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },
    { "HDU_pc_disable", "PCWrite",
      A("HDU", Left, -0.2f), A("PC", Top, -0.2f),
      RouteStyle::Custom, 0, { {0.290f, 0.127f}, {0.290f, 0.060f}, {0.121f, 0.060f} },
      [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },

    { "ID_Flush_Out", "ID.Flush",
      A("IDFlushOR", Right, 0), A("CtrlMux", Top, 0),
      RouteStyle::Custom, 0, { {0.515f, 0.200f}, {0.530f, 0.200f}, {0.530f, 0.230f}, {0.505f, 0.230f}, {0.505f, 0.250f} },
      [](const CPU& c){ return (c.hazardFlags & 0x1) != 0; }, nullptr, WireType::Control, 1 },
    { "IF_Flush", "IF.Flush",
      A("BranchAND", Top, 0.2f), A("IF_ID", Top, 0),
      RouteStyle::U_Top, 0.025f, {}, [](const CPU& c){ return (c.hazardFlags & 0x2) != 0; }, nullptr, WireType::Control, 1 },
    { "EX_Flush", "EX.Flush",
      A("BranchAND", Top, 0.4f), A("EXFlushMuxWB", Top, 0),
      RouteStyle::U_Top, 0.030f, {}, [](const CPU& c){ return (c.hazardFlags & 0x4) != 0; }, nullptr, WireType::Control, 1 },
    { "EX_Flush_M", "EX.Flush",
      A("BranchAND", Top, 0.4f), A("EXFlushMuxM", Top, 0),
      RouteStyle::U_Top, 0.030f, {}, [](const CPU& c){ return (c.hazardFlags & 0x4) != 0; }, nullptr, WireType::Control, 1 },

    // =====================================================================
    // Control Signal Bundles
    // =====================================================================
    { "Ctrl_to_Mux", "Ctrl",
      A("Control", Right, 0.2f), A("CtrlMux", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.if_id.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlWB_to_IDEX", "WB",
      A("CtrlMux", Right, -0.3f), A("ID_EX", Left, -0.38f),
      RouteStyle::Z_VHV, 0.530f, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlM_to_IDEX", "M",
      A("CtrlMux", Right, -0.1f), A("ID_EX", Left, -0.32f),
      RouteStyle::Z_VHV, 0.530f, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
    { "CtrlEX_to_IDEX", "EX",
      A("CtrlMux", Right, 0.1f), A("ID_EX", Left, -0.26f),
      RouteStyle::Z_VHV, 0.530f, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Control, 0 },
      
    { "WB_IDEX_to_EXMEM", "WB",
      A("ID_EX", Right, -0.42f), A("EX_MEM", Left, -0.42f),
      RouteStyle::Custom, 0, { {0.590f, 0.2838f}, {0.590f, 0.215f}, {0.740f, 0.215f}, {0.740f, 0.3298f} }, 
      [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Control, 0 },
    { "M_IDEX_to_EXMEM", "M",
      A("ID_EX", Right, -0.37f), A("EX_MEM", Left, -0.37f),
      RouteStyle::Custom, 0, { {0.590f, 0.3143f}, {0.590f, 0.315f}, {0.740f, 0.315f}, {0.740f, 0.3578f} }, 
      [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Control, 0 },
      
    { "WB_EXMEM_to_MEMWB", "WB",
      A("EX_MEM", Right, -0.42f), A("MEM_WB", Left, -0.42f),
      RouteStyle::Z_VHV, 0.850f, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Control, 0 },
    { "M_to_DataMem", "M",
      A("EX_MEM", Right, -0.37f), A("DataMem", Top, -0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.ex_mem.valid && (c.ex_mem.memRead || c.ex_mem.memWrite); }, nullptr, WireType::Control, 0 },
    { "WB_to_WBMux", "WB",
      A("MEM_WB", Right, -0.42f), A("WBMux", Top, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Control, 0 },

    // =====================================================================
    // Data Paths (EX, MEM, WB)
    // =====================================================================
    { "RegData1_out", "RdData1",
      A("ID_EX", Right, -0.08f), A("FwdMuxA", Left, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "RegData2_out", "RdData2",
      A("ID_EX", Right, 0.06f), A("FwdMuxB", Left, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "SignExt_out", "Sign-Ext",
      A("SignExt", Right, 0), A("ID_EX", Left, 0.30f),
      RouteStyle::Z_VHV, 0.480f, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "SignExt_IDEX_out", "Imm",
      A("ID_EX", Right, 0.30f), A("ALUSrcMux", Bottom, 0),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid && c.id_ex.aluSrc; }, nullptr, WireType::Data, 32 },
      
    { "MuxA_to_ALU", "Op1",
      A("FwdMuxA", Right, 0), A("ALU", Left, -0.2f),
      RouteStyle::Z_VHV, 0.680f, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "MuxB_to_ALUSrc", "Op2",
      A("FwdMuxB", Right, 0), A("ALUSrcMux", Left, 0),
      RouteStyle::Custom, 0, { {0.665f, 0.605f}, {0.665f, 0.800f}, {0.635f, 0.800f} }, 
      [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
    { "ALUSrc_to_ALU", "ALU In",
      A("ALUSrcMux", Right, 0), A("ALU", Left, 0.2f),
      RouteStyle::L_HV, 0, {}, [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
      
    { "ALU_out", "ALU Res",
      A("ALU", Right, 0), A("EX_MEM", Left, 0.05f),
      RouteStyle::Z_VHV, 0.755f, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },
    { "RegData2_passthru", "Write Data",
      A("FwdMuxB", Right, 0.3f), A("EX_MEM", Left, 0.22f),
      RouteStyle::Custom, 0, { {0.665f, 0.638f}, {0.665f, 0.6882f}, {0.750f, 0.6882f} },
      [](const CPU& c){ return c.id_ex.valid; }, nullptr, WireType::Data, 32 },
      
    { "DataMem_out", "Read Data",
      A("DataMem", Right, 0.15f), A("MEM_WB", Left, 0.08f),
      RouteStyle::Z_VHV, 0.900f, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
    { "ALU_passthru", "ALU",
      A("EX_MEM", Right, 0.05f), A("MEM_WB", Left, -0.05f),
      RouteStyle::Z_VHV, 0.850f, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },
    { "MemData_to_WBMux", "Mem",
      A("MEM_WB", Right, 0.08f), A("WBMux", Left, 0.15f),
      RouteStyle::Z_VHV, 0.950f, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
    { "ALU_to_WBMux", "ALU",
      A("MEM_WB", Right, -0.05f), A("WBMux", Left, -0.15f),
      RouteStyle::Z_VHV, 0.950f, {}, [](const CPU& c){ return c.mem_wb.valid; }, nullptr, WireType::Data, 32 },
      
    // The Massive Bottom Feedback Loops (Matched to bottom node bounding boxes)
    { "WB_out", "WB Data",
      A("WBMux", Right, 0), A("Regs", Bottom, 0.2f),
      RouteStyle::Custom, 0, { {1.000f, 0.5525f}, {1.000f, 0.940f}, {0.4675f, 0.940f} },
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Data, 32 },
    { "RegWrite_wire", "RegWrite",
      A("MEM_WB", Right, -0.42f), A("Regs", Bottom, -0.2f),
      RouteStyle::Custom, 0, { {0.945f, 0.3758f}, {0.945f, 0.960f}, {0.4375f, 0.960f} },
      [](const CPU& c){ return c.mem_wb.valid && c.mem_wb.regWrite; }, nullptr, WireType::Control, 1 },
      
    // EX/MEM -> DataMem writes
    { "EXMEM_WriteData", "Wr Data",
      A("EX_MEM", Right, 0.22f), A("DataMem", Left, 0.15f),
      RouteStyle::Z_VHV, 0.805f, {}, [](const CPU& c){ return c.ex_mem.valid && c.ex_mem.memWrite; }, nullptr, WireType::Data, 32 },
    { "EXMEM_Addr", "Addr",
      A("EX_MEM", Right, 0.05f), A("DataMem", Left, -0.1f),
      RouteStyle::Z_VHV, 0.805f, {}, [](const CPU& c){ return c.ex_mem.valid; }, nullptr, WireType::Data, 32 },

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
      RouteStyle::Custom, 0, { {0.800f, 0.761f}, {0.800f, 0.874f}, {0.765f, 0.874f} }, 
      [](const CPU& c){ return c.enableForwarding && c.ex_mem.valid && c.ex_mem.regWrite && c.ex_mem.destReg != 0; }, nullptr, WireType::Control, 5 },
    { "Fwd_MEM_WB", "MEM/WB.Rd",
      A("MEM_WB", Right, 0.35f), A("FwdUnit", Right, 0.2f),
      RouteStyle::Custom, 0, { {0.940f, 0.7685f}, {0.940f, 0.906f}, {0.780f, 0.906f} },
      [](const CPU& c){ return c.enableForwarding && c.mem_wb.valid && c.mem_wb.regWrite && c.mem_wb.destReg != 0; }, nullptr, WireType::Control, 5 },
      
    { "FwdUnit_to_MuxA", "FwdA",
      A("FwdUnit", Top, -0.2f), A("FwdMuxA", Bottom, 0),
      RouteStyle::Custom, 0, { {0.7125f, 0.830f}, {0.645f, 0.830f} },
      [](const CPU& c){ return c.forwardA != 0; }, nullptr, WireType::Control, 0 },
    { "FwdUnit_to_MuxB", "FwdB",
      A("FwdUnit", Top, 0.2f), A("FwdMuxB", Bottom, 0),
      RouteStyle::Custom, 0, { {0.7425f, 0.810f}, {0.645f, 0.810f} },
      [](const CPU& c){ return c.forwardB != 0; }, nullptr, WireType::Control, 0 },
      
    { "Fwd_EXMEM_data", "EX Fwd",
      A("EX_MEM", Right, 0.05f), A("FwdMuxA", Right, 0),
      RouteStyle::Custom, 0, { {0.805f, 0.593f}, {0.805f, 0.350f}, {0.665f, 0.350f}, {0.665f, 0.460f} },
      [](const CPU& c){ return c.forwardA == 2 || c.forwardB == 2; }, nullptr, WireType::Data, 32 },
    { "Fwd_MEMWB_data", "MEM Fwd",
      A("MEM_WB", Right, -0.05f), A("FwdMuxB", Right, 0),
      RouteStyle::Custom, 0, { {0.945f, 0.5645f}, {0.945f, 0.700f}, {0.665f, 0.700f}, {0.665f, 0.605f} },
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
    case Shape::Capsule: {
        float rounding = (x1 - x0) * 0.5f; 
        dl->AddRectFilled(ImVec2(x0,y0), ImVec2(x1,y1), fill, rounding);
        dl->AddRect(ImVec2(x0,y0), ImVec2(x1,y1), border, rounding, 0, thick);
        break;
    }
    case Shape::ALU_Shape: {
        float notchDepth = (x1 - x0) * 0.25f;
        float notchY1 = y0 + (y1 - y0) * 0.35f;
        float notchY2 = y0 + (y1 - y0) * 0.65f;
        ImVec2 pts[6] = {
            ImVec2(x0, y0),                            
            ImVec2(x1, y0 + (y1 - y0) * 0.2f),         
            ImVec2(x1, y1 - (y1 - y0) * 0.2f),         
            ImVec2(x0, y1),                            
            ImVec2(x0 + notchDepth, notchY2),          
            ImVec2(x0 + notchDepth, notchY1)           
        };
        dl->AddConvexPolyFilled(pts, 6, fill);
        dl->AddPolyline(pts, 6, border, ImDrawFlags_Closed, thick);
        break;
    }
    case Shape::AndGate: {
        float cx = x0 + (x1 - x0) * 0.5f;
        float cy = y0 + (y1 - y0) * 0.5f;
        float radius = (y1 - y0) * 0.5f;
        
        dl->PathClear();
        dl->PathLineTo(ImVec2(cx, y0));
        dl->PathArcTo(ImVec2(cx, cy), radius, -IM_PI/2.0f, IM_PI/2.0f);
        dl->PathLineTo(ImVec2(x0, y1));
        dl->PathLineTo(ImVec2(x0, y0));
        dl->PathFillConvex(fill);
        
        dl->PathClear();
        dl->PathLineTo(ImVec2(cx, y0));
        dl->PathArcTo(ImVec2(cx, cy), radius, -IM_PI/2.0f, IM_PI/2.0f);
        dl->PathLineTo(ImVec2(x0, y1));
        dl->PathLineTo(ImVec2(x0, y0));
        dl->PathStroke(border, ImDrawFlags_Closed, thick);
        break;
    }
    case Shape::OrGate: {
        dl->PathClear();
        dl->PathLineTo(ImVec2(x0, y0));
        dl->PathBezierCubicCurveTo(ImVec2(x0 + (x1-x0)*0.5f, y0), ImVec2(x1, y0 + (y1-y0)*0.2f), ImVec2(x1, y0 + (y1-y0)*0.5f));
        dl->PathBezierCubicCurveTo(ImVec2(x1, y1 - (y1-y0)*0.2f), ImVec2(x0 + (x1-x0)*0.5f, y1), ImVec2(x0, y1));
        dl->PathBezierCubicCurveTo(ImVec2(x0 + (x1-x0)*0.2f, y1 - (y1-y0)*0.2f), ImVec2(x0 + (x1-x0)*0.2f, y0 + (y1-y0)*0.2f), ImVec2(x0, y0));
        dl->PathFillConvex(fill);
        
        dl->PathClear();
        dl->PathLineTo(ImVec2(x0, y0));
        dl->PathBezierCubicCurveTo(ImVec2(x0 + (x1-x0)*0.5f, y0), ImVec2(x1, y0 + (y1-y0)*0.2f), ImVec2(x1, y0 + (y1-y0)*0.5f));
        dl->PathBezierCubicCurveTo(ImVec2(x1, y1 - (y1-y0)*0.2f), ImVec2(x0 + (x1-x0)*0.5f, y1), ImVec2(x0, y1));
        dl->PathBezierCubicCurveTo(ImVec2(x0 + (x1-x0)*0.2f, y1 - (y1-y0)*0.2f), ImVec2(x0 + (x1-x0)*0.2f, y0 + (y1-y0)*0.2f), ImVec2(x0, y0));
        dl->PathStroke(border, ImDrawFlags_Closed, thick);
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

#ifdef DEV_MODE
    // ---- Blueprint Background Image ----
    if (!s_blueprintLoaded) {
        s_blueprintTex = LoadTextureFromFile("assets/datapath_pipeline.jpg", &s_blueprintW, &s_blueprintH);
        s_blueprintLoaded = true;
        if (s_blueprintTex == 0)
            fprintf(stderr, "[DEV_MODE] Failed to load blueprint image.\n");
    }
    if (s_blueprintTex != 0) {
        ImVec2 imgMin = toScreen(ImVec2(0.0f, 0.0f));
        ImVec2 imgMax = toScreen(ImVec2(1.0f, 1.0f));
        ImU32 tint = IM_COL32(255, 255, 255, (int)(s_blueprintAlpha * 255));
        dl->AddImage((ImTextureID)(intptr_t)s_blueprintTex, imgMin, imgMax,
                     ImVec2(0,0), ImVec2(1,1), tint);
    }

    // ---- Dev Grid (2% increments) ----
    if (s_showGrid) {
        ImU32 gridCol = IM_COL32(60, 65, 80, 40);
        ImU32 gridCol5 = IM_COL32(80, 90, 110, 60); // every 10% slightly brighter
        float step = 0.02f;
        for (float u = 0.0f; u <= 1.001f; u += step) {
            bool isMajor = (std::fmod(u + 0.001f, 0.10f) < step);
            ImU32 c = isMajor ? gridCol5 : gridCol;
            float th = isMajor ? 1.0f : 0.5f;
            // Vertical line
            ImVec2 top = toScreen(ImVec2(u, 0.0f));
            ImVec2 bot = toScreen(ImVec2(u, 1.0f));
            dl->AddLine(top, bot, c, th * zoom);
            // Horizontal line
            ImVec2 left = toScreen(ImVec2(0.0f, u));
            ImVec2 right = toScreen(ImVec2(1.0f, u));
            dl->AddLine(left, right, c, th * zoom);
        }
        // Grid coordinate labels (every 10%)
        ImFont* gFont = ImGui::GetFont();
        float gFontSize = ImGui::GetFontSize() * 0.6f * zoom;
        for (float u = 0.0f; u <= 1.001f; u += 0.10f) {
            char buf[8]; snprintf(buf, sizeof(buf), "%.1f", u);
            ImVec2 pos = toScreen(ImVec2(u, 0.0f));
            dl->AddText(gFont, gFontSize, ImVec2(pos.x + 2, pos.y + 2), IM_COL32(100,105,120,120), buf);
        }
    }
#endif

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
        if (selected)                                                              { col = Pal::WireSelected; thick = 3.5f; }
        else if (hovered)                                                          { col = Pal::NodeHover;    thick = 3.0f; }
        else if ((std::string(wire.id) == "IF_Flush" || std::string(wire.id) == "EX_Flush" || std::string(wire.id) == "EX_Flush_M") && active)
                                                                                   { col = Pal::WireFlush;    thick = 2.5f; }
        else if (active)                                                           { col = Pal::WireActive;   thick = 2.0f; }
        else                                                                       { col = baseCol;           thick = 1.2f; }

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

#ifdef DEV_MODE
    // ---- Dev Mode: Live Cursor Coordinates Tooltip ----
    float mouseU = (mouse.x - origin.x - scroll.x) / (W * zoom);
    float mouseV = (mouse.y - origin.y - scroll.y) / (H * zoom);

    if (canvasHovered && mouseU >= 0.0f && mouseU <= 1.0f && mouseV >= 0.0f && mouseV <= 1.0f) {
        float snappedU = std::round(mouseU * 200.0f) / 200.0f;
        float snappedV = std::round(mouseV * 200.0f) / 200.0f;

        ImGui::BeginTooltip();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Raw:  (%.4f, %.4f)", mouseU, mouseV);
        ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.4f, 1.0f), "Snap: (%.3f, %.3f)", snappedU, snappedV);
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "[Press F to add Waypoint]");
        ImGui::EndTooltip();

        // F-key: add snapped coordinate as a Custom waypoint to the selected wire
        if (ImGui::IsKeyPressed(ImGuiKey_F)) {
            if (s_selectedWireIdx >= 0 && s_selectedWireIdx < (int)kWires.size()) {
                auto& w = kWires[s_selectedWireIdx];
                if (w.route == RouteStyle::Custom) {
                    w.customWaypoints.push_back(ImVec2(snappedU, snappedV));
                }
            }
        }
    }
#endif

    dl->PopClipRect();
    ImGui::End();

#ifdef DEV_MODE
    // ---- Dev Mode: Node Editor Panel ----
    ImGui::Begin("Dev Mode: Node Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::TextColored(ImVec4(1,0.85f,0.4f,1), "Blueprint Overlay");
    ImGui::SliderFloat("Opacity", &s_blueprintAlpha, 0.0f, 1.0f, "%.2f");
    ImGui::Checkbox("Show Grid", &s_showGrid);
    ImGui::Separator();

    // Build node name list (kNodes + FwdUnit)
    constexpr int totalNodes = 31; // 30 + FwdUnit
    const char* nodeNames[totalNodes];
    for (int i = 0; i < 30; ++i) nodeNames[i] = kNodes[i].id;
    nodeNames[30] = kFwdUnit.id;

    ImGui::TextColored(ImVec4(0.4f,0.85f,1,1), "Node Editor");
    ImGui::Combo("Node", &s_selectedNodeIdx, nodeNames, totalNodes);

    SchematicNode* editNode = (s_selectedNodeIdx < 30)
                              ? &kNodes[s_selectedNodeIdx]
                              : &kFwdUnit;

    bool changed = false;
    changed |= ImGui::DragFloat("X", &editNode->x, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("Y", &editNode->y, 0.005f, 0.0f, 1.0f, "%.3f");
    changed |= ImGui::DragFloat("W", &editNode->w, 0.005f, 0.001f, 0.5f, "%.3f");
    changed |= ImGui::DragFloat("H", &editNode->h, 0.005f, 0.001f, 0.5f, "%.3f");

    // ---- Live Shape Selector ----
    const char* shapeNames[] = { "Rect", "Ellipse", "Trapezoid", "Latch", "Capsule", "AndGate", "OrGate", "ALU_Shape" };
    int currentShape = static_cast<int>(editNode->shape);
    if (ImGui::Combo("Shape", &currentShape, shapeNames, IM_ARRAYSIZE(shapeNames))) {
        editNode->shape = static_cast<Shape>(currentShape);
    }

    ImGui::Separator();

    // Dump formatted C++ array to console
    if (ImGui::Button("Dump kNodes Array to Console")) {
        printf("// ---- Auto-generated kNodes (paste into UI_ArchitectureWidget.cpp) ----\n");
        printf("static const std::array<SchematicNode, 30> kNodes = {{\n");
        for (int i = 0; i < 30; ++i) {
            const auto& nd = kNodes[i];
            printf("    { \"%s\", \"%s\", %.3ff, %.3ff, %.3ff, %.3ff, Shape::%s, %s },\n",
                nd.id, nd.label,
                nd.x, nd.y, nd.w, nd.h,
                nd.shape == Shape::Rect ? "Rect" :
                nd.shape == Shape::Ellipse ? "Ellipse" :
                nd.shape == Shape::Trapezoid ? "Trapezoid" :
                nd.shape == Shape::Capsule ? "Capsule" :
                nd.shape == Shape::AndGate ? "AndGate" :
                nd.shape == Shape::OrGate ? "OrGate" :
                nd.shape == Shape::ALU_Shape ? "ALU_Shape" : "Latch",
                nd.dataFmt ? "/* dataFmt */" : "nullptr");
        }
        printf("}};\n\n");
        printf("static const SchematicNode kFwdUnit =\n");
        printf("    { \"%s\", \"%s\", %.3ff, %.3ff, %.3ff, %.3ff, Shape::Rect, nullptr };\n",
            kFwdUnit.id, kFwdUnit.label,
            kFwdUnit.x, kFwdUnit.y, kFwdUnit.w, kFwdUnit.h);
        printf("// ---- End auto-generated ----\n");
        fflush(stdout);
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy to Clipboard")) {
        std::string out;
        out += "static const std::array<SchematicNode, 30> kNodes = {{\n";
        for (int i = 0; i < 30; ++i) {
            const auto& nd = kNodes[i];
            char line[256];
            snprintf(line, sizeof(line),
                "    { \"%s\", \"%s\", %.3ff, %.3ff, %.3ff, %.3ff, Shape::%s, %s },\n",
                nd.id, nd.label, nd.x, nd.y, nd.w, nd.h,
                nd.shape == Shape::Rect ? "Rect" :
                nd.shape == Shape::Ellipse ? "Ellipse" :
                nd.shape == Shape::Trapezoid ? "Trapezoid" :
                nd.shape == Shape::Capsule ? "Capsule" :
                nd.shape == Shape::AndGate ? "AndGate" :
                nd.shape == Shape::OrGate ? "OrGate" :
                nd.shape == Shape::ALU_Shape ? "ALU_Shape" : "Latch",
                nd.dataFmt ? "/* dataFmt */" : "nullptr");
            out += line;
        }
        out += "}};\n";
        char fwd[256];
        snprintf(fwd, sizeof(fwd),
            "static const SchematicNode kFwdUnit = { \"%s\", \"%s\", %.3ff, %.3ff, %.3ff, %.3ff, Shape::Rect, nullptr };\n",
            kFwdUnit.id, kFwdUnit.label, kFwdUnit.x, kFwdUnit.y, kFwdUnit.w, kFwdUnit.h);
        out += fwd;
        ImGui::SetClipboardText(out.c_str());
    }

    // ================================================================
    // Wire Editor (below Node Editor in the same DEV panel)
    // ================================================================
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.3f, 1.0f), "Wire Editor");

    // Wire selector combo
    {
        int wireCount = (int)kWires.size();
        // Build names on the fly (small vector, fine for dev tool)
        std::vector<std::string> wireNameStorage(wireCount);
        std::vector<const char*> wireNames(wireCount);
        for (int i = 0; i < wireCount; ++i) {
            wireNameStorage[i] = std::string(kWires[i].id) + " (" + kWires[i].signalName + ")";
            wireNames[i] = wireNameStorage[i].c_str();
        }
        if (s_selectedWireIdx >= wireCount) s_selectedWireIdx = 0;
        ImGui::Combo("Wire", &s_selectedWireIdx, wireNames.data(), wireCount);
    }

    if (s_selectedWireIdx >= 0 && s_selectedWireIdx < (int)kWires.size()) {
        auto& ew = kWires[s_selectedWireIdx];

        // RouteStyle selector
        const char* routeNames[] = { "Direct", "L_HV", "L_VH", "Z_HVH", "Z_VHV", "U_Top", "U_Bottom", "Custom" };
        int curRoute = static_cast<int>(ew.route);
        if (ImGui::Combo("RouteStyle", &curRoute, routeNames, IM_ARRAYSIZE(routeNames))) {
            ew.route = static_cast<RouteStyle>(curRoute);
        }

        // Channel slider (only for styles that use it)
        if (ew.route == RouteStyle::Z_HVH || ew.route == RouteStyle::Z_VHV ||
            ew.route == RouteStyle::U_Top || ew.route == RouteStyle::U_Bottom) {
            ImGui::DragFloat("Channel", &ew.channel, 0.005f, -0.1f, 1.1f, "%.3f");
        }

        // Source/Dest anchor info (read-only display)
        ImGui::TextDisabled("Src: %s  Dst: %s", ew.source.nodeId, ew.dest.nodeId);

        // Source/Dest offset sliders
        ImGui::DragFloat("Src Offset", &ew.source.offset, 0.005f, -0.5f, 0.5f, "%.3f");
        ImGui::DragFloat("Dst Offset", &ew.dest.offset, 0.005f, -0.5f, 0.5f, "%.3f");

        // Custom Waypoints editor
        if (ew.route == RouteStyle::Custom) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 0.4f, 1.0f), "Custom Waypoints (%d):", (int)ew.customWaypoints.size());
            for (int wi = 0; wi < (int)ew.customWaypoints.size(); ++wi) {
                ImGui::PushID(wi);
                char label[32]; snprintf(label, sizeof(label), "WP %d", wi);
                ImGui::DragFloat2(label, &ew.customWaypoints[wi].x, 0.005f, -0.1f, 1.1f, "%.3f");
                ImGui::PopID();
            }
            if (!ew.customWaypoints.empty()) {
                if (ImGui::Button("Clear Last WP")) ew.customWaypoints.pop_back();
                ImGui::SameLine();
            }
            if (ImGui::Button("Clear All WPs")) ew.customWaypoints.clear();
            ImGui::TextDisabled("Hover canvas + press F to append waypoint");
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // ---- kWires Exporter ----
    if (ImGui::Button("Copy kWires to Clipboard")) {
        std::string out;
        out += "static const std::vector<SchematicWire> kWires = {\n";
        for (int i = 0; i < (int)kWires.size(); ++i) {
            const auto& w = kWires[i];
            // RouteStyle name
            const char* rsName = "Direct";
            switch (w.route) {
                case RouteStyle::Direct:   rsName = "Direct"; break;
                case RouteStyle::L_HV:     rsName = "L_HV"; break;
                case RouteStyle::L_VH:     rsName = "L_VH"; break;
                case RouteStyle::Z_HVH:    rsName = "Z_HVH"; break;
                case RouteStyle::Z_VHV:    rsName = "Z_VHV"; break;
                case RouteStyle::U_Top:    rsName = "U_Top"; break;
                case RouteStyle::U_Bottom: rsName = "U_Bottom"; break;
                case RouteStyle::Custom:   rsName = "Custom"; break;
            }
            // WireType name
            const char* wtName = (w.type == WireType::Control) ? "Control" : "Data";
            // Port names helper
            auto portStr = [](Port p) -> const char* {
                switch (p) {
                    case Port::Top:    return "Top";
                    case Port::Bottom: return "Bottom";
                    case Port::Left:   return "Left";
                    case Port::Right:  return "Right";
                    case Port::Center: return "Center";
                }
                return "Center";
            };

            char line[512];
            // Build customWaypoints string
            std::string wpStr = "{}";
            if (!w.customWaypoints.empty()) {
                wpStr = "{{ ";
                for (size_t k = 0; k < w.customWaypoints.size(); ++k) {
                    char wp[64];
                    snprintf(wp, sizeof(wp), "{%.3ff,%.3ff}", w.customWaypoints[k].x, w.customWaypoints[k].y);
                    if (k > 0) wpStr += ", ";
                    wpStr += wp;
                }
                wpStr += " }}";
            }

            snprintf(line, sizeof(line),
                "    { \"%s\", \"%s\",\n"
                "      A(\"%s\", %s, %.3ff), A(\"%s\", %s, %.3ff),\n"
                "      RouteStyle::%s, %.3ff, %s,\n"
                "      /* isActive LAMBDA */, nullptr, WireType::%s, %d },\n",
                w.id, w.signalName,
                w.source.nodeId, portStr(w.source.port), w.source.offset,
                w.dest.nodeId, portStr(w.dest.port), w.dest.offset,
                rsName, w.channel, wpStr.c_str(),
                wtName, w.bitWidth);
            out += line;
        }
        out += "};\n";
        ImGui::SetClipboardText(out.c_str());
    }

    ImGui::End();
#endif
}

void CleanupArchitectureWidget() {
#ifdef DEV_MODE
    if (s_blueprintTex != 0) {
        UnloadTexture(s_blueprintTex);
        s_blueprintTex = 0;
        s_blueprintLoaded = false;
    }
#endif
}

} // namespace MIPS::UI