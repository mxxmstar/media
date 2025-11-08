#include <string>
#include <algorithm>
#if __cplusplus >= 202002L
#define CPP_VERSION_AT_LEAST_20
#include <string_view>


#else

#endif


namespace StringHelper {
    inline std::string trim(const std::string& s) {
        auto is_space = [](unsigned char c){ return std::isspace(c); };
        auto start = std::find_if_not(s.begin(), s.end(), is_space);
        if (start == s.end()) return "";
        auto end = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
        return std::string(start, end);
    }
#ifdef CPP_VERSION_AT_LEAST_20
    inline std::string_view trim(std::string_view s) {
        auto is_space = [](unsigned char c){ return std::isspace(c); };
        auto start = std::find_if_not(s.begin(), s.end(), is_space);
        if (start == s.end()) return "";
        auto end = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
        return std::string_view(start, end);
    }

#endif // CPP_VERSION_AT_LEAST_20
    
}
