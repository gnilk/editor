//
// Language -> structure scanner
//

#include "StructureScanner.h"
#include "CPPStructureScanner.h"
#include "Core/Language/LanguageBase.h"

using namespace gedit;

StructureScanner::Ptr StructureScanner::CreateForLanguage(LanguageBase &language) {
    if (language.GetConfigNodeName() == "cpp") {
        return CPPStructureScanner::Create(language.Tokenizer());
    }
    return nullptr;
}
