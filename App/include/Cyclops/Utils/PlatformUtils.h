#pragma once
#include <string>

namespace Cyclops {
    /**
     * @brief High-level access to native OS file dialogs.
     * Backed by NFD Extended (nativefiledialog-extended).
     */
    class FileDialogs {
    public:
        /**
         * @brief Opens a file picker.
         * @param filter Extension filter (e.g., "s,asm").
         */
        static std::string OpenFile(const char* filter);

        /**
         * @brief Opens a save file dialog.
         * @param filter Extension filter (e.g., "s").
         */
        static std::string SaveFile(const char* filter);

        /**
         * @brief Opens a folder selection dialog.
         */
        static std::string OpenFolder();
    };
}
