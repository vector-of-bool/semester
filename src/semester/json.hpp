#pragma once

#include <semester/data.hpp>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace semester {

/**
 * Base traits for JSON-style data
 */
template <typename Allocator>
struct json_traits_alloc {
    template <typename Data>
    struct traits {
        using allocator_type = Allocator;

        template <typename T>
        using rebind_alloc =
            typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

        using null_type   = semester::null_t;
        using bool_type   = bool;
        using number_type = double;
        using string_type = std::basic_string<char, std::char_traits<char>, rebind_alloc<char>>;

        using array_type   = std::vector<Data, rebind_alloc<Data>>;
        using mapping_type = std::map<string_type,  //
                                      Data,
                                      std::less<>,
                                      rebind_alloc<std::pair<const string_type, Data>>>;

        using variant_type = std::variant<  //
            null_type,                      //
            string_type,                    //
            number_type,                    //
            bool_type,                      //
            array_type,                     //
            mapping_type                    //
            >;

        static null_type   convert(decltype(nullptr)) { return null; }
        static bool_type   convert(bool b) { return b; }
        static number_type convert(number_type n) { return n; }
        static number_type convert(int n) { return n; }
        static string_type convert(const char* s) { return s; }
    };
};

struct json_traits : json_traits_alloc<std::allocator<void>> {};

/**
 * A structure that can contain JSON data
 */
using json_data = basic_data<json_traits>;

}  // namespace semester
