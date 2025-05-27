#include "Test.h"
#include "ll/api/Expected.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/reflection/Reflection.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "mc/nbt/CompoundTag.h"
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

struct Root {
    std::string name;
    struct Node {
        struct Property {
            std::string name;
            struct Value {
                std::string name;
                BlockPos    pos{6, 66, 6};
            } value;
        } property;
    } node;
};

struct AnotherRoot {
    std::string name;
    struct Node {
        struct Property {
            std::string name;
            struct Value {
                std::string name;
                std::string pos{};
            } value;
        } property;
    } node;
};


static_assert(ll::reflection::Reflectable<Str>);
// NOLINTBEGIN: bugprone-assert-side-effect, performance-unnecessary-value-param

std::tuple<Str, Str, Str> testFunc(Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; }
bool                      testInvocation() {
    constexpr auto& ns      = TEST_EXPORT_NAMESPACE;
    bool            result  = false;
    auto            success = [&] { return !(result = !result); };
    size_t          index   = 0;

    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&]() { result = true; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        importAs<decltype(testFunc)>(ns, funcName)().transform_error(logAndAssert);
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto testFunc = [&](Str a1, Str const& a2, Str&& a3) { result = a1 == "a1" && a2 == "a2" && a3 == "a3"; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        importAs<decltype(testFunc)>(ns, funcName)(Str("a1"), Str("a2"), Str("a3")).transform_error(logAndAssert);
        assert(success());
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; };
        exportAs(ns, funcName, std::move(testFunc)).transform_error(logAndAssert);
        auto res =
            *importAs<decltype(testFunc)>(ns, funcName)(Str("a1"), Str("a2"), Str("a3")).transform_error(logAndAssert);
        result = std::get<0>(res) == "a1" && std::get<1>(res) == "a2" && std::get<2>(res) == "a3";
        assert(success());
    }
    {
        // operator()
        std::string funcName = fmt::format("testFunc{}", ++index);
        struct TestFunc {
            auto operator()(Str a1, Str const& a2, Str&& a3) { return std::tuple{a1, a2, a3}; };
        } testFunc;
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res = *importAs<TestFunc>(ns, funcName)(Str("a1"), Str("a2"), Str("a3")).transform_error(logAndAssert);
        result = std::get<0>(res) == "a1" && std::get<1>(res) == "a2" && std::get<2>(res) == "a3";
        assert(success());
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res =
            *importAs<decltype(testFunc)>(ns, funcName)(Str("a1"), Str("a2"), Str("a3")).transform_error(logAndAssert);
        result = std::get<0>(res) == "a1" && std::get<1>(res) == "a2" && std::get<2>(res) == "a3";
        assert(success());
    }
    {
        // move only args and return
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](Str2&& a1) -> Str2 { return std::move(a1); };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res = *importAs<decltype(testFunc)>(ns, funcName)(Str2("a1")).transform_error(logAndAssert);
        result = res == "a1";
        assert(success());
    }
    {
        // not exported
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        res = importAs<decltype(testFunc)>(ns, funcName)(Str("a1"), Str("a2"), Str("a3"));
        assert(!res);
        assert(res.error().isA<error_utils::RemoteCallError>());
        assert(res.error().as<error_utils::RemoteCallError>().reason() == error_utils::ErrorReason::NotExported);
        // getLogger().debug(res.error().message());
    }
    {
        // args count not match
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](Str const& a1) -> Str { return a1; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res = importAs<Str()>(ns, funcName)();
        assert(!res);
        assert(res.error().isA<error_utils::RemoteCallError>());
        assert(res.error().as<error_utils::RemoteCallError>().reason() == error_utils::ErrorReason::ArgsCountNotMatch);
        // getLogger().debug(res.error().message());
    }
    {
        // args type not match
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](Str const& a1) -> Str { return a1; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res = importAs<Str(int)>(ns, funcName)(7);
        assert(!res);
        assert(res.error().isA<error_utils::RemoteCallError>());
        assert(res.error().as<error_utils::RemoteCallError>().reason() == error_utils::ErrorReason::UnexpectedType);
        // getLogger().debug(res.error().message());
    }
    {
        // Failed to parse DynamicValue to class BlockPos. Expected alternative struct remote_call::BlockPosType,struct
        // remote_call::WorldPosType. Holding alternative class std::basic_string<char,struct
        // std::char_traits<char>,class std::allocator<char> >. Failed to deserialize value. Field:
        // args[0].node.property.value.pos
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](Root const& a1) -> BlockPos { return a1.node.property.value.pos; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto res = importAs<Player*(AnotherRoot const&)>(ns, funcName)({});
        assert(!res);
        auto msg = res.error().message();
        assert(msg.find("args[0].node.property.value.pos") != std::string::npos);
        assert(res.error().isA<error_utils::RemoteCallError>());
        assert(res.error().as<error_utils::RemoteCallError>().reason() == error_utils::ErrorReason::UnexpectedType);
        // getLogger().debug(msg);
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](CompoundTag& tag) -> CompoundTag& { return tag; };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto tag       = std::make_unique<CompoundTag>();
        (*tag)["test"] = "value";
        CompoundTag& res =
            *importAs<CompoundTag&(std::unique_ptr<CompoundTag>&)>(ns, funcName)(tag).transform_error(logAndAssert);
        assert(res["test"] == "value");
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto testFunc = [&](CompoundTag&) -> ll::Expected<bool> { return ll::makeStringError("TestUnexpected"); };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto tag       = std::make_unique<CompoundTag>();
        (*tag)["test"] = "value";
        auto res       = importAs<decltype(testFunc)>(ns, funcName)(*tag);
        assert(!res);
        assert(res.error().message().find("TestUnexpected") != std::string::npos);
    }
    {
        // Optional args
        std::string funcName = fmt::format("testFunc{}", ++index);
        auto        testFunc = [&](std::optional<std::string> a) -> ll::Expected<std::string> {
            return a.value_or("default");
        };
        exportAs(ns, funcName, testFunc).transform_error(logAndAssert);
        auto tag       = std::make_unique<CompoundTag>();
        (*tag)["test"] = "value";
        auto res1      = *importAs<std::string()>(ns, funcName)().transform_error(logAndAssert);
        assert(res1 == "default");
        auto res2 = *importAs<std::string(std::optional<std::string>)>(ns, funcName)(std::nullopt)
                         .transform_error(logAndAssert);
        assert(res2 == "default");
        auto res3 = *importAs<std::string(std::optional<std::string>)>(ns, funcName)(std::make_optional("value"))
                         .transform_error(logAndAssert);
        assert(res3 == "value");
        auto res4 = *importAs<std::string(std::string const&)>(ns, funcName)("value").transform_error(logAndAssert);
        assert(res4 == "value");
    }
    {
        std::string funcName = fmt::format("testFunc{}", ++index);
        Str         s{"abcdefghijklmnopqrstuvwxy"};
        auto        testFunc = [s]() mutable -> Str { return Str{s.s + "z"}; };
        exportAs(ns, funcName, testFunc);
        ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [ns, funcName] {
                auto str = *importAs<Str()>(ns, funcName)().transform_error(logAndAssert);
                assert(str == "abcdefghijklmnopqrstuvwxyz");
            },
            ll::chrono::ticks{20}
        );
    }
    return true;
}
// NOLINTEND

auto const testResult = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
    [] {
        if (testInvocation()) {
            test::success("Invoke Test passed");
        } else {
            test::success("Invoke Test failed");
        }
    },
    ll::chrono::ticks{1}
);


} // namespace remote_call::test
