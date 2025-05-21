
#include "Test.h"
#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/memory/Memory.h"
#include "ll/api/thread/ServerThreadExecutor.h"
#include "remote_call/api/RemoteCall.h"

#include <nlohmann/json.hpp>

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


struct JsonA {
    uchar        byte{};
    short        s{};
    unsigned int ui{};
    // int64_t                              i64{};
    float                                f{};
    double                               d{};
    std::nullptr_t                       null{};
    std::string                          str;
    std::vector<std::string>             list;
    std::unordered_map<std::string, int> record;

    bool         operator==(JsonA const& o) const = default;
    static JsonA random();
};
struct JsonB {
    // JsonA
    uchar        byte{};
    short        s{};
    unsigned int ui{};
    // int64_t                              i64{};
    float                                f{};
    double                               d{};
    std::nullptr_t                       null{};
    std::string                          str;
    std::vector<std::string>             list;
    std::unordered_map<std::string, int> record;

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
    auto       value = remote_call::reflection::serialize<remote_call::DynamicValue>(ori);
    if (!value) {
        value.error().log(getLogger());
        co_return false;
    }
    auto res = remote_call::reflection::deserialize<JsonB>(*std::move(value));
    if (!res) {
        res.error().log(getLogger());
        co_return false;
    }
    assert(ori == *res);
    auto json = remote_call::reflection::serialize<nlohmann::ordered_json>(*res)->dump();

    remote_call::exportAs(ns, "forwardJsonType", [&ori](JsonB const& jsonB) {
        assert(jsonB == ori);
        return jsonB;
    });

    auto&& forwardJsonType = remote_call::importAs<JsonB(JsonB const&)>(ns, "forwardJsonType");
    auto   forwarded       = forwardJsonType(*res);
    assert(*res == *forwarded);
    assert(reflection::isEqual(*res, *forwarded));

    auto deadline = ll::chrono::GameTickClock::now() + ll::chrono::ticks{100};
    while (!remote_call::hasFunc(ns, "lseCompareJson")) {
        assert(ll::chrono::GameTickClock::now() <= deadline);
        co_await ll::chrono::ticks{5};
    }
    auto const lseCompareJson = remote_call::importAs<bool(JsonB, std::string&&)>(ns, "lseCompareJson");
    auto       equal          = lseCompareJson(ori, std::move(json));
    assert(*equal);
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
    JsonA                      b;
    std::random_device         r;
    std::default_random_engine generator(r());
    auto                       dice = [&generator](auto&& distribution) { return distribution(generator); };
    b.byte                          = (uchar)dice(std::uniform_int_distribution<unsigned short>()) % UINT16_MAX;
    b.s                             = dice(std::uniform_int_distribution<decltype(s)>());
    b.ui                            = dice(std::uniform_int_distribution<decltype(ui)>());
    // // b.i64  = dice(std::uniform_int_distribution<decltype(i64)>());
    b.f    = dice(std::uniform_real_distribution<decltype(f)>());
    b.d    = dice(std::uniform_real_distribution<decltype(d)>());
    b.null = nullptr;
    b.str  = ll::string_utils::intToHexStr(dice(std::uniform_int_distribution()));
    auto listview =
        std::views::iota(0, dice(std::uniform_int_distribution()) % 10 + 5) | std::views::transform([&dice](auto&&) {
            return ll::string_utils::intToHexStr(dice(std::uniform_int_distribution()));
        });
    std::set<int> keys{};
    while (keys.size() < 20) keys.insert(dice(std::uniform_int_distribution()));
    auto recordView = keys | std::views::transform([&dice](auto&& key) {
                          return std::pair<std::string, int>{
                              "a" + ll::string_utils::intToHexStr(key),
                              dice(std::uniform_int_distribution())
                          };
                      });
    b.list          = std::vector(listview.begin(), listview.end());
    b.record        = std::unordered_map(recordView.begin(), recordView.end());
    return b;
}

void JsonB::print(std::string const& msg) const {
    fmt::println("{}: {}", msg, remote_call::reflection::serialize<nlohmann::ordered_json>(*this)->dump());
}

JsonB JsonB::random() {

    JsonB s;
    reinterpret_cast<JsonA&>(s) = JsonA::random();
    s.obj                       = JsonA::random();
    auto listview = std::views::iota(0, 10) | std::views::transform([](auto&&) { return JsonA::random(); });
    s.vec         = std::vector(listview.begin(), listview.end());
    s.tuple       = {7, 9.0f, JsonA::random()};
    return s;
}

} // namespace remote_call::test
