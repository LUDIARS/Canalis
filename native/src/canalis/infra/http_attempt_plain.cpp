#include "canalis/infra/http_attempt_op.hpp"

namespace canalis::http::detail {

void start_plain_attempt(boost::asio::io_context& io, const ClientRequest& req, AttemptArgs args, Attempt& out) {
    std::make_shared<AttemptOp<false>>(io, req, std::move(args), out)->start();
}

} // namespace canalis::http::detail
