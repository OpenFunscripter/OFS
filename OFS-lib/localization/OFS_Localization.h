#pragma once
#include "OFS_StringsGenerated.h"
#include "OFS_Util.h"

class OFS_Translator
{
    public:

};

#define TR(str_id)\
OFS_DefaultStrings::Default[static_cast<uint32_t>(Tr::str_id)]

#define TRD(id)\
OFS_DefaultStrings::Default[static_cast<uint32_t>(id)]

#define TR_ID(id, str_id)\
FMT("%s###%s", OFS_DefaultStrings::Default[static_cast<uint32_t>(str_id)], id)
