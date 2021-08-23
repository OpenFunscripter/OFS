#pragma once
#include <limits>
#include "bitsery/bitsery.h"
#include "bitsery/adapter/buffer.h"
#include "bitsery/traits/vector.h"
#include "bitsery/traits/array.h"
#include "bitsery/traits/string.h"
#include "bitsery/ext/growable.h"
#include "bitsery/ext/std_smart_ptr.h"

#include <vector>
#include <cstdint>

#include "OFS_Profiling.h"

#include "EASTL/vector.h"
#include "EASTL/vector_set.h"
namespace bitsery {
    namespace traits {
        // eastl::vector
        template<typename T, typename Allocator>
        struct ContainerTraits<eastl::vector<T, Allocator>>
            :public StdContainer<eastl::vector<T, Allocator>, true, true> {};

        // this is true for std::vector but not for eastl::vector
        //bool vector is not contiguous, do not copy it directly to buffer
        //template<typename Allocator>
        //struct ContainerTraits<eastl::vector<bool, Allocator>>
        //    :public StdContainer<eastl::vector<bool, Allocator>, true, false> {};

        template<typename T, typename Allocator>
        struct BufferAdapterTraits<eastl::vector<T, Allocator>>
            :public StdContainerForBufferAdapter<eastl::vector<T, Allocator>> {};


        // eastl::vector_set
        template<typename T, typename Allocator>
        struct ContainerTraits<eastl::vector_set<T, Allocator>>
            :public StdContainer<eastl::vector_set<T, Allocator>, true, true> {};

        template<typename T, typename Allocator>
        struct BufferAdapterTraits<eastl::vector_set<T, Allocator>>
            :public StdContainerForBufferAdapter<eastl::vector_set<T, Allocator>> {};
    }
}



using ByteBuffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<ByteBuffer>;
using InputAdapter = bitsery::InputBufferAdapter<ByteBuffer>;

using TContext = std::tuple<bitsery::ext::PointerLinkingContext>;

using ContextSerializer = bitsery::Serializer<OutputAdapter, TContext>;
using ContextDeserializer = bitsery::Deserializer<InputAdapter, TContext>;

struct OFS_Binary
{

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

namespace bitsery
{
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
}