#ifndef AGILINK_OMNIHAND_CLIENT_REST_CLIENT_H
#define AGILINK_OMNIHAND_CLIENT_REST_CLIENT_H

#include <memory>
#include <string>

#include "omnihand_client/client_types.h"

namespace agilink {
namespace omnihand {
namespace client {

// Synchronous REST client for a local or remote OmniHandServer. Network and
// JSON implementation details stay behind the Pimpl boundary.
class OmniHandRestClient {
 public:
  OmniHandRestClient();
  OmniHandRestClient(std::string host, std::uint16_t port = 8000,
                     int timeout_ms = 10000);
  explicit OmniHandRestClient(OmniHandClientOptions options);
  ~OmniHandRestClient();

  OmniHandRestClient(const OmniHandRestClient&) = delete;
  OmniHandRestClient& operator=(const OmniHandRestClient&) = delete;
  OmniHandRestClient(OmniHandRestClient&&) noexcept;
  OmniHandRestClient& operator=(OmniHandRestClient&&) noexcept;

  Value Health() const;
  Value VendorInfo() const;
  Value ListHands() const;
  Value ListProductTypes() const;
  Value ListConnectionTypes() const;
  Value ConnectionConfigs() const;
  Value MethodsForProduct(const std::string& product_type) const;

  Value CreateHand(const std::string& product_type,
                   const std::string& connection_type,
                   Value connection_config = Value::EmptyObject(),
                   const std::string& hand_side = "left") const;
  Value DescribeHand(int hand_id) const;
  Value RemoveHand(int hand_id) const;
  Value Methods(int hand_id) const;
  Value Call(int hand_id, const std::string& method,
             Value params = Value::EmptyObject()) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace client
}  // namespace omnihand
}  // namespace agilink

#endif  // AGILINK_OMNIHAND_CLIENT_REST_CLIENT_H
