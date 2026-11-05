#ifndef AGILINK_OMNIHAND_CLIENT_CLIENT_TYPES_H
#define AGILINK_OMNIHAND_CLIENT_CLIENT_TYPES_H

#include <cstdint>
#include <stdexcept>
#include <string>

#include "omnihand_client/value.h"

namespace agilink {
namespace omnihand {
namespace client {

struct OmniHandClientOptions {
  std::string host = "127.0.0.1";
  std::uint16_t port = 8000;
  int timeout_ms = 10000;
};

class OmniHandClientError : public std::runtime_error {
 public:
  OmniHandClientError(int status_code, std::string message, Value response);

  int status_code() const noexcept;
  const Value& response() const noexcept;

 private:
  int status_code_ = 0;
  Value response_;
};

}  // namespace client
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_CLIENT_CLIENT_TYPES_H
