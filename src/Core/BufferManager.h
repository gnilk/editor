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
    class BufferManager {
    public:
        virtual ~BufferManager() = default;
        static BufferManager &Instance();

        TextBuffer *NewBuffer(const std::string &name);
        bool HaveBuffer(const std::string &name);
        TextBuffer *GetBuffer(const std::string &name);

        TextBuffer *NewBufferFromFile(const std::string &filename);

    private:
        BufferManager();
        gnilk::ILogger *logger = nullptr;
        std::map<std::string, TextBuffer *> buffers;
    };
}


#endif //EDITOR_BUFFERMANAGER_H
