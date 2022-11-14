#pragma once
#include "bitsery/bitsery.h"
#include "bitsery/adapter/buffer.h"
#include "bitsery/traits/vector.h"
#include "bitsery/traits/array.h"
#include "bitsery/traits/string.h"
#include "bitsery/ext/growable.h"
#include "bitsery/ext/std_smart_ptr.h"
#include "bitsery/ext/value_range.h"

#include <vector>
#include <cstdint>

#include "OFS_Profiling.h"


#include "OFS_VectorSet.h"

namespace bitsery {
    namespace traits {
        // vector_set
        template<typename T, typename Allocator>
        struct ContainerTraits<vector_set<T, Allocator>>
        : public StdContainer<vector_set<T, Allocator>, true, true> {
        };

        template<typename T, typename Allocator>
        struct BufferAdapterTraits<vector_set<T, Allocator>>
        : public StdContainerForBufferAdapter<vector_set<T, Allocator>> {
        };
    }
}


using ByteBuffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<ByteBuffer>;
using InputAdapter = bitsery::InputBufferAdapter<ByteBuffer>;

using TContext = std::tuple<bitsery::ext::PointerLinkingContext>;

using ContextSerializer = bitsery::Serializer<OutputAdapter, TContext>;
using ContextDeserializer = bitsery::Deserializer<InputAdapter, TContext>;

struct OFS_Binary {

    template<typename T>
    static size_t Serialize(ByteBuffer& buffer, T& obj) noexcept
    {
        OFS_PROFILE(__FUNCTION__);
        TContext ctx{};

        ContextSerializer ser{ ctx, buffer };
        ser.object(obj);
        ser.adapter().flush();
        auto writtenSize = ser.adapter().writtenBytesCount();

        return writtenSize;
    }

    template<typename T>
    static auto Deserialize(ByteBuffer& buffer, T& obj) noexcept
    {
        OFS_PROFILE(__FUNCTION__);
        TContext ctx{};
        ContextDeserializer des{ ctx, buffer.begin(), buffer.size() };
        des.object(obj);

        auto error = des.adapter().error();
        auto valid = std::get<0>(ctx).isValid();
        std::get<0>(ctx).clearSharedState();

        return error;
    }
};

#include "imgui.h"

namespace bitsery {
    template<typename S>
    void serialize(S& s, ImVec2& o)
    {
        s.value4b(o.x);
        s.value4b(o.y);
    }

    template<typename S>
    void serialize(S& s, ImVec4& o)
    {
        s.value4b(o.x);
        s.value4b(o.y);
        s.value4b(o.z);
        s.value4b(o.w);
    }

    template<typename S>
    void serialize(S& s, ImColor& o)
    {
        s.object(o.Value);
    }

    template<typename S>
    void serialize(S& s, std::vector<float>& o)
    {
        s.container(o, std::numeric_limits<uint32_t>::max(),
            [](S& s, float& v) {
                s.value4b(v);
            });
    }
}