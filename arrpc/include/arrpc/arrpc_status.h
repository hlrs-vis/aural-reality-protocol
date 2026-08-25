#pragma once

#include <arrpc/message_header.pb.h>
#include <string>
#include <utility>

namespace rpc
{

struct RpcStatus
{
    using Code = auralreality::StatusCode;

    Code code = Code::Ok;
    std::string message;

    [[nodiscard]]
    bool ok() const noexcept
    {
        return code == Code::Ok;
    }

    static RpcStatus ok_status() { return {}; }

    static RpcStatus error (auralreality::StatusCode code, std::string message)
    {
        return { code, std::move (message) };
    }
};

} // namespace rpc
