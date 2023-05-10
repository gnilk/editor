//
// Created by gnilk on 15.02.23.
//
#include <cstdio>
#include <cstdlib>
#include "BufferManager.h"

using namespace gedit;

// FIXME: This can be removed!
BufferManager::BufferManager() {
    logger = gnilk::Logger::GetLogger("BufferManager");

}

BufferManager &BufferManager::Instance() {
    static BufferManager glbBufferManager;
    return glbBufferManager;
}

// Create a new buffer with a given name
// Note: If loading files - supply the filename as the name..
TextBuffer::Ref BufferManager::NewBuffer(const std::string &name) {
    if (HaveBuffer(name)) {
        logger->Error("Trying to create buffer with same name");
        return nullptr;
    }
    auto textBuffer = std::make_shared<TextBuffer>(name);
    buffers[name] = textBuffer;
    return textBuffer;
}

//
// TODO: Rewrite this!
//
TextBuffer::Ref BufferManager::NewBufferFromFile(const std::string &filename) {
    printf("DON'T CALL THIS! 'BufferManager::NewBufferFromFile'\n");
    exit(1);
/*
    FILE *f = fopen(filename.c_str(), "r");
    if (f == nullptr) {
        logger->Error("Unable to open file '%s'", filename.c_str());
        return nullptr;
    }
    auto textBuffer = NewBuffer(filename);
    textBuffer->Rename(filename);

    char tmp[MAX_LINE_LENGTH];
    while(fgets(tmp, MAX_LINE_LENGTH, f)) {
        textBuffer->AddLine(tmp);
    }

    fclose(f);
    return textBuffer;
*/
}

bool BufferManager::HaveBuffer(const std::string &name) {
    return (buffers.find(name) != buffers.end());
}

TextBuffer::Ref BufferManager::GetBuffer(const std::string &name) {
    return buffers[name];
}


