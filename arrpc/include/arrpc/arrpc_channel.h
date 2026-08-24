#pragma once

#include <google/protobuf/message_lite.h>

#include "arrpc_future.h"

namespace rpc
{

using RpcId = std::uint32_t;

class RpcChannel
{
public:
    virtual ~RpcChannel() = default;

    template <typename Response, typename Request>
    RpcFuture<Response> call (RpcId rpc_id, const Request& request)
    {
        return call_impl<Response> (rpc_id, request);
    }

protected:
    template <typename Response>
    RpcFuture<Response> call_impl (RpcId rpc_id, const google::protobuf::MessageLite& request)
    {
        return start_call<Response> (rpc_id, request);
    }

private:
    template <typename Response>
    RpcFuture<Response> start_call (RpcId rpc_id, const google::protobuf::MessageLite& request);
};

} // namespace rpc
