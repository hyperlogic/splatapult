/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <cassert>
#include <cstdint>
#include <functional>

class BinaryAttribute
{
public:
    enum class Type
    {
        Unknown,
        Char,
        UChar,
        Short,
        UShort,
        Int,
        UInt,
        Float,
        Double,
        NumTypes
    };

    BinaryAttribute() : type(Type::Unknown), size(0), offset(0) {}
    BinaryAttribute(Type typeIn, size_t offsetIn);

    template <typename T>
    const T* Get(const void* data) const
    {
        if (type == Type::Unknown)
        {
            return nullptr;
        }
        else
        {
            assert(size == sizeof(T));
            return reinterpret_cast<const T*>(static_cast<const uint8_t*>(data) + offset);
        }
    }

    template <typename T>
    T* Get(void* data)
    {
        if (type == Type::Unknown)
        {
            return nullptr;
        }
        else
        {
            assert(size == sizeof(T));
            return reinterpret_cast<T*>(static_cast<uint8_t*>(data) + offset);
        }
    }

    template <typename T>
    const T Read(const void* data) const
    {
        const T* ptr = Get<T>(data);
        return ptr ? *ptr : 0;
    }

    template <typename T>
    bool Write(void* data, const T& val)
    {
        T* ptr = Get<T>(data);
        if (ptr)
        {
            *ptr = val;
            return true;
        }
        else
        {
            return false;
        }
    }

    template<typename T>
    void ForEachMut(void* data, size_t stride, size_t count, const std::function<void(T*)>& cb)
    {
        assert(type != Type::Unknown);
        assert(data);
        uint8_t* ptr = (uint8_t*)data;
        for (size_t i = 0; i < count; i++)
        {
            cb((T*)(ptr + offset));
            ptr += stride;
        }
    }

    template<typename T>
    void ForEach(const void* data, size_t stride, size_t count, const std::function<void(const T*)>& cb) const
    {
        assert(type != Type::Unknown);
        assert(data);
        const uint8_t* ptr = (const uint8_t*)data;
        for (size_t i = 0; i < count; i++)
        {
            cb((const T*)(ptr + offset));
            ptr += stride;
        }
    }

    Type type;
    size_t size;
    size_t offset;
};
