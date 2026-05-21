#pragma once
#include <string_view>
#include <string>
#include <array>
#include <cstdint>

namespace skCrypt
{
    // Compile-time string encryption using XOR
    template<typename T, std::size_t N>
    struct xor_string
    {
    private:
        static constexpr std::size_t key_size = 16;
        static constexpr std::array<uint8_t, key_size> key = {
            0x2A, 0x3F, 0x5E, 0x71, 0x88, 0x9D, 0xB2, 0xC7,
            0xD4, 0xE9, 0xFE, 0x13, 0x28, 0x3D, 0x52, 0x67
        };
        
        std::array<T, N> encrypted{};
        
        // Compile-time XOR encryption
        constexpr T encrypt_char(T c, std::size_t idx) const
        {
            return c ^ key[idx % key_size];
        }
        
    public:
        // Constructor for compile-time encryption
        template<std::size_t... I>
        constexpr xor_string(const char* str, std::index_sequence<I...>) 
            : encrypted{ encrypt_char(str[I], I)... } {}
        
        // Runtime decryption
        std::basic_string<T> decrypt() const
        {
            std::basic_string<T> result;
            result.reserve(N);
            
            for (std::size_t i = 0; i < N; ++i)
            {
                result += encrypted[i] ^ key[i % key_size];
            }
            
            return result;
        }
        
        // Get decrypted string (most convenient)
        std::string get() const
        {
            return decrypt();
        }
        
        // Conversion operator for automatic decryption
        operator std::string() const
        {
            return decrypt();
        }
        
        // Const char* conversion (temporary)
        operator const char*() const = delete; // Prevent memory leaks
        
        // Get decrypted char array (for compatibility)
        std::basic_string<T> str() const
        {
            return decrypt();
        }
    };
    
    // Helper function to create xor_string from string literal
    template<std::size_t N>
    constexpr auto make_xor_string(const char(&str)[N])
    {
        return xor_string<char, N - 1>(str, std::make_index_sequence<N - 1>{});
    }
}

// Macros for easy usage
#define _XOR(str) skCrypt::make_xor_string(str).get()
#define _XOR_STR(str) skCrypt::make_xor_string(str)

// Alternative syntax for better readability
#define XOR(str) _XOR(str)
#define CRYPT(str) _XOR(str)

// For JSON compatibility - returns std::string
#define JSON_KEY(str) _XOR(str)

// For Windows API calls
#define API_STR(str) _XOR(str)

// For process/module names
#define PROC_NAME(str) _XOR(str)
#define MOD_NAME(str) _XOR(str)

// For logging
#define LOG_STR(str) _XOR(str)
