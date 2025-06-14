#pragma once

#include "mc/deps/core/math/Vec3.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"


class Block;
class Player;
class Actor;
class BlockActor;
class Container;

namespace remote_call {
// NOLINTBEGIN: google-explicit-constructor

template <class T>
    requires(std::destructible<T>)
struct MayUniquePtr {
    T const* ptr = nullptr;
    bool     own = false;

    constexpr MayUniquePtr(const MayUniquePtr&) = delete;
    constexpr MayUniquePtr(MayUniquePtr&& o) noexcept : ptr(o.ptr), own(o.own) { o.own = false; };
    constexpr MayUniquePtr& operator=(const MayUniquePtr&) = delete;
    constexpr MayUniquePtr& operator=(MayUniquePtr&& o) {
        if (own && ptr) delete ptr;
        ptr   = o.ptr;
        own   = o.own;
        o.own = false;
        return *this;
    };
    constexpr MayUniquePtr(std::unique_ptr<T>&& tag) noexcept : ptr(tag.release()), own(true) {};
    constexpr MayUniquePtr(std::unique_ptr<T> const& tag) noexcept : ptr(tag.get()), own(false) {};
    constexpr MayUniquePtr(T const* ptr) noexcept : ptr(ptr), own(false) {};
    constexpr ~MayUniquePtr() {
        if (own && ptr) delete ptr;
        own = false;
        ptr = nullptr;
    }
    [[nodiscard]] constexpr std::unique_ptr<T> tryGetUniquePtr() noexcept(std::is_nothrow_constructible_v<T, T&>) {
        if (!own) {
            if (!ptr) return {};
            return std::make_unique<T>(*ptr); // clone
        }
        own = false;
        return std::unique_ptr<T>(const_cast<T*>(ptr));
    }
    template <typename RTN = T const*>
    [[nodiscard]] constexpr RTN get() = delete;
    template <>
    [[nodiscard]] constexpr T const* get() noexcept {
        return ptr;
    };
    template <>
    [[nodiscard]] constexpr T* get() noexcept {
        return const_cast<T*>(ptr);
    };
    template <>
    [[nodiscard]] constexpr std::unique_ptr<T> get() noexcept(noexcept(tryGetUniquePtr())) {
        return tryGetUniquePtr();
    };
};

struct NbtType : public MayUniquePtr<CompoundTag> {};
struct ItemType : public MayUniquePtr<ItemStack> {};

struct BlockType {
    Block const* block;
    BlockPos     blockPos{};
    int          dimension = 0;

    constexpr BlockType(Block const* ptr) noexcept : block(ptr) {};
    template <typename RTN>
    [[nodiscard]] constexpr RTN get() const = delete;
    template <>
    [[nodiscard]] constexpr Block const* get() const noexcept {
        return block;
    };
};

struct NumberType {
    __int64 i = 0;
    double  f = 0;

    constexpr NumberType(__int64 i, double f) noexcept : i(i), f(f) {};

    template <typename T>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
    constexpr NumberType(T v) noexcept : i(static_cast<__int64>(v)),
                                         f(static_cast<double>(v)) {}

    template <typename T>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
    constexpr NumberType& operator=(T v) noexcept {
        i = static_cast<__int64>(v);
        f = static_cast<double>(v);
        return *this;
    }
    template <typename RTN>
        requires(std::is_integral_v<RTN> && !ll::traits::is_char_v<RTN>)
    [[nodiscard]] constexpr RTN get() const noexcept {
        return static_cast<RTN>(i);
    };
    template <typename RTN>
        requires(std::is_floating_point_v<RTN>)
    [[nodiscard]] constexpr RTN get() const noexcept {
        return static_cast<RTN>(f);
    };
};

struct WorldPosType {
    Vec3 pos{};
    int  dimId = 3; // VanillaDimensions::Undefined;

    constexpr WorldPosType(Vec3 const& pos, int dimId = 3) noexcept : pos(pos), dimId(dimId) {};
    constexpr WorldPosType(std::pair<Vec3, int> const& pos) noexcept : pos(pos.first), dimId(pos.second) {};
    template <typename RTN>
    [[nodiscard]] constexpr RTN get() const noexcept = delete;
    template <>
    [[nodiscard]] constexpr Vec3 get() const noexcept {
        return pos;
    };
    template <>
    [[nodiscard]] constexpr BlockPos get() const noexcept {
        BlockPos res{};
        res.x = static_cast<int>(this->pos.x);
        res.y = static_cast<int>(this->pos.y);
        res.y = static_cast<int>(this->pos.y);
        return res;
    };
    template <>
    [[nodiscard]] constexpr std::pair<Vec3, int> get() const noexcept {
        return std::make_pair(pos, dimId);
    };
    template <>
    [[nodiscard]] constexpr std::pair<BlockPos, int> get() const noexcept {
        return std::make_pair(get<BlockPos>(), dimId);
    };
};

struct BlockPosType {
    BlockPos pos{};
    int      dimId = 3; // VanillaDimensions::Undefined;

    constexpr BlockPosType(BlockPos const& pos, int dimId = 0) noexcept : pos(pos), dimId(dimId) {};
    constexpr BlockPosType(std::pair<BlockPos, int> const& pos) noexcept : pos(pos.first), dimId(pos.second) {};
    template <typename RTN>
    [[nodiscard]] constexpr RTN get() const noexcept = delete;
    template <>
    [[nodiscard]] constexpr BlockPos get() const noexcept {
        return pos;
    };
    template <>
    [[nodiscard]] constexpr std::pair<BlockPos, int> get() const noexcept {
        return std::make_pair(pos, dimId);
    };
    template <>
    [[nodiscard]] constexpr Vec3 get() const noexcept {
        return {static_cast<float>(this->pos.x), static_cast<float>(this->pos.y), static_cast<float>(this->pos.y)};
    };
    template <>
    [[nodiscard]] constexpr std::pair<Vec3, int> get() const noexcept {
        return std::make_pair(get<Vec3>(), dimId);
    };
};

// NOLINTEND: google-explicit-constructor
} // namespace remote_call
