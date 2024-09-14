#ifndef SOURCE_LOCATION_H_
#define SOURCE_LOCATION_H_

#if !defined(__has_include)
 #define __has_include(include) 0L
#endif

#if __cplusplus >= 201103L || _MSVC_LANG >= 201103L
#include <cstdint>
#define NOEXCEPT noexcept
#define CONSTEXPR constexpr
#else
#define NOEXCEPT
#define CONSTEXPR
#endif

#if !defined(__has_builtin)
#define __has_builtin(builtin) OL
#endif

class Source_Location
{
public:
#if __cplusplus >= 201103L || _MSVC_LANG >= 201103L
    using intType = std::uint_least32_t;
#else
    typedef int intType;
#endif
    const intType m_line;
    const intType m_column;
    const char* m_file_name;
    const char* m_function_name;

#if __has_builtin(__builtin_COLUMN) || (defined(_MSC_VER) && _MSC_VER > 1925)
    inline static CONSTEXPR Source_Location current(const intType& line=__builtin_LINE(),const intType& column=__builtin_COLUMN(),const char* file_name=__builtin_FILE(),const char* function_name=__builtin_FUNCTION()) NOEXCEPT
#elif defined(__GNUC__) and (__GNUC__ > 4 or (__GNUC__ == 4 and __GNUC_MINOR__ >= 8))
        inline static CONSTEXPR Source_Location current(const intType& line=__builtin_LINE(),const intType& column=0,const char* file_name=__builtin_FILE(),const char* function_name=__builtin_FUNCTION()) NOEXCEPT
#else
    inline static CONSTEXPR Source_Location current(const intType& line=0,const intType& column=0,const char* file_name="unsupported !",const char* function_name="unsupported !") NOEXCEPT
#endif
    {
        return Source_Location(line, column, file_name, function_name);
    }

    CONSTEXPR intType line() const NOEXCEPT
    {
        return m_line;
    }

    CONSTEXPR intType column() const NOEXCEPT
    {
        return m_column;
    }

    CONSTEXPR const char* file_name() const NOEXCEPT
    {
        return m_file_name;
    }

    CONSTEXPR const char* function_name() const NOEXCEPT
    {
        return m_function_name;
    }

private:
    explicit CONSTEXPR Source_Location(const intType& line=0,const intType& column=0,const char* file_name="unsupported !",const char* function_name="unsupported !") NOEXCEPT : m_line(line), m_column(column), m_file_name(file_name), m_function_name(function_name) {}
};

#undef NOEXCEPT
#undef CONSTEXPR

#endif //SOURCE_LOCATION_H_
