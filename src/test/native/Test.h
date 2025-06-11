#pragma once

#include "ll/api/io/LogLevel.h"
#include "ll/api/io/LoggerRegistry.h"
#include "ll/api/utils/StringUtils.h"
#include "mc/util/ColorFormat.h"

namespace remote_call::test {

constexpr std::string TEST_EXPORT_NAMESPACE = "RemoteCallTest";
// constexpr auto SUCCESS_PREFIX =fmt::styled("",fmt::fg(fmt::terminal_color::green));
inline auto& getLogger() {
    static auto logger = [] {
        auto l = ll::io::LoggerRegistry::getInstance().getOrCreate(TEST_EXPORT_NAMESPACE);
        l->setLevel(ll::io::LogLevel::Debug);
        return l;
    }();
    return *logger;
}

inline void success(std::string_view msg) {
    getLogger().info(ll::string_utils::replaceMcToAnsiCode(fmt::format("{}{}", ColorFormat::GREEN, msg)));
}

inline void error(std::string_view msg) { getLogger().error(msg); }

} // namespace remote_call::test