#include "Cyclops/Utils/PlatformUtils.h"
#include <nfd.h>
#include <iostream>

namespace Cyclops {
    /**
     * @brief Implementation of cross-platform file picker logic.
     * NFD extended allows native performance for Windows, macOS, and Linux.
     */
    std::string FileDialogs::OpenFile(const char* filter) {
        nfdchar_t* outPath;
        nfdfilteritem_t filterItem[1] = { { "Supported Files", filter } };
        
        // Use the native OS picker
        // NFD initialization is handled globally in ImGuiApp::Initialize()
        nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 1, NULL);
        
        std::string path = "";
        if (result == NFD_OKAY) {
            path = outPath;
            NFD_FreePath(outPath);
        } else if (result == NFD_ERROR) {
            std::cerr << "NFD Error: " << NFD_GetError() << std::endl;
        }
        return path;
    }

    std::string FileDialogs::SaveFile(const char* filter) {
        nfdchar_t* outPath;
        nfdfilteritem_t filterItem[1] = { { "Project Files", filter } };
        nfdresult_t result = NFD_SaveDialog(&outPath, filterItem, 1, NULL, NULL);
        
        std::string path = "";
        if (result == NFD_OKAY) {
            path = outPath;
            NFD_FreePath(outPath);
        }
        return path;
    }

    std::string FileDialogs::OpenFolder() {
        nfdchar_t* outPath;
        nfdresult_t result = NFD_PickFolder(&outPath, NULL);
        
        std::string path = "";
        if (result == NFD_OKAY) {
            path = outPath;
            NFD_FreePath(outPath);
        }
        return path;
    }
}
