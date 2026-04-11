#pragma once
#include <string>
#include <vector>
#include <deque>
#include <mutex>

namespace MIPS::Core {

class Logger {
public:
    enum class Channel { Emulation, Console };

    static Logger& Get() {
        static Logger instance;
        return instance;
    }

    void Log(Channel channel, const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& buffer = (channel == Channel::Emulation) ? m_emuLog : m_consoleLog;
        buffer.push_back(message);
        if (buffer.size() > 1000) {
            buffer.pop_front();
        }
    }

    void Clear(Channel channel) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (channel == Channel::Emulation) m_emuLog.clear();
        else m_consoleLog.clear();
    }

    std::vector<std::string> GetEntries(Channel channel) {
        std::lock_guard<std::mutex> lock(m_mutex);
        const auto& buffer = (channel == Channel::Emulation) ? m_emuLog : m_consoleLog;
        return std::vector<std::string>(buffer.begin(), buffer.end());
    }

private:
    Logger() = default;
    std::mutex m_mutex;
    std::deque<std::string> m_emuLog;
    std::deque<std::string> m_consoleLog;
};

} // namespace MIPS::Core
