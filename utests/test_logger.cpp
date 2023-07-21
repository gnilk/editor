//
// Created by gnilk on 21.07.23.
//
#include <testinterface.h>
#include "logger.h"


extern "C" {
DLL_EXPORT int test_logger(ITesting *t);
DLL_EXPORT int test_logger_debug(ITesting *t);
}

class MockSink : public gnilk::LogBaseSink {
public:
    MockSink() = default;
    virtual ~MockSink() = default;

    void Initialize(int argc, const char **argv) override { }
    int WriteLine(int dbgLevel, char *hdr, char *string) override;
    int GetCounter() { return counter; }
    void Close() override { }
private:
    int counter = 0;
};
int MockSink::WriteLine(int dbgLevel, char *hdr, char *string) {
    counter++;
    return counter;
}


DLL_EXPORT int test_logger(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_logger_debug(ITesting *t) {
    auto logger = gnilk::Logger::GetLogger("test");
    auto logger2 = gnilk::Logger::GetLogger("test2");
    MockSink sink;
    gnilk::Logger::AddSink(&sink, "mock");
    TR_ASSERT(t, sink.GetCounter() == 0);
    logger->Debug("wefwef");
    TR_ASSERT(t, sink.GetCounter() == 1);
    logger->SetEnabled(false);
    logger->Debug("wefwef");
    TR_ASSERT(t, sink.GetCounter() == 1);

    gnilk::Logger::DisableAllLoggers();
    logger->Debug("wefwef");
    logger2->Debug("wefwfe");
    TR_ASSERT(t, sink.GetCounter() == 1);

    gnilk::Logger::EnableLogger("test2");
    logger->Debug("wefwef");
    logger2->Debug("wefwfe");
    TR_ASSERT(t, sink.GetCounter() == 2);

    return kTR_Pass;
}
