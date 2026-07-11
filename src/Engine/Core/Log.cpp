#include "Core/Log.h"
#include "Core/Paths.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <ctime>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace summer {

namespace {

std::string MakeTimestampedLogPath() {
    std::time_t now = std::time(nullptr);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    char stamp[32]{};
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &localTime);

    // Logs live next to the executable so standalone launches (RenderDoc/PIX) still write them.
    return Paths::Resolve(std::string("logs/summer_") + stamp + ".log");
}

} // namespace

void Log::Init() {
    try {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_pattern("[%T] [%^%l%$] %v");

        const std::string logPath = MakeTimestampedLogPath();
        // spdlog creates parent directories for the file automatically.
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true);
        fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");

        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        auto logger = std::make_shared<spdlog::logger>("summer", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::info);

        spdlog::set_default_logger(logger);
        spdlog::info("File logging to {}", logPath);
    } catch (const std::exception& ex) {
        // Never let logging setup take down the app -- fall back to console only.
        spdlog::set_pattern("[%T] [%^%l%$] %v");
        spdlog::set_level(spdlog::level::trace);
        spdlog::error("File logging init failed ({}); using console only", ex.what());
    }
}

} // namespace summer
