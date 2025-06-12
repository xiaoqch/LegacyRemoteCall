#pragma once

#include "ll/api/Expected.h"
#include "ll/api/mod/NativeMod.h"
#include "remote_call/api/base/Macro.h"
#include "remote_call/api/value/DynamicValue.h"

namespace remote_call {

/**
 * @brief Type alias for a generic remote call callback function.
 *
 * This callback receives a vector of DynamicValue as arguments and returns an ll::Expected<DynamicValue>
 * as the result of the remote function execution.
 */
using CallbackFn = std::function<ll::Expected<DynamicValue>(std::vector<DynamicValue>)>;

/**
 * @brief Reference to an exported remote-callable function and its metadata.
 *
 * FunctionRef encapsulates information about an exported function, including its callback,
 * return value expectation, enabled/disabled state, and provider mod. It also provides an
 * operator() for convenient invocation with runtime arguments.
 */
struct FunctionRef {
    /**
     * @brief The callback function that handles the remote call.
     */
    CallbackFn const& callable;

    /**
     * @brief Whether the function's return value should be wrapped in ll::Expected.
     */
    bool const returnExpected;

    /**
     * @brief Whether this function is currently disabled and not callable.
     */
    bool const disabled;

    /**
     * @brief Weak pointer to the provider Mod that exported this function.
     */
    std::weak_ptr<ll::mod::Mod> const provider;

    // std::string_view nameSpace;
    // std::string_view functionName;

    /**
     * @brief Invoke the remote function with the given arguments.
     * @param args Arguments passed to the function (moved).
     * @return The result of the remote function as ll::Expected<DynamicValue>.
     */
    [[nodiscard]] inline ll::Expected<DynamicValue> operator()(std::vector<DynamicValue>&& args) const {
        return callable(std::move(args));
    }
};

/**
 * @brief Export a function for remote invocation.
 *
 * Registers a function under the given namespace and name, making it callable remotely.
 *
 * @param nameSpace    The namespace under which to export the function.
 * @param funcName     The name of the function to export.
 * @param callback     The callback function implementation.
 * @param returnExpected If true, the function is expected to return an ll::Expected<DynamicValue>.
 * @param mod          (Optional) The mod exporting the function. Defaults to the current native mod.
 * @return On success, returns a FunctionRef representing the exported function; otherwise, an error.
 * @note If a function with the same namespace and name already exists, it may be replaced or return an error depending
 * on implementation.
 */
REMOTE_CALL_API ll::Expected<FunctionRef> exportFunc(
    std::string_view            nameSpace,
    std::string_view            funcName,
    CallbackFn&&                callback,
    bool                        returnExpected,
    std::weak_ptr<ll::mod::Mod> mod = ll::mod::NativeMod::current()
);

/**
 * @brief Import a previously exported remote function by namespace and name.
 *
 * @param nameSpace        The namespace of the target function.
 * @param funcName         The name of the target function.
 * @param includeDisabled  Whether to include disabled functions in the search.
 * @return On success, returns a FunctionRef for the imported function; otherwise, an error.
 * @note If includeDisabled is false, disabled functions will not be found.
 */
REMOTE_CALL_API ll::Expected<FunctionRef>
                importFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled = false);

/**
 * @brief Check if a function is registered under the given namespace and name.
 *
 * @param nameSpace        The namespace of the target function.
 * @param funcName         The name of the target function.
 * @param includeDisabled  Whether to include disabled functions in the search.
 * @return True if the function exists (and is enabled unless includeDisabled is true), false otherwise.
 */
REMOTE_CALL_API bool hasFunc(std::string_view nameSpace, std::string_view funcName, bool includeDisabled = false);

/**
 * @brief Get the provider mod for a given exported function.
 *
 * @param nameSpace The namespace of the function.
 * @param funcName  The name of the function.
 * @return A weak pointer to the Mod that provided the function, or an empty pointer if not found.
 */
REMOTE_CALL_API std::weak_ptr<ll::mod::Mod> getProvider(std::string_view nameSpace, std::string_view funcName);

/**
 * @brief Remove a function by namespace and name.
 *
 * @param nameSpace The namespace of the function.
 * @param funcName  The name of the function.
 * @return True if the function was removed successfully, false otherwise.
 */
REMOTE_CALL_API bool removeFunc(std::string_view nameSpace, std::string_view funcName);

/**
 * @brief Remove all functions under a given namespace.
 *
 * @param nameSpace The namespace to remove.
 * @return The number of functions removed.
 */
REMOTE_CALL_API int removeNameSpace(std::string_view nameSpace);

/**
 * @brief Remove a batch of functions given by namespace and name pairs.
 *
 * @param funcs Vector of (namespace, function name) pairs to remove.
 * @return The number of functions successfully removed.
 */
REMOTE_CALL_API int removeFuncs(std::vector<std::pair<std::string, std::string>> const& funcs);

} // namespace remote_call