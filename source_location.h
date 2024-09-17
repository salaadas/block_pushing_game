#ifndef SOURCE_LOCATION_H_
#define SOURCE_LOCATION_H_

#if !defined(__has_include)
    #define __has_include(include) 0L
#endif

#define NOEXCEPT  noexcept
#define CONSTEXPR constexpr

#if !defined(__has_builtin)
    #define __has_builtin(builtin) OL
#endif

class Source_Location
{
public:
    // using intType = std::uint_least32_t;
    typedef int intType;
    const intType m_line;
    const intType m_column;
    const char* m_file_name;
    const char* m_function_name;

    inline static CONSTEXPR Source_Location current(const intType& line=__builtin_LINE(),const intType& column=0,const char* file_name=__builtin_FILE(),const char* function_name=__builtin_FUNCTION()) NOEXCEPT
    {
        return Source_Location(line, column, file_name, function_name);
    }

    CONSTEXPR intType line()              const NOEXCEPT { return m_line; }
    CONSTEXPR intType column()            const NOEXCEPT { return m_column; }
    CONSTEXPR const char* file_name()     const NOEXCEPT { return m_file_name; }
    CONSTEXPR const char* function_name() const NOEXCEPT { return m_function_name; }

private:
    explicit CONSTEXPR Source_Location(const intType& line=0,const intType& column=0,const char* file_name="unsupported !",const char* function_name="unsupported !") NOEXCEPT : m_line(line), m_column(column), m_file_name(file_name), m_function_name(function_name) {}
};

#undef NOEXCEPT
#undef CONSTEXPR

#endif //SOURCE_LOCATION_H_
