#include "BoostMath.h"

#pragma push_macro("check")
#pragma push_macro("verify")
#undef check
#undef verify

THIRD_PARTY_INCLUDES_START
#define BOOST_DISABLE_ABI_HEADERS
#include <boost/math/special_functions/bessel.hpp>
#include <boost/crc.hpp>    // for boost::crc_32_type
THIRD_PARTY_INCLUDES_END

#pragma pop_macro("check")
#pragma pop_macro("verify")

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wmissing-braces"
#  pragma clang diagnostic ignored "-Wunusable-partial-specialization"
#  pragma clang diagnostic ignored "-Wundef"
#  pragma clang diagnostic ignored "-Wall"
#  pragma clang diagnostic ignored "-Wshadow"
#  pragma clang diagnostic ignored "-Wdeprecated-builtins"
#endif

namespace ueboost
{
float bessel_j(int j, float v)
{
    return boost::math::cyl_bessel_j(j, v);
}
unsigned int crc(const void* data, int len)
{
  boost::crc_32_type result;
  result.process_bytes(data, len);
  return result.checksum();
}
}    // namespace ueboost
