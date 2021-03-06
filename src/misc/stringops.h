#ifndef LIBGEODECOMP_MISC_STRINGOPS_H
#define LIBGEODECOMP_MISC_STRINGOPS_H

#include <libgeodecomp/misc/stringvec.h>

#include <boost/algorithm/string.hpp>
#include <sstream>

// sad but true: CodeGear's C++ compiler has troubles with the +
// operator for strings.
#ifdef __CODEGEARC__
namespace LibGeoDecomp {
std::string operator+(const std::string& a, const std::string& b)
{
    return std::string(a).append(b);
}

}
#endif

namespace LibGeoDecomp {

/**
 * Common string operations which are mysterically missing in C++'s
 * standard library.
 */
class StringOps
{
public:

    static std::string itoa(int i)
    {
        std::stringstream buf;
        buf << i;
        return buf.str();
    }

    static int atoi(const std::string& s)
    {
        std::stringstream buf(s);
        int ret;
        buf >> ret;
        return ret;
    }

    static double atof(const std::string& s)
    {
        std::stringstream buf(s);
        double ret;
        buf >> ret;
        return ret;
    }

    static StringVec tokenize(const std::string& string, const std::string& delimiters)
    {
        StringVec ret;
        boost::split(ret, string, boost::is_any_of(delimiters), boost::token_compress_on);
        del(ret, "");

        return ret;
    }

    static std::string join(const StringVec& tokens, const std::string& delimiter)
    {
        std::stringstream buf;

        for (StringVec::const_iterator i = tokens.begin(); i != tokens.end(); ++i) {
            if (i != tokens.begin()) {
                buf << delimiter;
            }
            buf << *i;
        }

        return buf.str();
    }
};

}

#endif
