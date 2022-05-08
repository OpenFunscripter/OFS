#include <cstdio>
#include <cstring>
#include <vector>
#include "rapidcsv.h"

/*
    This program generates the default strings (english) from a csv file.
*/

enum ColIdx : uint32_t
{
    Key,
    Default,
    Translation
};


constexpr const char* HeaderHead = R"(#pragma once
#include <cstdint>
#include <array>
#include <unordered_map>
#include <string>

enum class Tr : uint32_t
{
    )";

constexpr const char* HeaderFooter = R"(
};

struct OFS_DefaultStrings
{
    static std::array<const char*, static_cast<uint32_t>(Tr::MAX_STRING_COUNT)> Default;
    static std::unordered_map<std::string, Tr> KeyMapping;
};
)";


constexpr const char* SrcArrayHead = R"(#include "OFS_StringsGenerated.h"
std::array<const char*, static_cast<uint32_t>(Tr::MAX_STRING_COUNT)> OFS_DefaultStrings::Default =
{
    )";

constexpr const char* SrcArrayFooter = R"(
};
)";

constexpr const char* SrcMappingHead = R"(
std::unordered_map<std::string, Tr> OFS_DefaultStrings::KeyMapping =
{
)";
constexpr const char* SrcMappingFooter = R"(
};
)";

static void write_src_file(FILE* src, rapidcsv::Document& doc) noexcept
{
    // write the array
    fwrite(SrcArrayHead, 1, strlen(SrcArrayHead), src);
    auto row = doc.GetRow<std::string>(0);
    fwrite("R\"(", 1, sizeof("R\"(") - 1, src);
    fwrite(row[ColIdx::Default].data(), 1, row[ColIdx::Default].size(), src);
    fwrite(")\",\n\t", 1, sizeof(")\",\n\t") - 1, src);

    for(size_t i=1; i < doc.GetRowCount(); i += 1)
    {
        row = doc.GetRow<std::string>(i);
        fwrite("R\"(", 1, sizeof("R\"(") - 1, src);
        fwrite(row[ColIdx::Default].data(), 1, row[ColIdx::Default].size(), src);
        fwrite(")\",\n\t", 1, sizeof(")\",\n\t") - 1, src);
    }
    fwrite(SrcArrayFooter, 1, strlen(SrcArrayFooter), src);

    // write the key to enum hashmap
    fwrite(SrcMappingHead, 1, strlen(SrcMappingHead), src);
    row = doc.GetRow<std::string>(0);
    fwrite("\t{\"", 1, sizeof("\t{\"")-1, src);
    fwrite(row[0].data(), 1, row[0].size(), src);
    fwrite("\", Tr::", 1, sizeof("\", Tr::")-1, src);
    fwrite(row[0].data(), 1, row[0].size(), src);
    fwrite("},\n", 1, sizeof("},\n")-1, src);
    for(size_t i=1; i < doc.GetRowCount(); i += 1)
    {
        row = doc.GetRow<std::string>(i);
        fwrite("\t{\"", 1, sizeof("\t{\"")-1, src);
        fwrite(row[0].data(), 1, row[0].size(), src);
        fwrite("\", Tr::", 1, sizeof("\", Tr::")-1, src);
        fwrite(row[0].data(), 1, row[0].size(), src);
        fwrite("},\n", 1, sizeof("},\n")-1, src);
    }
    fwrite(SrcMappingFooter, 1, strlen(SrcMappingFooter), src);
    fclose(src);
}

static void write_header_enum(FILE* header, rapidcsv::Document& doc) noexcept
{
    fwrite(HeaderHead, 1, strlen(HeaderHead), header);

    auto row = doc.GetRow<std::string>(0);
    fwrite(row[ColIdx::Key].data(), 1, row[ColIdx::Key].size(), header);

    for(size_t i=1; i < doc.GetRowCount(); i += 1)
    {
        row = doc.GetRow<std::string>(i);
        fwrite(",\n\t", 1, sizeof(",\n\t")-1, header);
        fwrite(row[ColIdx::Key].data(), 1, row[ColIdx::Key].size(), header);
    }

    fwrite(",\n\t", 1, 3, header);
    fwrite("MAX_STRING_COUNT", 1, sizeof("MAX_STRING_COUNT")-1, header);

    fwrite(HeaderFooter, 1, strlen(HeaderFooter), header);
    fclose(header);
}

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("Please provide a csv file.\n");
        return -1;
    }

    auto csvFile = argv[1];
    auto header = fopen("OFS_StringsGenerated.h", "wb");
    auto src = fopen("OFS_StringsGenerated.cpp", "wb");
    if(header == nullptr || src == nullptr)
    {
        printf("Failed to create source files.\n");
        return -1;
    }

    try
    {
        rapidcsv::Document doc(csvFile, 
            rapidcsv::LabelParams(),
            rapidcsv::SeparatorParams(',', false, true, true),
            rapidcsv::ConverterParams(),
            rapidcsv::LineReaderParams()
        );
        
        auto cols = doc.GetColumnNames();
        if(cols.size() != 3) 
        {
            printf("Wrong amount of columns.\n");
            return -1;
        }
        write_header_enum(header, doc);
        write_src_file(src, doc);
    }
    catch(const std::exception& e)
    {
        printf("Error: %s\n", e.what());
    }

    return 0;
}