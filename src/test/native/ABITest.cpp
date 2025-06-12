#include "remote_call/api/ABI.h"

#include "Test.h"
#include "ll/api/Expected.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/mod/NativeMod.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/utils/ErrorUtils.h"
#include "remote_call/api/utils/ErrorUtils.h"


namespace remote_call::test {

auto testABI() {
    constexpr auto             ns   = TEST_EXPORT_NAMESPACE.substr(0, 10);
    constexpr std::string_view name = "abi";

    auto fn = [](auto&&) -> ll::Expected<DynamicValue> { return NULL_VALUE; };
    assert(!hasFunc(ns, name));
    auto res = exportFunc(ns, name, fn, true);
    assert(hasFunc(ns, name));
    assert(getProvider(ns, name).lock() == ll::mod::NativeMod::current());
    assert(res.value().callable({})->is_null());
    auto res2 = exportFunc(ns, name, fn, true);
    assert(error_utils::RemoteCallError::isRemoteCallError(res2.error()));
    assert(removeFunc(ns, name));
    assert(!hasFunc(ns, name));
    for (auto i : std::views::iota(0, 10)) {
        exportFunc(ns, fmt::format("{}{}", name, i), fn, true);
    }
    assert(hasFunc(ns, fmt::format("{}{}", name, 5)));
    assert(
        removeFuncs({
            {ns, fmt::format("{}{}", name, 4)  },
            {ns, fmt::format("{}{}", name, 115)},
            {ns, fmt::format("{}{}", name, 5)  }
    })
        == 2
    );
    assert(!hasFunc(ns, fmt::format("{}{}", name, 5)));
    assert(hasFunc(ns, fmt::format("{}{}", name, 3)));
    assert(removeNameSpace(ns) == 8);
    assert(!hasFunc(ns, fmt::format("{}{}", name, 3)));
    return true;
}

[[maybe_unused]] static auto test = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
    [] {
        try {
            if (testABI()) {
                success("ABI Test passed");
            } else {
                error("ABI Test failed");
            }
        } catch (...) {
            ll::error_utils::printCurrentException(getLogger());
        }
    },
    ll::chrono::ticks{1}
);


} // namespace remote_call::test