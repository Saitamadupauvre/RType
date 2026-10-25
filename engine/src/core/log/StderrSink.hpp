#pragma once

#include "engine/core/log/Sink.hpp"

namespace engine::core::log {

class StderrSink final : public Sink {
public:
    void write(const Record& record) override;
};

} // namespace engine::core::log
