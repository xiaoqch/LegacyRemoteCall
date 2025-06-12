#pragma once

#include "fmt/core.h"
#include "ll/api/Expected.h"
#include "ll/api/base/Concepts.h"
#include "ll/api/mod/Mod.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/base/Concepts.h"
#include "remote_call/api/reflection/TypeName.h"
#include "remote_call/api/value/DynamicValue.h"


namespace remote_call::error_utils {

using ll::reflection::type_raw_name_v;
using reflection::typeListName;
using reflection::typeName;

enum class ErrorReason {
    Unknown = 0,
    InvalidName,
    AlreadyExists,
    NotExported,
    ProviderDisabled,
    ArgsCountNotMatch,
    UnexpectedType,
    UnsupportedValue,
    IndexOutOfRange,
    KeyNotFound,
};

class RemoteCallError : public ll::ErrorInfoBase {
    ErrorReason                      mReason;
    mutable std::string              mMessage;
    mutable std::vector<std::string> mFiled{};
    std::optional<ll::Error>         mOriginError{};

public:
    inline RemoteCallError(ErrorReason reason, std::string&& msg, std::optional<ll::Error> originError = {}) noexcept
    : mReason(reason),
      mMessage(std::move(msg)),
      mOriginError(std::move(originError)) {}
    inline ~RemoteCallError() override = default;
    [[nodiscard]] constexpr std::string message() const noexcept override {
        if (!mOriginError) return mMessage;
        return fmt::format("{}\nOrigin Error: {}", mMessage, mOriginError->message());
    }
    constexpr RemoteCallError& append(std::string_view msg) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        mMessage.append(msg);
        return *this;
    }
    template <typename Fn = void>
    inline RemoteCallError& append(std::string_view ns, std::string_view func) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        auto  provider     = getProvider(ns, func).lock();
        auto& providerName = provider ? provider->getName() : "UnknownMod";
        mMessage.append(fmt::format(
            "Function: [{}::{}](signature {}) provided by <{}>.\n",
            ns,
            func,
            type_raw_name_v<Fn>,
            providerName
        ));
        return *this;
    }
    constexpr RemoteCallError& joinField(std::string_view field) {
        mFiled.emplace_back(field);
        return *this;
    }
    constexpr RemoteCallError& flushFields(std::string_view msg, std::string_view valueName) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        if (msg.ends_with("\n")) msg.remove_suffix(1);
        std::string field = std::string(valueName);
        for (auto&& f : mFiled | std::views::reverse) field.append(f);
        mMessage.append(msg).append(" Field: ").append(field).append("\n");
        mFiled.clear();
        return *this;
    }
    [[nodiscard]] constexpr ErrorReason reason() const noexcept { return mReason; }

    [[nodiscard]] LL_CONSTEXPR23 static bool isRemoteCallError(ll::Error& error) {
        /// TODO:
        return error.isA<RemoteCallError>();
    }
};


[[nodiscard]] LL_CONSTEXPR23 std::unique_ptr<RemoteCallError>
                             convertUnknownError(std::string_view msg, ll::Error&& error) {
    return std::make_unique<RemoteCallError>(
        ErrorReason::Unknown,
        fmt::format("{}: {}", msg, error.as<ll::ErrorInfoBase>().message()),
        std::move(error)
    );
}

[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeError(ErrorReason reason, std::string_view msg) {
    auto error = std::make_unique<RemoteCallError>(reason, std::string(msg));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

[[nodiscard]] inline ll::Unexpected
makeError(std::string_view ns, std::string_view func, ErrorReason reason, std::string_view msg) {
    auto error = std::make_unique<RemoteCallError>(reason, std::string(msg));
    error->append(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

template <typename Fn>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected
makeUnknownError(std::string_view ns, std::string_view func, std::string_view msg, ll::Error&& error) {
    auto newError = convertUnknownError(msg, std::move(error));
    newError->append<Fn>(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}

[[nodiscard]] inline ll::Unexpected makeNotFoundError(std::string_view ns, std::string_view func) {
    return makeError(ns, func, ErrorReason::NotExported, "Fail to import! Function has not been exported.");
}


[[nodiscard]] inline ll::Unexpected makeDisabledError(std::string_view ns, std::string_view func) {
    return makeError(ns, func, ErrorReason::ProviderDisabled, "Fail to import! Provider has not been disabled.");
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected
                             makeArgsCountError(std::string_view ns, std::string_view func, size_t provided) {
    auto error = std::make_unique<RemoteCallError>(
        ErrorReason::ArgsCountNotMatch,
        fmt::format("Fail to invoke! function requires {} args, but {} provided.", sizeof...(Args), provided)
    );
    error->append<Ret(Args...)>(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected
makeSerializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    if (RemoteCallError::isRemoteCallError(error)) {
        auto& newError = error.as<RemoteCallError>();
        newError.flushFields("Failed to serialize value.", valueName);
        newError.append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Serialization Error: ", std::move(error));
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected
makeDeserializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    if (RemoteCallError::isRemoteCallError(error)) {
        auto& newError = error.as<RemoteCallError>();
        newError.flushFields("Failed to deserialize value.", valueName);
        newError.append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Deserialization Error: {}", std::move(error));
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected
                             makeCallError(std::string_view ns, std::string_view func, ll::Error& error) {
    if (RemoteCallError::isRemoteCallError(error)) {
        auto& newError = error.as<RemoteCallError>();
        newError.append("Failed to call function!").append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Call Error: {}", std::move(error));
}

template <typename Target, typename... Expected>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeFromDynamicTypeError(DynamicValue const& value) {
    return makeError(
        ErrorReason::UnexpectedType,
        fmt::format(
            "Failed to parse DynamicValue to {}. Expected alternative {}. Holding alternative {}.",
            type_raw_name_v<Target>,
            typeListName<Expected...>(),
            typeName(value)
        )
    );
}

template <typename T, typename Target>
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeUnsupportedValueError(std::string_view value, std::string_view msg) {
    return makeError(
        ErrorReason::UnsupportedValue,
        fmt::format("Failed to convert {}<{}> to {}. {}", type_raw_name_v<T>, value, type_raw_name_v<Target>, msg)
    );
}

[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeSerMemberError(std::string_view name, ll::Error& error) noexcept {
    if (RemoteCallError::isRemoteCallError(error)) {
        error.as<RemoteCallError>().joinField(fmt::format(".{}", name));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Member Error", std::move(error));
    newError->joinField(fmt::format(".{}", name));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}
[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeSerIndexError(std::size_t idx, ll::Error& error) noexcept {
    if (RemoteCallError::isRemoteCallError(error)) {
        error.as<RemoteCallError>().joinField(fmt::format("[{}]", idx));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Index Error", std::move(error));
    newError->joinField(fmt::format("[{}]", idx));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}

[[nodiscard]] LL_CONSTEXPR23 ll::Unexpected makeSerKeyError(std::string_view key, ll::Error& error) noexcept {
    if (RemoteCallError::isRemoteCallError(error)) {
        error.as<RemoteCallError>().joinField(fmt::format("[\"{}\"]", key));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Key Error", std::move(error));
    newError->joinField(fmt::format("[\"{}\"]", key));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}


} // namespace remote_call::error_utils