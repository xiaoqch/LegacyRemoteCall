
#include "Test.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/io/LogLevel.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/reflection/Serialization.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "ll/api/utils/RandomUtils.h"
#include "nlohmann/json.hpp" // IWYU pragma: keep
#include "remote_call/api/RemoteCall.h"

namespace llRand = ll::random_utils;
namespace remote_call::test {

namespace reflection {

template <size_t I, class T>
inline size_t getMemberOffset() {
    return ll::reflection::OffsetGetter<T>::template offset<I>(
        std::make_index_sequence<ll::reflection::member_count_v<T>>()
    );
}


template <ll::reflection::Reflectable T1, ll::reflection::Reflectable T2>
bool isEqual(T1 const& v1, T2 const& v2);

namespace detail {
template <ll::reflection::Reflectable T1, ll::reflection::Reflectable T2, size_t I = 0>
constexpr bool may_equal_v =
    ll::reflection::member_count_v<T1> == ll::reflection::member_count_v<T2>
    && (ll::reflection::member_count_v<T1> == 0
        || (std::same_as<ll::reflection::member_t<I, T1>, ll::reflection::member_t<I, T2>>
            && ll::reflection::member_name_array_v<T1>[I] == ll::reflection::member_name_array_v<T2>[I]));

template <ll::reflection::Reflectable T1, ll::reflection::Reflectable T2, size_t I>
    requires(I < ll::reflection::member_count_v<T1> && I < ll::reflection::member_count_v<T2>)
inline bool isEqualImpl(T1 const& v1, T2 const& v2) {
    using M1  = ll::reflection::member_t<I, T1>;
    using M2  = ll::reflection::member_t<I, T2>;
    auto off1 = getMemberOffset<I, T1>();
    auto off2 = getMemberOffset<I, T2>();
    // make sure getMemberOffset works correctly
    if constexpr (I > 0) assert((sizeof(ll::reflection::member_t<I - 1, T1>) == 0 || off1 > 0));
    if constexpr (requires(M1 const& v1, M2 const& v2) { v1 != v2; }) {
        if (ll::memory::dAccess<M1>(&v1, off1) != ll::memory::dAccess<M2>(&v2, off2)) return false;
    } else if constexpr (ll::reflection::Reflectable<M1> && ll::reflection::Reflectable<M2>) {
        if (!isEqual<M1, M2, 0>(ll::memory::dAccess<M1>(&v1, off1), ll::memory::dAccess<M2>(&v2, off2))) return false;
    } else {
        static_assert(false, "can not compare");
    }
    return true;
}

} // namespace detail

template <ll::reflection::Reflectable T1, ll::reflection::Reflectable T2>
bool isEqual(T1 const& v1, T2 const& v2) {
    if constexpr (ll::reflection::member_count_v<T1> != ll::reflection::member_count_v<T1>) return false;
    else {
        bool equal = true;
        [&]<size_t... I>(std::index_sequence<I...>) {
            equal = (detail::isEqualImpl<T1, T2, I>(v1, v2) && ...);
        }(std::make_index_sequence<ll::reflection::member_count_v<T1>>{});
        return equal;
    }
}

} // namespace reflection

enum class EnumType {
    a,
    b,
};
struct JsonA {
    uchar        byte{};
    short        s{};
    unsigned int ui{};
    // int64_t                              i64{};
    float    f{};
    double   d{};
    EnumType e{};
    // remote_call::NullType                null{};
    std::string                               str;
    std::vector<std::string>                  list;
    std::unordered_map<std::string, int>      record;
    std::unordered_map<ll::io::LogLevel, int> record2{};

    bool         operator==(JsonA const& o) const = default;
    static JsonA random();
};
struct JsonB {
    // JsonA
    uchar        byte{};
    short        s{};
    unsigned int ui{};
    // int64_t                              i64{};
    float    f{};
    double   d{};
    EnumType e{};
    // remote_call::NullType                null{};
    std::string                               str;
    std::vector<std::string>                  list;
    std::unordered_map<std::string, int>      record;
    std::unordered_map<ll::io::LogLevel, int> record2{};

    JsonA                         obj;
    std::vector<JsonA>            vec;
    std::tuple<int, float, JsonA> tuple;

    // REMOTECALL_SERIALIZATION(s, ui, i64, f, d, null, str, list, record, obj, complex, tuple);
    bool         operator==(JsonB const& o) const = default;
    static JsonB random();
    void         print(std::string const& msg = "JsonB") const;
};

ll::coro::CoroTask<bool> testJsonType() {
    constexpr auto& ns = TEST_EXPORT_NAMESPACE;

    auto const ori   = JsonB::random();
    auto       value = remote_call::DynamicValue::from(ori).value();
    auto       res   = value.tryGet<JsonB>().value();
    assert(ori == res);
    auto json = ll::reflection::serialize<nlohmann::ordered_json>(res).value().dump();

    remote_call::exportAs(ns, "forwardJsonType", [&ori](JsonB const& jsonB) {
        assert(jsonB == ori);
        return jsonB;
    });

    auto forwardJsonType = remote_call::importAs<JsonB(JsonB const&)>(ns, "forwardJsonType");
    auto forwarded       = forwardJsonType(res).value();
    assert(res == forwarded);
    assert(reflection::isEqual(res, forwarded));

    auto deadline = ll::chrono::GameTickClock::now() + ll::chrono::ticks{100};
    while (!remote_call::hasFunc(ns, "lseCompareJson")) {
        if (ll::chrono::GameTickClock::now() > deadline) break;
        co_await ll::chrono::ticks{5};
    }
    if (remote_call::hasFunc(ns, "lseCompareJson")) {
        auto const lseCompareJson = remote_call::importAs<bool(JsonB, std::string&&)>(ns, "lseCompareJson");
        lseCompareJson(ori, std::move(json)).value();
    }
    co_return true;
}


auto simpleTestStarted =
    (testJsonType().launch(
         ll::thread::ServerThreadExecutor::getDefault(),
         [](ll::Expected<bool>&& result) {
             if (result) test::success("SimpleType Test passed");
             else result.error().log(getLogger());
         }
     ),
     true);
JsonA JsonA::random() {
    JsonA b;
    b.byte = llRand::rand<uchar>();
    b.s    = llRand::rand<decltype(s)>();
    b.ui   = llRand::rand<decltype(ui)>();
    // // b.i64  = llRand::rand<decltype(i64)>();
    b.f = llRand::rand<decltype(f)>();
    b.d = llRand::rand<decltype(d)>();
    b.e = static_cast<EnumType>(llRand::rand<std::underlying_type_t<decltype(e)>>());
    // b.null = detail::NULL_VALUE;
    b.str         = ll::string_utils::intToHexStr(llRand::rand<int>());
    auto listview = std::views::iota(0, llRand::rand<int>(5, 15))
                  | std::views::transform([](auto&&) { return ll::string_utils::intToHexStr(llRand::rand<int>()); });
    std::set<int> keys{};
    while (keys.size() < 20) keys.insert(llRand::rand<int>());
    auto recordView =
        keys | std::views::transform([](auto&& key) {
            return std::pair<std::string, int>{"a" + ll::string_utils::intToHexStr(key), llRand::rand<int>()};
        });
    b.list    = std::vector(listview.begin(), listview.end());
    b.record  = std::unordered_map(recordView.begin(), recordView.end());
    b.record2 = {
        {ll::io::LogLevel::Info,  llRand::rand<int>()},
        {ll::io::LogLevel::Debug, llRand::rand<int>()}
    };
    return b;
}

void JsonB::print(std::string const& msg) const {
    fmt::println("{}: {}", msg, ll::reflection::serialize<nlohmann::ordered_json>(*this).value().dump());
}

JsonB JsonB::random() {

    JsonB s;
    reinterpret_cast<JsonA&>(s) = JsonA::random();
    s.obj                       = JsonA::random();
    auto listview =
        std::views::iota(0, llRand::rand<int>(10, 20)) | std::views::transform([](auto&&) { return JsonA::random(); });
    s.vec   = std::vector(listview.begin(), listview.end());
    s.tuple = {llRand::rand<int>(), llRand::rand<float>(), JsonA::random()};
    return s;
}

} // namespace remote_call::test
