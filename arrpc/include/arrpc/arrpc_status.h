#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace rpc
{

struct RpcStatus
{
    enum class Code : std::uint32_t
    {
        Ok = 0,

        Cancelled,
        InvalidArgument,
        NotFound,
        AlreadyExists,
        PermissionDenied,
        Unavailable,
        DeadlineExceeded,

        ProtocolError,
        SerializationError,
        UnknownMethod,
        Internal,
    };

    Code code = Code::Ok;
    std::string message;

    [[nodiscard]]
    bool ok() const noexcept
    {
        return code == Code::Ok;
    }

    static RpcStatus ok_status() { return {}; }

    static RpcStatus error (Code code, std::string message)
    {
        return { code, std::move (message) };
    }
};

} // namespace rpc
