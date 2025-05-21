#include "LegacyRemoteCall.h"

#include "ll/api/mod/RegisterHelper.h"

namespace remote_call {

extern void removeAllFunc();

LegacyRemoteCall& LegacyRemoteCall::getInstance() {
    static LegacyRemoteCall instance;
    return instance;
}

bool LegacyRemoteCall::load() { return true; }

bool LegacyRemoteCall::enable() { return true; }

bool LegacyRemoteCall::disable() {
    remote_call::removeAllFunc();
    return true;
}

} // namespace remote_call

LL_REGISTER_MOD(remote_call::LegacyRemoteCall, remote_call::LegacyRemoteCall::getInstance());
