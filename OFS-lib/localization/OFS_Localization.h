#pragma once
#include "OFS_StringsGenerated.h"
#include "OFS_Util.h"
#include <array>
#include <vector>

class OFS_Translator
{
    private:
    std::vector<char> StringData;
    
    public:
    static OFS_Translator* ptr;
    static constexpr const char* TranslationDir = "lang";

    OFS_Translator() noexcept;
    std::array<const char*, static_cast<uint32_t>(Tr::MAX_STRING_COUNT)> Translation;
    bool LoadTranslation(const char* name) noexcept;
    void LoadDefaults() noexcept;

    static bool MergeIntoOne(const char* input1, const char* input2, const char* outputPath) noexcept;

    static void Init() noexcept
    {
        if(ptr != nullptr) return;
        ptr = new OFS_Translator();
    }

    static void Shutdown() noexcept
    {
        if(ptr)
        {
            delete ptr;
            ptr = nullptr;
        }
    }
};

//#define TR(str_id)\
//OFS_DefaultStrings::Default[static_cast<uint32_t>(Tr::str_id)]
//#define TRD(id)\
//OFS_DefaultStrings::Default[static_cast<uint32_t>(id)]
//#define TR_ID(id, str_id)\
//FMT("%s###%s", OFS_DefaultStrings::Default[static_cast<uint32_t>(str_id)], id)

#define TR(str_id)\
OFS_Translator::ptr->Translation[static_cast<uint32_t>(Tr::str_id)]
#define TRD(id)\
OFS_Translator::ptr->Translation[static_cast<uint32_t>(id)]
#define TR_ID(id, str_id)\
FMT("%s###%s", OFS_Translator::ptr->Translation[static_cast<uint32_t>(str_id)], id)
