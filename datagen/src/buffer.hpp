#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <iostream>

#include <lmdb.h>

template<class N>
struct is_vector { static constexpr bool value = false; };
template <class N, class A>
struct is_vector<std::vector<N, A>> { static constexpr bool value = true; };

template<class T>
inline constexpr bool is_vector_v = is_vector<T>::value;

class Buffer
{
    std::vector<uint8_t> m_data;
    size_t m_offset;
public:
    Buffer() = default;
    explicit Buffer(std::size_t reserve) : m_offset(0) { m_data.reserve(reserve); }

    inline std::size_t size() const noexcept { return m_data.size(); }
    inline const uint8_t* data() const noexcept { return m_data.data(); }

    template<typename T>
    void write(const T& value)
    {
        if constexpr(std::is_integral_v<T> || std::is_floating_point_v<T>)
        {
            m_data.insert(m_data.end(), sizeof(T), 0);
            auto* ptr = m_data.data() + m_offset;
            *(T*)ptr = value;
            m_offset += sizeof(T);
        }
        else if constexpr(is_vector_v<T>)
        {
            write(value.size());
            for (const auto& v : value)
                write(v);
        }
        else if constexpr(std::is_same_v<T, std::string>)
        {
            write((uint8_t)value.size());
            for (const auto& c : value)
                write(c);
        }
        else
        {
            throw std::runtime_error("Type not serializable!");
        }
    }

    inline operator MDB_val() const
    {
        return MDB_val{ size(), (void*)data() };
    }
};
