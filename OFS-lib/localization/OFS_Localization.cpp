#include "OFS_Localization.h"
#include "OFS_Util.h"
#include "rapidcsv.h"

OFS_Translator* OFS_Translator::ptr = nullptr;

OFS_Translator::OFS_Translator() noexcept
{
    Util::CreateDirectories(Util::Prefpath(TranslationDir));
    // initialize with the default strings
    LoadDefaults();
}

void OFS_Translator::LoadDefaults() noexcept
{
    StringData = std::vector<char>();
    memcpy(Translation.data(), OFS_DefaultStrings::Default.data(), Translation.size() * sizeof(const char*));
}

bool OFS_Translator::LoadTranslation(const char* name) noexcept
{
    auto stdPath = Util::PathFromString(Util::Prefpath(TranslationDir)) / name;
    auto path = stdPath.u8string();

    if(!Util::FileExists(path))
    {
        LOG_ERROR("File doesn't exists");
        return false;
    }

    rapidcsv::Document doc(path, 
        rapidcsv::LabelParams(),
        rapidcsv::SeparatorParams(',', false, true, true),
        rapidcsv::ConverterParams(),
        rapidcsv::LineReaderParams()
    );
    std::vector<char> tmpData;
    tmpData.reserve(1024*15);

    std::vector<const char*> tmpTranslation;
    tmpTranslation.resize(OFS_DefaultStrings::Default.size());
    
    std::vector<int> tmpTrIndices;
    tmpTrIndices.resize(OFS_DefaultStrings::Default.size(), -1);

    auto row = doc.GetRow<std::string>(0);
    if(row.size() != 3)
    {        
        LOG_ERROR("Translation column count mismatch.");
        return false;
    }

    for(size_t i=1; i < doc.GetRowCount(); i += 1)
    {
        row = doc.GetRow<std::string>(i);
        if(row.size() != 3) {
            continue;
        }

        auto key = std::move(row[0]);
        auto value = std::move(row[2]);
        value = Util::trim(value);

        if(value.empty()) {
            continue;
        }

        auto it = OFS_DefaultStrings::KeyMapping.find(key);
        if(it != OFS_DefaultStrings::KeyMapping.end())
        {
            size_t index = static_cast<size_t>(it->second);
            size_t valPos = tmpData.size();
            tmpTrIndices[index] = valPos;

            for(auto c : value) {
                tmpData.emplace_back(c);
            }
            tmpData.emplace_back('\0');
        }
    }

    for(int i=0; i < OFS_DefaultStrings::Default.size(); i += 1)
    {
        if(tmpTrIndices[i] >= 0) {
            tmpTranslation[i] = &tmpData[tmpTrIndices[i]];
        }
        else 
        {
            tmpTranslation[i] = OFS_DefaultStrings::Default[i];
        }
    }

    StringData = std::move(tmpData);
    memcpy(Translation.data(), tmpTranslation.data(), Translation.size() * sizeof(const char*));
    return true;
}