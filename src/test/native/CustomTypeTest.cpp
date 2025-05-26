#include "Test.h"
#include "ll/api/Expected.h"
#include "ll/api/reflection/Reflection.h"
#include "mc/world/item/ItemInstance.h"
#include "remote_call/api/RemoteCall.h"


class CustomType : std::variant<std::string, ItemInstance> {
    std::string customName;

public:
    explicit CustomType(std::string id) : std::variant<std::string, ItemInstance>(id) {}
    explicit CustomType(ItemInstance item) : std::variant<std::string, ItemInstance>(item) {}
};

struct CustomTypeWrapper {
    std::string name;
    CustomType  customType;
};

static_assert(ll::reflection::Reflectable<CustomTypeWrapper>);
static_assert(!std::is_constructible_v<CustomTypeWrapper>);

template <>
struct ::remote_call::AdlSerializer<CustomType> {
    inline static ll::Expected<CustomType> fromDynamic(DynamicValue& dv) {
        if (dv.hold<std::string>()) {
            return CustomType(dv.get<std::string>());
        } else if (dv.hold<ItemType>()) {
            return CustomType(ItemInstance(*dv.get<ItemType>().ptr));
        } else {
            return ll::makeStringError("Invalid type");
        }
    }
};

namespace remote_call::test {

static_assert(ll::reflection::Reflectable<CustomTypeWrapper>);
static_assert(concepts::SupportFromDynamic<std::optional<CustomType>>);

} // namespace remote_call::test
