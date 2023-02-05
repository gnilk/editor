//
// Created by gnilk on 05.02.23.
//
#include "logger.h"
static void SetupLogger() {
    char *sinkArgv[]={"file","logfile.log"};
    auto fileSink = new gnilk::LogFileSink();
    gnilk::Logger::AddSink(fileSink, "fileSink", 2, sinkArgv);
    auto logger = gnilk::Logger::GetLogger("main");
}

int main(int argc, char **argv) {
    SetupLogger();
    if (gnilk::Logger::RemoveSink("console")) {
        printf("Console sink removed\n");
    }

    auto logger = gnilk::Logger::GetLogger("main");
    logger->Debug("Test");
    auto log2 = gnilk::Logger::GetLogger("apa");
    log2->Debug("Test2");
    return -1;
}
