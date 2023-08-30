//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_FOLDERMONITOR_H
#define GOATEDIT_FOLDERMONITOR_H

#include <string>
#include <vector>
#include <utility>
#include <functional>
#include <cstdint>
#include <filesystem>

namespace gedit {
    class FolderMonitor {
    public:
        enum kChangeFlags : uint8_t {
            kNone     = 0,
            kCreated  = 0x01,
            kModified = 0x02,
            kRemoved  = 0x04,
            kRenamed  = 0x08,
        };
        using EventDelegate = std::function<void(const std::string &globPattern, kChangeFlags eventFlags)>;

        class MonitorPoint {
        public:
            using Ref = std::shared_ptr<MonitorPoint>;
        public:
            MonitorPoint() = default;
            virtual ~MonitorPoint() = default;

            static Ref Create();
        private:
        };

    public:
        FolderMonitor() = default;
        virtual ~FolderMonitor() = default;


        MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler);





        virtual bool Start() {
            return false;
        }
        virtual bool Stop() {
            return false;
        }
        bool AddEventListener(const std::string &pathToMonitor, const std::vector<std::string> &excludePaths, EventDelegate handler);
        bool IsRunning() {
            return isRunning;
        }
    protected:
        virtual bool AddPath(const std::filesystem::path &path, const std::vector<std::string> &exclusions) {
            return false;
        }

        void DispatchEvent(const std::string &name, kChangeFlags eventFlags);
    protected:
        struct Listener {
            std::string pattern;
            EventDelegate handler;
        };
        bool isRunning = false;
        std::vector<Listener> eventListeners = {};
    };

    // Define | operator
    inline constexpr FolderMonitor::kChangeFlags operator|(FolderMonitor::kChangeFlags lhs, FolderMonitor::kChangeFlags rhs) {
        auto left = static_cast<std::underlying_type_t<FolderMonitor::kChangeFlags>>(lhs);
        auto right = static_cast<std::underlying_type_t<FolderMonitor::kChangeFlags>>(rhs);
        return static_cast<FolderMonitor::kChangeFlags>(left | right);
    }
    // Define !=
    inline constexpr FolderMonitor::kChangeFlags &operator|=(FolderMonitor::kChangeFlags &lhs, const FolderMonitor::kChangeFlags rhs) {
        lhs = lhs | rhs;
        return lhs;
    }

    // Define & operator
    inline constexpr bool operator&(FolderMonitor::kChangeFlags lhs, FolderMonitor::kChangeFlags rhs) {
        auto left = static_cast<std::underlying_type_t<FolderMonitor::kChangeFlags>>(lhs);
        auto right = static_cast<std::underlying_type_t<FolderMonitor::kChangeFlags>>(rhs);
        return static_cast<FolderMonitor::kChangeFlags>(left & right);

    }


}

#endif //GOATEDIT_FOLDERMONITOR_H
