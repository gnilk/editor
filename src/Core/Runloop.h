//
// Created by gnilk on 15.04.23.
//

#ifndef EDITOR_RUNLOOP_H
#define EDITOR_RUNLOOP_H

#include <functional>

#include "Core/KeypressAndActionHandler.h"
#include "Core/KeyMapping.h"
#include "Core/Views/ViewBase.h"
#include "Core/SafeQueue.h"

namespace gedit {
    class Runloop {
    public:
        struct MessageData {};  // Do pass data to your message you MUST inherit from this one..
        using MessageCallback = std::function<void(uint32_t msgIdentifier, const MessageData *data)>;
        using MessageCallbackNoData = std::function<void(uint32_t msgIdentifier)>;
    private:
        struct Message {
            Message(const uint32_t mid, const MessageCallbackNoData callback) :
                msgIdentifier(mid),
                handlerNoData(callback) {
            }
            Message(const uint32_t mid, std::unique_ptr<MessageData> data, const MessageCallback callback) :
                msgIdentifier(mid),
                msgData(std::move(data)),
                handler(callback) {
            }

            void Invoke() const {
                if (handler) {
                    handler(msgIdentifier, msgData.get());
                } else if (handlerNoData) {
                    handlerNoData(msgIdentifier);
                }
            }

            uint32_t msgIdentifier = {};
            std::unique_ptr<MessageData> msgData = {};
            MessageCallback handler = {};
            MessageCallbackNoData handlerNoData = {};
        };
    public:
        static void DefaultLoop();
        static void ShowModal(ViewBase *modal);
        static void StopRunLoop() {
            bQuit = true;
        }
        static bool IsRunning() {
            return isRunning;
        }

        template<typename T>
        static void PostMessage(uint32_t msgIdentifier, std::unique_ptr<T> msg, const MessageCallback handler) {
            auto msgData = std::make_unique<Message>(msgIdentifier, std::move(msg), handler);
            msgQueue.push(std::move(msgData));
        }

        static void PostMessage(uint32_t msgIdentifier, const MessageCallbackNoData handler) {
            auto msgData = std::make_unique<Message>(msgIdentifier, handler);
            msgQueue.push(std::move(msgData));
        }


        // Call with 'null' to disable
        static void SetKeypressAndActionHook(KeypressAndActionHandler *newHook);
        static void TestLoop();

        // TEMP?
        static void ProcessKeyPress(KeyPress keyPress);
    private:
        static bool DispatchToHandler(KeypressAndActionHandler &handler, KeyPress keyPress);
        static void InstallKeymapChangeNotification();
        static bool ProcessMessageQueue();
    private:
        static bool bQuit;
        static bool isRunning;
        static KeypressAndActionHandler *hookedActionHandler;
        static KeyMapping::Ref activeKeyMap;
        static SafeQueue<std::unique_ptr<Message> > msgQueue;

    };
}

#endif //EDITOR_RUNLOOP_H
