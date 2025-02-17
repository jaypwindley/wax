#pragma once

#include <cstddef>
#include <limits>

//
// Various atomic types we use frequently.
//

namespace wax {

// All indexes in Wax code are (or should be) size_t. This value represents an
// invalid index. Cf. std::string::npos.
//
static constexpr std::size_t npos = std::numeric_limits<std::size_t>::max();

} // namespace wax
