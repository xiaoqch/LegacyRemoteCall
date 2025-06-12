#pragma once

#include "Common.h"
#include "ll/api/base/FixedString.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/utils/ErrorUtils.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::impl {

template <typename Ret>
struct corrected_return {
    using Real     = Ret;
    using Excepted = decltype(std::declval<DynamicValue>().tryGet<Ret>());
};
template <typename Ret>
struct corrected_return<Ret const> : corrected_return<Ret> {};

template <typename Ret>
struct corrected_return<Ret&&> : corrected_return<Ret> {};

template <typename Ret>
    requires(!concepts::SupportFromDynamic<Ret*>)
struct corrected_return<Ret&> : corrected_return<Ret> {};

template <typename Ret>
struct corrected_return<ll::Expected<Ret>> : corrected_return<Ret> {};

template <typename Ret, typename... Args>
[[nodiscard]] inline corrected_return<Ret>::Excepted
importCallImpl(std::string const& nameSpace, std::string const& funcName, Args&&... args) {
    using ExpectedRet = corrected_return<Ret>::Excepted;
    using RealRet     = corrected_return<Ret>::Real;

    auto res = importFunc(nameSpace, funcName);
    if (!res) {
        return ll::forwardError(res.error());
    }
    auto const&  rawFunc = *res;
    DynamicValue params  = DynamicValue::array();
    params.get<DynamicArray>().reserve(sizeof...(Args));
    auto paramTuple = std::forward_as_tuple(std::forward<Args>(args)...);

    ll::Expected<> success = toDynamic(params, std::forward<decltype(paramTuple)>(paramTuple));
    if (!success) return error_utils::makeSerializeError<Ret, Args...>(nameSpace, funcName, "args", success.error());

    auto dynRet = rawFunc(std::move(params).get<DynamicArray>());
    if (!dynRet) return error_utils::makeCallError<Ret, Args...>(nameSpace, funcName, dynRet.error());

    return dynRet->tryGet<RealRet>().or_else([&nameSpace, &funcName](auto&& err) -> ExpectedRet {
        return error_utils::makeDeserializeError<Ret, Args...>(nameSpace, funcName, "ret", err);
    });
}

template <typename Ret, typename... Args>
[[nodiscard]] constexpr auto
importImpl(std::in_place_type_t<Ret(Args...)>, std::string const& nameSpace, std::string const& funcName) {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [nameSpace, funcName](Args... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace, funcName, std::forward<decltype(args)>(args)...);
    };
}

template <ll::FixedString nameSpace, ll::FixedString funcName, typename Ret, typename... Args>
[[nodiscard]] consteval auto importImpl() {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [](Args... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace.str(), funcName.str(), std::forward<decltype(args)>(args)...);
    };
}

template <typename Ret>
[[nodiscard]] constexpr auto importExImpl(std::string const& nameSpace, std::string const& funcName) {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [nameSpace, funcName](auto&&... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace, funcName, std::forward<decltype(args)>(args)...);
    };
}

template <ll::FixedString nameSpace, ll::FixedString funcName, typename Ret>
[[nodiscard]] consteval auto importExImpl() {
    checkUptrType<Ret>();
    using ExpectedRet = corrected_return<Ret>::Excepted;
    return [](auto&&... args) -> ExpectedRet {
        return importCallImpl<Ret>(nameSpace.str(), funcName.str(), std::forward<decltype(args)>(args)...);
    };
}


} // namespace remote_call::impl