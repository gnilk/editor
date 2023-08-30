//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_FOLDERMONITOR_H
#define GOATEDIT_FOLDERMONITOR_H

#include <string>
#include <vector>
#include <mutex>
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
            explicit MonitorPoint(const std::string &path) : pathToMonitor(path) {

            }
            virtual ~MonitorPoint() = default;

            static Ref Create(const std::string &pathToMonitor) {
                return std::make_shared<MonitorPoint>(pathToMonitor);
            }

            virtual bool Start() {
                return false;
            }
            virtual bool Stop() {
                return false;
            }

            bool IsRunning() {
                return isRunning;
            }


            void SetEventHandler(EventDelegate newHandler) {
                std::lock_guard<std::mutex> guard(dataGuard);
                handler = newHandler;
            }
            virtual void SetExcludePaths(const std::vector<std::string> &newPathsToExclude) {
                std::lock_guard<std::mutex> guard(dataGuard);
                pathsToExclude = newPathsToExclude;
            }

        protected:
            MonitorPoint() = default;
            void DispatchEvent(const std::string &name, kChangeFlags eventFlags);
        protected:
            bool isRunning = false;
            std::mutex dataGuard;
            std::string pathToMonitor = {};
            std::vector<std::string> pathsToExclude = {};
            EventDelegate handler = {};
        };

    public:
        FolderMonitor() = default;
        virtual ~FolderMonitor() = default;

        virtual MonitorPoint::Ref CreateMonitorPoint(const std::filesystem::path &pathToMonitor, EventDelegate handler);
    protected:
        void AddMonitor(MonitorPoint::Ref monitor) {
            monitorPoints.push_back(monitor);
        }
    protected:
        std::vector<MonitorPoint::Ref> monitorPoints = {};
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
