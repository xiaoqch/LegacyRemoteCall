#pragma once

#include "ll/api/mod/NativeMod.h"

namespace remote_call {

class LegacyRemoteCall {

public:
    static LegacyRemoteCall& getInstance();

    LegacyRemoteCall() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();

    bool enable();

    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace legacy_remotecallapi
