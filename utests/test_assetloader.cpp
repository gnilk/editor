//
// Created by gnilk on 02.05.23.
//
#include <testinterface.h>
#include "Core/AssetLoaderBase.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_assetloader(ITesting *t);
DLL_EXPORT int test_assetloader_load(ITesting *t);
DLL_EXPORT int test_assetloader_loadne(ITesting *t);
DLL_EXPORT int test_assetloader_loadtext(ITesting *t);
}

DLL_EXPORT int test_assetloader(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_assetloader_load(ITesting *t) {
    AssetLoaderBase assetLoaderBase;
    auto asset = assetLoaderBase.LoadAsset("dummy.fil");
    TR_ASSERT(t, asset != nullptr);
    TR_ASSERT(t, asset->size != 0);
    return kTR_Pass;
}
DLL_EXPORT int test_assetloader_loadtext(ITesting *t) {
    AssetLoaderBase assetLoaderBase;
    auto asset = assetLoaderBase.LoadAsset("dummy.fil");
    TR_ASSERT(t, asset != nullptr);
    TR_ASSERT(t, asset->size != 0);
    return kTR_Pass;
}

DLL_EXPORT int test_assetloader_loadne(ITesting *t) {
    AssetLoaderBase assetLoaderBase;
    auto asset = assetLoaderBase.LoadAsset("wefwef.wefwef");
    TR_ASSERT(t, asset == nullptr);
    return kTR_Pass;
}
