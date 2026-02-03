#pragma once
#include <spdlog/async.h>
#include <spdlog/spdlog.h>

namespace rmuduo {

#define LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)

}  // namespace rmuduo