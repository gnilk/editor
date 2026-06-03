//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/AssetLoaderBase.h"
#include "Core/RuntimeConfig.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_assetloader(ITesting *t);
DLL_EXPORT int test_assetloader_load(ITesting *t);
DLL_EXPORT int test_assetloader_loadne(ITesting *t);
}

DLL_EXPORT int test_assetloader(ITesting *t) {
    return kTR_Pass;
}

// Load an asset - note, the asset comes from resources - this requires the 'Editor::Initialize' to have been called
// which it is, through 'test_main'
DLL_EXPORT int test_assetloader_load(ITesting *t) {
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto asset = assetLoader.LoadAsset("colors.json");
    TR_ASSERT(t, asset != nullptr);
    TR_ASSERT(t, asset->GetSize() != 0);
    return kTR_Pass;
}


// Load non-existing
DLL_EXPORT int test_assetloader_loadne(ITesting *t) {
    auto assetLoader = RuntimeConfig::Instance().GetAssetLoader();
    auto asset = assetLoader.LoadAsset("wefwef.wefwef");
    TR_ASSERT(t, asset == nullptr);
    return kTR_Pass;
}
