#ifndef AGILINK_OMNIHAND_CLIENT_VALUE_H
#define AGILINK_OMNIHAND_CLIENT_VALUE_H

#include "omnihand_web/data_value.h"

namespace agilink {
namespace omnihand {
namespace client {

// JSON-compatible value used by the public client API. It intentionally uses
// only C++ standard-library types, so embedding applications do not inherit a
// public dependency on the server's internal JSON implementation.
using Value = web::DataValue;

}  // namespace client
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_CLIENT_VALUE_H
