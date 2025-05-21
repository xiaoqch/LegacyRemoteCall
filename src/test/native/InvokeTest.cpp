#include "Test.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/reflection/Reflection.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "remote_call/api/RemoteCall.h"


namespace remote_call::test {

struct CustomArg {
    uchar       Count;
    short       Damage;
    std::string Name;
    struct Tag {
        int Damage;
        struct Display {
            std::string Name;
        } display;
        struct Ench {
            short id;
            short lvl;
        };
        std::vector<Ench> ench;
        int               RepairCost;
    } tag;
    bool WasPickedUp;
};
static_assert(ll::reflection::Reflectable<CustomArg>);


using remote_call::exportAs;
using remote_call::importAs;

struct Str {
    std::string s;
    bool        operator==(std::string_view o) const { return s == o; }
};
struct Str2 {
    std::string                  s;
    std::unique_ptr<CompoundTag> ptr{}; // disable copy constructor
    bool                         operator==(std::string_view o) const { return s == o; }
};

static_assert(ll::reflection::Reflectable<Str>);
// NOLINTBEGIN: bugprone-assert-side-effect, performance-unnecessary-value-param

std::tuple<Str, Str, Str> testFunc5(Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; }
bool                      testInvocation() {
    constexpr auto& ns      = TEST_EXPORT_NAMESPACE;
    bool            result  = false;
    auto            success = [&] { return !(result = !result); };

    auto testFunc1 = [&]() { result = true; };
    exportAs(ns, "testFunc1", testFunc1);
    importAs<decltype(testFunc1)>(ns, "testFunc1")();
    assert(success());

    auto testFunc2 = [&](Str a1, Str const& a2, Str&& a3) { result = a1 == "a1" && a2 == "a2" && a3 == "a3"; };
    exportAs(ns, "testFunc2", testFunc2);
    importAs<decltype(testFunc2)>(ns, "testFunc2")(Str("a1"), Str("a2"), Str("a3"));
    assert(success());

    auto testFunc3 = [&](Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; };
    exportAs(ns, "testFunc3", std::move(testFunc3));
    auto tpl3 = *importAs<decltype(testFunc3)>(ns, "testFunc3")(Str("a1"), Str("a2"), Str("a3"));
    result = std::get<0>(tpl3) == "a1" && std::get<1>(tpl3) == "a2" && std::get<2>(tpl3) == "a3";
    assert(success());

    struct TestFunc4 {
        auto operator()(Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; };
    } testFunc4;
    exportAs(ns, "testFunc4", testFunc4);
    auto tpl4 = *importAs<TestFunc4>(ns, "testFunc4")(Str("a1"), Str("a2"), Str("a3"));
    result = std::get<0>(tpl4) == "a1" && std::get<1>(tpl4) == "a2" && std::get<2>(tpl4) == "a3";
    assert(success());

    exportAs(ns, "testFunc5", testFunc5);
    auto tpl5 = *importAs<decltype(testFunc5)>(ns, "testFunc5")(Str("a1"), Str("a2"), Str("a3"));
    result = std::get<0>(tpl5) == "a1" && std::get<1>(tpl5) == "a2" && std::get<2>(tpl5) == "a3";
    assert(success());

    auto res = importAs<decltype(testFunc5)>(ns, "testFunc6")(Str("a1"), Str("a2"), Str("a3"));
    ;
    assert(!res);

    auto testFunc7 = [&](Str2&& a1) -> Str2 { return std::forward<Str2>(a1); };
    exportAs(ns, "testFunc7", testFunc7);
    auto str7 = *importAs<decltype(testFunc7)>(ns, "testFunc7")(Str2("a1"));
    result    = str7 == "a1";
    assert(success());

    {
        Str  s{"abcdefghijklmnopqrstuvwxyz"};
        auto testFunc10 = [s]() mutable -> Str { return s; };
        exportAs(ns, "testFunc10", testFunc10);
    }
    ll::thread::ServerThreadExecutor::getDefault().executeAfter(
        [ns] {
            auto str = *importAs<Str()>(ns, "testFunc10")();
            assert(str == "abcdefghijklmnopqrstuvwxyz");
        },
        ll::chrono::ticks{20}
    );
    test::success("Invoke Test passed");
    return true;
}
// NOLINTEND

auto const testResult = testInvocation();

} // namespace remote_call::test
