#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include <google/protobuf/message_lite.h>

#include "arrpc_status.h"

namespace rpc
{

using RequestId = std::uint64_t;

class RpcServerConnection
{
public:
    virtual ~RpcServerConnection() = default;

    virtual void respond (RequestId request_id,
                          RpcStatus status,
                          const google::protobuf::MessageLite* message) = 0;

    void respond_error (RequestId request_id, RpcStatus status)
    {
        respond (request_id, std::move (status), nullptr);
    }
};

template <typename T>
class RpcResponder
{
public:
    RpcResponder() = default;

    RpcResponder (RpcServerConnection& connection, RequestId request_id) :
        connection_ (&connection), request_id_ (request_id)
    {
    }

    RpcResponder (const RpcResponder&) = delete;
    RpcResponder& operator= (const RpcResponder&) = delete;

    RpcResponder (RpcResponder&& other) noexcept :
        connection_ (other.connection_),
        request_id_ (other.request_id_),
        completed_ (other.completed_)
    {
        other.connection_ = nullptr;
        other.completed_ = true;
    }

    RpcResponder& operator= (RpcResponder&& other) noexcept
    {
        if (this != &other)
        {
            connection_ = other.connection_;
            request_id_ = other.request_id_;
            completed_ = other.completed_;

            other.connection_ = nullptr;
            other.completed_ = true;
        }

        return *this;
    }

    ~RpcResponder()
    {
        // Deliberately don't automatically send an error.
        //
        // A service implementation is allowed to abandon
        // a request because the underlying connection went away.
    }

    void success (const T& value)
    {
        if (completed_ || connection_ == nullptr)
        {
            return;
        }

        connection_->respond (request_id_, RpcStatus::ok_status(), &value);

        completed_ = true;
    }

    void success (T&& value) { success (value); }

    void error (RpcStatus status)
    {
        if (completed_ || connection_ == nullptr)
        {
            return;
        }

        connection_->respond (request_id_, std::move (status), nullptr);

        completed_ = true;
    }

    void cancel() { error (RpcStatus::error (RpcStatus::Code::Cancelled, "RPC cancelled")); }

    [[nodiscard]]
    RequestId request_id() const noexcept
    {
        return request_id_;
    }

    [[nodiscard]]
    bool completed() const noexcept
    {
        return completed_;
    }

private:
    RpcServerConnection* connection_ = nullptr;
    RequestId request_id_ = 0;
    bool completed_ = true;
};

} // namespace rpc
