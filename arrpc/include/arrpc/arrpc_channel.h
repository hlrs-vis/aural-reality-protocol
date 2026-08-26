#pragma once

#include <functional>
#include <google/protobuf/message_lite.h>

#include "arrpc/arrpc_status.h"
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
        std::string out;
        RpcPromise<Response> promise;

        if (! request.SerializeToString (&out))
        {
            promise.failure (RpcStatus::error (RpcStatus::Code::SerializationError,
                                               "Failed to serialize request."));
            return promise.future();
        }

        perform_request (rpc_id, out)
            .then (std::bind (
                [] (RpcPromise<Response>& promise, RpcResult<std::string> result)
                {
                    if (result.ok())
                    {
                        std::string data = result.take_value();
                        Response res;
                        if (! res.ParseFromString (data))
                        {
                            promise.failure (RpcStatus::error (RpcStatus::Code::SerializationError,
                                                               "Failed to parse response."));
                        }
                        else
                        {
                            promise.success (std::move (res));
                        }
                    }
                    else
                    {
                        promise.failure (result.status());
                    }
                },
                std::move (promise),
                std::placeholders::_1));

        return promise.future();
    }

private:
    virtual RpcFuture<std::string> perform_request (RpcId rpc_id, std::string_view data) = 0;
};

} // namespace rpc
