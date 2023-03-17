//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_BUFFERMANAGER_H
#define EDITOR_BUFFERMANAGER_H

#include <map>
#include <string>
#include "Core/TextBuffer.h"
#include "logger.h"

namespace gedit {
    // This class handles open buffers in the editor
    class BufferManager {
    public:
        virtual ~BufferManager() = default;
        static BufferManager &Instance();

        TextBuffer::Ref NewBuffer(const std::string &name);
        bool HaveBuffer(const std::string &name);
        TextBuffer::Ref GetBuffer(const std::string &name);
        TextBuffer::Ref NewBufferFromFile(const std::string &filename);

        const std::map<std::string, TextBuffer::Ref> &GetBuffers() {
            return buffers;
        }

    private:
        BufferManager();
        gnilk::ILogger *logger = nullptr;
        std::map<std::string, TextBuffer::Ref> buffers;
    };
}


#endif //EDITOR_BUFFERMANAGER_H
