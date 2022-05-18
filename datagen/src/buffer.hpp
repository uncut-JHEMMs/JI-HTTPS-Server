#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>

#include <lmdb.h>

template<class N>
struct is_vector { static constexpr bool value = false; };
template <class N, class A>
struct is_vector<std::vector<N, A>> { static constexpr bool value = true; };

template<class T>
inline constexpr bool is_vector_v = is_vector<T>::value;

template<class T, class = void>
struct has_serialize : std::false_type{};

template<class T>
struct has_serialize<T, typename std::void_t<decltype(std::declval<T>().serialize())>> : std::true_type {};

template<class T>
inline constexpr bool has_serialize_v = has_serialize<T>::value;

class Buffer
{
    std::vector<uint8_t> m_data;
    size_t m_offset;
public:
    Buffer() : m_offset(0), m_data() {};
    explicit Buffer(std::size_t reserve) : m_offset(0) { m_data.reserve(reserve); }

    inline std::size_t size() const noexcept { return m_data.size(); }
    inline const uint8_t* data() const noexcept { return m_data.data(); }

    template<typename T>
    void write(const T& value)
    {
        if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>)
        {
            write(&value, sizeof(T));
        }
        else if constexpr(is_vector_v<T>)
        {
            write((uint64_t)value.size());
            for (const auto& v : value)
                write(v);
        }
        else if constexpr(std::is_same_v<T, std::string>)
        {
            write((uint8_t)value.size());
            write(value.data(), value.size());
        }
        else if constexpr(has_serialize_v<T>)
        {
            auto buffer = value.serialize();
            write(buffer);
        }
        else if constexpr(std::is_same_v<T, Buffer>)
        {
            write(value.data(), value.size());
        }
        else
        {
            throw std::runtime_error("Type not serializable!");
        }
    }

    void write(const void* data, size_t size)
    {
        m_data.insert(m_data.end(), size, 0);
        memcpy(m_data.data() + m_offset, data, size);
        m_offset += size;
    }

    inline operator MDB_val() const
    {
        return MDB_val{ size(), (void*)data() };
    }
};
