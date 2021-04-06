#pragma once

#include "bitsery/bitsery.h"
#include "bitsery/adapter/buffer.h"
#include "bitsery/traits/vector.h"
#include "bitsery/traits/array.h"
#include "bitsery/traits/string.h"
#include "bitsery/ext/growable.h"
#include "bitsery/ext/std_smart_ptr.h"

#include <vector>
#include <cstdint>

using ByteBuffer = std::vector<uint8_t>;
using OutputAdapter = bitsery::OutputBufferAdapter<ByteBuffer>;
using InputAdapter = bitsery::InputBufferAdapter<ByteBuffer>;

using TContext = std::tuple<bitsery::ext::PointerLinkingContext>;

//NOTE:
// RTTI can be customizable, if you can't use dynamic_cast and typeid, and have 'custom' solution
using ContextSerializer = bitsery::Serializer<OutputAdapter, TContext>;
using ContextDeserializer = bitsery::Deserializer<InputAdapter, TContext>;

struct OFS_Binary
{

    template<typename T>
    static size_t Serialize(ByteBuffer& buffer, T& obj) noexcept
    {
        TContext ctx{};
        //std::get<1>(ctx).registerBasesList<MySerializer>(MyPolymorphicClassesForRegistering{});
        //create writer and serialize
        ContextSerializer ser{ ctx, buffer };
        ser.object(obj);
        ser.adapter().flush();
        auto writtenSize = ser.adapter().writtenBytesCount();
        
        //auto written = bitsery::quickSerialization<OutputAdapter>(buffer, obj);
        //return written;
        return writtenSize;
    }

    template<typename T>
    static auto Deserialize(ByteBuffer& buffer, T& obj) noexcept
    {
        //auto state = bitsery::quickDeserialization<InputAdapter>({ buffer.begin(), buffer.size() }, obj);
        //return state;
        TContext ctx{};
        ContextDeserializer des{ ctx, buffer.begin(), buffer.size() };
        des.object(obj);
        
        auto error = des.adapter().error();
        auto valid = std::get<0>(ctx).isValid();
        std::get<0>(ctx).clearSharedState();

        return error;
    }
};