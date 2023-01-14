//
// Created by gnilk on 14.01.23.
//

#include "Core/RuntimeConfig.h"

RuntimeConfig &RuntimeConfig::Instance() {
    static RuntimeConfig config;
    return config;
}
