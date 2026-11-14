#include "canalis/core/pipeline.hpp"

#include <stdexcept>
#include <string>

namespace canalis::core {

void Sink::write_raw(const std::vector<RawRecord>&) {
    throw std::logic_error("sink '" + std::string(name()) + "' accepts '" + std::string(accepts()) + "', not raw records");
}

void Sink::write_batch(const SinkBatch&) {
    throw std::logic_error("sink '" + std::string(name()) + "' accepts raw records, not envelopes");
}

} // namespace canalis::core
