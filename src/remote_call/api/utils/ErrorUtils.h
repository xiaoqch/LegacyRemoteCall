#pragma once

#include "fmt/core.h"
#include "ll/api/Expected.h"
#include "ll/api/mod/Mod.h"
#include "ll/api/reflection/TypeName.h"
#include "remote_call/api/ABI.h"
#include "remote_call/api/value/DynamicValue.h"

namespace remote_call::error_utils {

using ll::reflection::type_raw_name_v;

struct UnknownFn {};

enum class ErrorReason {
    Unknown = 0,
    InvalidName,
    AlreadyExists,
    FunctionNotExported,
    ProviderDisabled,
    ArgsCountNotMatch,
    UnexpectedType,
    IndexOutOfRange,
    KeyNotFound,
};

class RemoteCallError : public ll::ErrorInfoBase {
    ErrorReason                      mReason;
    mutable std::string              mMessage;
    mutable std::vector<std::string> mFiled{};
    std::optional<ll::Error>         mOriginError{};

public:
    RemoteCallError(ErrorReason reason, std::string&& msg, std::optional<ll::Error> originError = {}) noexcept
    : mReason(reason),
      mMessage(std::move(msg)),
      mOriginError(std::move(originError)) {
        assert(!mOriginError || !mOriginError->isA<RemoteCallError>());
    }
    ~RemoteCallError() override = default;
    std::string message() const noexcept override {
        if (!mOriginError) return mMessage;
        return fmt::format("{}\nOrigin Error: {}", mMessage, mOriginError->message());
    }
    RemoteCallError& append(std::string_view msg) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        mMessage.append(msg);
        return *this;
    }
    template <typename Fn = UnknownFn>
    RemoteCallError& append(std::string_view ns, std::string_view func) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        auto  provider     = getProvider(ns, func).lock();
        auto& providerName = provider ? provider->getName() : "Unknown Mod";
        mMessage.append(fmt::format(
            "Function: [{}::{}](signature {}) provided by <{}>.\n",
            ns,
            func,
            type_raw_name_v<Fn>,
            providerName
        ));
        return *this;
    }
    RemoteCallError& joinField(std::string_view field) {
        mFiled.emplace_back(field);
        return *this;
    }
    RemoteCallError& flushFields(std::string_view msg, std::string_view valueName) {
        if (!mMessage.ends_with("\n")) mMessage.append("\n");
        if (msg.ends_with("\n")) msg.remove_suffix(1);
        std::string field = std::string(valueName);
        for (auto&& f : mFiled | std::views::reverse) field.append(f);
        mMessage.append(msg).append(" Field: ").append(field).append("\n");
        mFiled.clear();
        return *this;
    }
    ErrorReason reason() const noexcept { return mReason; }
};


inline std::unique_ptr<RemoteCallError> convertUnknownError(std::string_view msg, ll::Error&& error) {
    return std::make_unique<RemoteCallError>(
        ErrorReason::Unknown,
        fmt::format("{}: {}", msg, error.as<ll::ErrorInfoBase>().message()),
        std::move(error)
    );
}

inline ll::Unexpected makeError(ErrorReason reason, std::string_view msg) {
    auto error = std::make_unique<RemoteCallError>(reason, std::string(msg));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

template <typename Fn>
inline ll::Unexpected
makeUnknownError(std::string_view ns, std::string_view func, std::string_view msg, ll::Error&& error) {
    auto newError = convertUnknownError(msg, std::move(error));
    newError->append<Fn>(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}

inline ll::Unexpected makeNotFoundError(std::string_view ns, std::string_view func) {
    auto error = std::make_unique<RemoteCallError>(
        ErrorReason::FunctionNotExported,
        "Fail to import! Function has not been exported."
    );
    error->append(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

template <typename Ret, typename... Args>
inline ll::Unexpected makeArgsCountError(std::string_view ns, std::string_view func, size_t provided) {
    auto error = std::make_unique<RemoteCallError>(
        ErrorReason::ArgsCountNotMatch,
        fmt::format("Fail to invoke! function requires {} args, but {} provided.", sizeof...(Args), provided)
    );
    error->append<Ret(Args...)>(ns, func);
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(error));
}

template <typename Ret, typename... Args>
inline ll::Unexpected
makeSerializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    if (error.isA<RemoteCallError>()) {
        auto& newError = error.as<RemoteCallError>();
        newError.flushFields("Failed to serialize value.", valueName);
        newError.append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Serialization Error: ", std::move(error));
}

template <typename Ret, typename... Args>
inline ll::Unexpected
makeDeserializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    if (error.isA<RemoteCallError>()) {
        auto& newError = error.as<RemoteCallError>();
        newError.flushFields("Failed to deserialize value.", valueName);
        newError.append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Deserialization Error: {}", std::move(error));
}

template <typename Ret, typename... Args>
inline ll::Unexpected makeCallError(std::string_view ns, std::string_view func, ll::Error& error) {
    if (error.isA<RemoteCallError>()) {
        auto& newError = error.as<RemoteCallError>();
        newError.append("Failed to call function!").append<Ret(Args...)>(ns, func);
        return ll::forwardError(error);
    }
    return makeUnknownError<Ret(Args...)>(ns, func, "Unknown Call Error: {}", std::move(error));
}

inline std::string_view getTypeName(DynamicValue const& v) {
    constexpr auto getRawName = [](auto&& v) { return type_raw_name_v<std::decay_t<decltype(v)>>; };
    if (v.hold<ElementType>()) {
        return std::visit(getRawName, v.get<ElementType>());
    } else {
        return std::visit(getRawName, v);
    }
}

template <typename... T>
consteval std::string_view typeListName() {
    std::string_view res = type_raw_name_v<void(T...)>;
    res.remove_prefix(5);
    res.remove_suffix(1);
    return res;
}

template <typename Target, typename... Expected>
inline ll::Unexpected makeFromDynamicTypeError(DynamicValue const& value) {
    return makeError(
        ErrorReason::UnexpectedType,
        fmt::format(
            "Failed to parse DynamicValue to {}. Expected alternative {}. Holding alternative {}.",
            type_raw_name_v<Target>,
            typeListName<Expected...>(),
            getTypeName(value)
        )
    );
}

inline ll::Unexpected makeSerMemberError(std::string_view name, ll::Error& error) noexcept {
    if (error.isA<RemoteCallError>()) {
        error.as<RemoteCallError>().joinField(fmt::format(".{}", name));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Member Error", std::move(error));
    newError->joinField(fmt::format(".{}", name));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}
inline ll::Unexpected makeSerIndexError(std::size_t idx, ll::Error& error) noexcept {
    if (error.isA<RemoteCallError>()) {
        error.as<RemoteCallError>().joinField(fmt::format("[{}]", idx));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Index Error", std::move(error));
    newError->joinField(fmt::format("[{}]", idx));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}
inline ll::Unexpected makeSerKeyError(std::string_view key, ll::Error& error) noexcept {
    if (error.isA<RemoteCallError>()) {
        error.as<RemoteCallError>().joinField(fmt::format("[\"{}\"]", key));
        return ll::forwardError(error);
    }
    auto newError = convertUnknownError("Unknown Serialization Key Error", std::move(error));
    newError->joinField(fmt::format("[\"{}\"]", key));
    return ::nonstd::make_unexpected<ll::Error>(std::in_place, std::move(newError));
}


} // namespace remote_call::error_utils