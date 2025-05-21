#pragma once

#include "mc/deps/core/math/Vec3.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "remote_call/api/base/Concepts.h"

#include <concepts>

class Block;
class Player;
class Actor;
class BlockActor;
class Container;

namespace remote_call {
// NOLINTBEGIN: google-explicit-constructor

template <class T>
    requires(std::destructible<T>)
struct MayUniquePointerType {
    T const* ptr = nullptr;
    bool     own = false;

    MayUniquePointerType(const MayUniquePointerType&) = delete;
    MayUniquePointerType(MayUniquePointerType&& o) : ptr(o.ptr), own(o.own) { o.own = false; };
    MayUniquePointerType& operator=(const MayUniquePointerType&) = delete;
    MayUniquePointerType& operator=(MayUniquePointerType&& o) {
        if (own && ptr) delete ptr;
        ptr   = o.ptr;
        own   = o.own;
        o.own = false;
        return *this;
    };
    MayUniquePointerType(std::unique_ptr<T> tag) : ptr(tag.release()), own(true) {};
    MayUniquePointerType(T const* ptr) : ptr(ptr), own(false) {};
    ~MayUniquePointerType() {
        if (own && ptr) delete ptr;
        own = false;
        ptr = nullptr;
    }
    inline std::unique_ptr<T> tryGetUniquePtr() {
        if (!own) {
            if (!ptr) return {};
            return std::make_unique<T>(*ptr); // clone
        }
        own = false;
        return std::unique_ptr<T>(const_cast<T*>(ptr));
    }
    template <typename RTN = T const*>
    inline RTN get() = delete;
    template <>
    inline T const* get() {
        return ptr;
    };
    template <>
    inline T* get() {
        return const_cast<T*>(ptr);
    };
    template <>
    inline std::unique_ptr<T> get() {
        return tryGetUniquePtr();
    };
};

struct NbtType : MayUniquePointerType<CompoundTag> {};
struct ItemType : MayUniquePointerType<ItemStack> {};

struct BlockType {
    Block const* block;
    BlockPos     blockPos;
    int          dimension;

    BlockType(Block const* ptr) : block(ptr) {
        blockPos  = BlockPos::ZERO();
        dimension = 0;
    };
    template <typename RTN>
    inline RTN get() = delete;
    template <>
    inline Block const* get() {
        return block;
    };
};

struct NumberType {
    __int64 i = 0;
    double  f = 0;

    NumberType(__int64 i, double f) : i(i), f(f) {};
    template <typename T>
        requires(std::is_integral_v<T> || std::is_floating_point_v<T>)
    NumberType& operator=(T v) {
        i = static_cast<__int64>(v);
        f = static_cast<double>(v);
    }
    NumberType(double v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(float v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(__int64 v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(int v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(short v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(char v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(unsigned __int64 v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(unsigned int v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(unsigned short v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    NumberType(unsigned char v) : i(static_cast<__int64>(v)), f(static_cast<double>(v)) {};
    template <typename RTN>
        requires(std::is_integral_v<RTN> && !ll::traits::is_char_v<RTN>)
    inline RTN get() const {
        return static_cast<RTN>(i);
    };
    template <typename RTN>
        requires(std::is_floating_point_v<RTN>)
    inline RTN get() const {
        return static_cast<RTN>(f);
    };
};

struct WorldPosType {
    Vec3 pos   = Vec3::ZERO();
    int  dimId = 3; // VanillaDimensions::Undefined;

    WorldPosType(Vec3 const& pos, int dimId = 3) : pos(pos), dimId(dimId) {};
    WorldPosType(std::pair<Vec3, int> const& pos) : pos(pos.first), dimId(pos.second) {};
    template <typename RTN>
    inline RTN get() = delete;
    template <>
    inline Vec3 get() {
        return pos;
    };
    template <>
    inline BlockPos get() {
        return BlockPos(pos);
    };
    template <>
    inline std::pair<Vec3, int> get() {
        return std::make_pair(pos, dimId);
    };
    template <>
    inline std::pair<BlockPos, int> get() {
        return std::make_pair(BlockPos(pos), dimId);
    };
};

struct BlockPosType {
    BlockPos pos   = BlockPos::ZERO();
    int      dimId = 3; // VanillaDimensions::Undefined;

    BlockPosType(BlockPos const& pos, int dimId = 0) : pos(pos), dimId(dimId) {};
    BlockPosType(std::pair<BlockPos, int> const& pos) : pos(pos.first), dimId(pos.second) {};
    template <typename RTN>
    inline RTN get() = delete;
    template <>
    inline BlockPos get() {
        return pos;
    };
    template <>
    inline std::pair<BlockPos, int> get() {
        return std::make_pair(pos, dimId);
    };
    template <>
    inline Vec3 get() {
        return pos;
    };
    template <>
    inline std::pair<Vec3, int> get() {
        return std::make_pair(pos, dimId);
    };
};

// NOLINTEND: google-explicit-constructor
} // namespace remote_call
