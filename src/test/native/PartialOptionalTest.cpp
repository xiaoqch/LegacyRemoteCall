#include "Test.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/utils/ErrorUtils.h"
#include "remote_call/api/API.h"


namespace remote_call::test {


struct PartialType {
    bool $RemoteCallPartial{false};

    ::std::string name{"sp"};
    ::BlockPos    spawnPos{0, 66, 0};
    ::Vec3        spawnPosDelta{1.2, 3.4, 5.6};
    ::Vec2        spawnRotation{7.8, 9.0};
    bool          spawnLoadedFromSave{false};
    // ::DimensionType                  dimensionId{1};
    ::std::string xuid{"12345678"};
    // ::std::optional<::ActorUniqueID> idOverride{};

    struct InnerPartial {
        bool        $RemoteCallPartial{false};
        std::string a{"a"};
        std::string b{"b"};

        bool operator==(InnerPartial const&) const = default;
    } partial{};

    struct InnerRequired {
        std::tuple<int, int, int> pos{0, 67, 0};
        int                       dim{1};

        bool operator==(InnerRequired const&) const = default;
    } required{};

    bool operator==(PartialType const&) const = default;
};

inline constexpr struct Empty {
} EMPTY;

bool testPartialType() {
    constexpr auto&   ns  = TEST_EXPORT_NAMESPACE;
    static auto const ori = PartialType{};
    static_assert(concepts::IsPartialOptional<PartialType>);

    remote_call::exportAs(ns, "forwardPartialType", [&](PartialType const& val) { return val; });
    constexpr auto forwardPartialType =
        remote_call::importEx<PartialType, ll::FixedString<ns.size()>{ns}, "forwardPartialType">();

    {
        struct {
        } testVal;
        auto value  = remote_call::DynamicValue::from(testVal).value();
        auto result = value.tryGet<PartialType>().value();
        assert(result.$RemoteCallPartial);
        result.$RemoteCallPartial = ori.$RemoteCallPartial;
        assert(result == ori);
        forwardPartialType(testVal).value();
    }
    {
        struct {
            ::std::string name = "sp2";
        } testVal;
        auto value  = remote_call::DynamicValue::from(testVal).value();
        auto result = value.tryGet<PartialType>().value();
        assert(result.$RemoteCallPartial);
        result.$RemoteCallPartial = ori.$RemoteCallPartial;
        assert(result.name == "sp2");
        result.name = ori.name;
        assert(result == ori);
        forwardPartialType(testVal).value();
    }
    {
        struct {
            PartialType::InnerPartial partial{.a = "b"};
        } testVal;
        auto value  = remote_call::DynamicValue::from(testVal).value();
        auto result = value.tryGet<PartialType>().value();
        assert(result.$RemoteCallPartial);
        result.$RemoteCallPartial = ori.$RemoteCallPartial;
        assert(result.partial.a == "b");
        result.partial.a = ori.partial.a;
        assert(result == ori);
        forwardPartialType(testVal).value();
    }
    {
        struct {
            struct {
                int dim = 3;
            } required;
        } testVal;
        auto value  = remote_call::DynamicValue::from(testVal).value();
        auto result = value.tryGet<PartialType>();
        assert(!result.has_value());
        assert(!forwardPartialType(testVal).has_value());
    }
    {
        auto value  = remote_call::DynamicValue::from(ori).value();
        auto result = value.tryGet<PartialType>().value();
        assert(result == ori);
        forwardPartialType(result).value();
    }

    assert(ori == forwardPartialType(NULL_VALUE).value());
    assert(ori == forwardPartialType(EMPTY).value());
    assert(ori == forwardPartialType(ori).value());

    return true;
}

[[maybe_unused]] inline static auto test = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
    [] {
        try {
            testPartialType();
            test::success("PartialType Test passed");
        } catch (...) {
            ll::error_utils::printCurrentException(getLogger());
        }
    },
    ll::chrono::ticks{1}
);

} // namespace remote_call::test
