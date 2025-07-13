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
    mutable std::vector<std::string> mFields{};
    std::optional<ll::Error>         mOriginError{};

public:
    inline RemoteCallError(ErrorReason reason, std::string&& msg, std::optional<ll::Error> originError = {})
    : mReason(reason),
      mMessage(std::move(msg)),
      mOriginError(std::move(originError)) {}

    inline ~RemoteCallError() override = default;

    [[nodiscard]] constexpr std::string message() const noexcept override {
        if (!mOriginError) return mMessage;
        return fmt::format("{}\nOrigin Error: {}", mMessage, mOriginError->message());
    }

    constexpr RemoteCallError& append(std::string_view msg) {
        if (!mMessage.ends_with("\n")) mMessage += "\n";
        mMessage += msg;
        return *this;
    }

    template <typename Fn = void>
    inline RemoteCallError& append(std::string_view ns, std::string_view func) {
        if (!mMessage.ends_with("\n")) mMessage += "\n";
        auto  provider      = getProvider(ns, func).lock();
        auto& providerName  = provider ? provider->getName() : "UnknownMod";
        mMessage           += fmt::format(
            "Function: [{}::{}](signature {}) provided by <{}>.\n",
            ns,
            func,
            type_raw_name_v<Fn>,
            providerName
        );
        return *this;
    }

    constexpr RemoteCallError& joinField(std::string_view field) {
        mFields.emplace_back(field);
        return *this;
    }

    constexpr RemoteCallError& flushFields(std::string_view msg, std::string_view valueName) {
        if (!mMessage.ends_with("\n")) mMessage += "\n";
        if (msg.ends_with("\n")) msg.remove_suffix(1);
        std::string fieldPath;
        fieldPath.reserve(valueName.size() + mFields.size() * 10);

        fieldPath += valueName;
        for (auto& field : std::ranges::reverse_view(mFields)) {
            fieldPath += field;
        }
        mMessage += fmt::format("{} Field: {}\n", msg, fieldPath);
        mFields.clear();
        return *this;
    }

    [[nodiscard]] constexpr ErrorReason reason() const noexcept { return mReason; }

    [[nodiscard]] inline static bool isRemoteCallError(ll::Error& error) noexcept {
        auto info = &error.as<ll::ErrorInfoBase>();
        try {
            return dynamic_cast<RemoteCallError*>(info) != nullptr;
        } catch (...) {
            return false;
        }
    }
};

struct RemoteCallErrorWrapper {
    ll::Error error;

    inline explicit RemoteCallErrorWrapper(ll::Error err, std::string_view unknownErrorMsg = "")
    : error(
          RemoteCallError::isRemoteCallError(err)
              ? std::move(err)
              : ll::Error{std::make_unique<RemoteCallError>(
                    ErrorReason::Unknown,
                    fmt::format("{}: {}", unknownErrorMsg, err.as<ll::ErrorInfoBase>().message()),
                    std::move(err)
                )}
      ) {}

    inline explicit RemoteCallErrorWrapper(ErrorReason reason, std::string&& msg)
    : error(std::make_unique<RemoteCallError>(reason, std::move(msg))) {}

    RemoteCallError& get() noexcept { return error.as<RemoteCallError>(); }

    inline operator ll::Unexpected() noexcept { return ll::forwardError(error); } // NOLINT(google-explicit-constructor)

    [[nodiscard]] std::string message() noexcept { return get().message(); }
    // clang-format off
    constexpr RemoteCallErrorWrapper& append(std::string_view msg) { get().append(msg); return *this; }
    template <typename Fn = void>
    constexpr RemoteCallErrorWrapper& append(std::string_view ns, std::string_view func) { get().append<Fn>(ns, func); return *this; }
    constexpr RemoteCallErrorWrapper& joinField(std::string_view field) { get().joinField(field); return *this; }
    constexpr RemoteCallErrorWrapper& flushFields(std::string_view msg, std::string_view valueName) { get().flushFields(msg, valueName); return *this; }
    [[nodiscard]] ErrorReason reason() noexcept { return get().reason(); }
    // clang-format on
};

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeError(ErrorReason reason, std::string_view msg) {
    return RemoteCallErrorWrapper(reason, std::string(msg));
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeError(std::string_view ns, std::string_view func, ErrorReason reason, std::string_view msg) {
    return RemoteCallErrorWrapper(reason, std::string{msg}) //
        .append(ns, func);
}

template <typename Fn>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeUnknownError(std::string_view ns, std::string_view func, std::string_view msg, ll::Error&& error) {
    return RemoteCallErrorWrapper(std::move(error), msg) //
        .template append<Fn>(ns, func);
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeNotFoundError(std::string_view ns, std::string_view func) {
    return RemoteCallErrorWrapper(ErrorReason::NotExported, "Fail to import! Function has not been exported.")
        .append(ns, func);
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeDisabledError(std::string_view ns, std::string_view func) {
    return RemoteCallErrorWrapper(ErrorReason::ProviderDisabled, "Fail to import! Provider has been disabled.")
        .append(ns, func);
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeArgsCountError(std::string_view ns, std::string_view func, size_t provided) {
    return RemoteCallErrorWrapper(
               ErrorReason::ArgsCountNotMatch,
               fmt::format("Fail to invoke! Function requires {} args, but {} provided.", sizeof...(Args), provided)
    )
        .template append<Ret(Args...)>(ns, func);
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeSerializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    bool isRemoteCallError = RemoteCallError::isRemoteCallError(error);
    auto wrapper           = RemoteCallErrorWrapper(std::move(error), "Serialization Error");
    if (isRemoteCallError) {
        wrapper.flushFields("Failed to serialize value", valueName);
    }
    return wrapper.template append<Ret(Args...)>(ns, func);
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeDeserializeError(std::string_view ns, std::string_view func, std::string_view valueName, ll::Error& error) {
    bool isRemoteCallError = RemoteCallError::isRemoteCallError(error);
    auto wrapper           = RemoteCallErrorWrapper(std::move(error), "Deserialization Error");
    if (isRemoteCallError) {
        wrapper.flushFields("Failed to deserialize value", valueName);
    }
    return wrapper.template append<Ret(Args...)>(ns, func);
}

template <typename Ret, typename... Args>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeCallError(std::string_view ns, std::string_view func, ll::Error& error) {
    return RemoteCallErrorWrapper(std::move(error), "Call Error")
        .append("Failed to call function!")
        .template append<Ret(Args...)>(ns, func);
}

template <typename Target, typename... Expected>
[[nodiscard]] LL_NOINLINE LL_CONSTEXPR23 ll::Unexpected makeFromDynamicTypeError(DynamicValue const& value) {
    return RemoteCallErrorWrapper(
        ErrorReason::UnexpectedType,
        fmt::format(
            "Failed to convert DynamicValue to {}. Expected: [{}]. Actual: {}.",
            type_raw_name_v<Target>,
            typeListName<Expected...>(),
            typeName(value)
        )
    );
}

template <typename T, typename Target>
[[nodiscard]] LL_NOINLINE inline ll::Unexpected
makeUnsupportedValueError(std::string_view value, std::string_view msg) {
    return RemoteCallErrorWrapper(
        ErrorReason::UnsupportedValue,
        fmt::format(
            "Unsupported value conversion: {}<{}> -> {}. {}",
            type_raw_name_v<T>,
            value,
            type_raw_name_v<Target>,
            msg
        )
    );
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeSerMemberError(std::string_view name, ll::Error& error) {
    return RemoteCallErrorWrapper(std::move(error), "Member Serialization Error") //
        .joinField(fmt::format(".{}", name));
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeSerIndexError(size_t idx, ll::Error& error) {
    return RemoteCallErrorWrapper(std::move(error), "Index Serialization Error") //
        .joinField(fmt::format("[{}]", idx));
}

[[nodiscard]] LL_NOINLINE inline ll::Unexpected makeSerKeyError(std::string_view key, ll::Error& error) {
    return RemoteCallErrorWrapper(std::move(error), "Key Serialization Error") //
        .joinField(fmt::format("[\"{}\"]", key));
}

} // namespace remote_call::error_utils