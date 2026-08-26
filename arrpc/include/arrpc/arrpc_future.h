#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include "arrpc_status.h"

namespace rpc
{

template <typename T>
class RpcResult
{
public:
    static RpcResult success (T value)
    {
        RpcResult result;
        result.value_ = std::move (value);
        result.status_ = RpcStatus::ok_status();
        return result;
    }

    static RpcResult failure (RpcStatus status)
    {
        RpcResult result;
        result.status_ = std::move (status);
        return result;
    }

    [[nodiscard]]
    bool ok() const noexcept
    {
        return status_.ok();
    }

    [[nodiscard]]
    const T& value() const
    {
        return *value_;
    }

    [[nodiscard]]
    T&& take_value()
    {
        return std::move (*value_);
    }

    [[nodiscard]]
    const RpcStatus& status() const noexcept
    {
        return status_;
    }

private:
    std::optional<T> value_;
    RpcStatus status_;
};

namespace detail
{

    template <typename T>
    class FutureState
    {
    public:
        using Result = RpcResult<T>;
        using Callback = std::function<void (Result)>;

        void complete (Result result)
        {
            Callback callback;

            if (completed_)
            {
                return;
            }

            result_ = std::move (result);
            completed_ = true;

            callback = std::move (callback_);

            if (callback)
            {
                callback (std::move (*result_));
            }
        }

        void set_callback (Callback callback)
        {
            std::optional<Result> result;

            if (completed_)
            {
                result = *result_;
            }
            else
            {
                callback_ = std::move (callback);
                return;
            }

            callback (std::move (*result));
        }

        bool completed() const { return completed_; }

    private:
        bool completed_ = false;

        std::optional<Result> result_;
        Callback callback_;
    };

} // namespace detail

template <typename T>
class RpcFuture
{
public:
    using State = detail::FutureState<T>;
    using Result = RpcResult<T>;
    using Callback = typename State::Callback;

    RpcFuture() = default;

    explicit RpcFuture (std::shared_ptr<State> state) : state_ (std::move (state)) {}

    RpcFuture (const RpcFuture&) = default;
    RpcFuture& operator= (const RpcFuture&) = default;

    RpcFuture (RpcFuture&&) noexcept = default;
    RpcFuture& operator= (RpcFuture&&) noexcept = default;

    void then (Callback callback)
    {
        if (! state_)
        {
            return;
        }

        state_->set_callback (std::move (callback));
    }

    void cancel()
    {
        if (state_)
        {
            state_->cancel();
        }
    }

    [[nodiscard]]
    bool valid() const noexcept
    {
        return state_ != nullptr;
    }

private:
    std::shared_ptr<State> state_;
};

template <typename T>
class RpcPromise
{
    using Future = RpcFuture<T>;
    using Callback = Future::Callback;
    using State = Future::State;

public:
    RpcPromise() : state_ (std::make_shared<State>()) {}
    explicit RpcPromise (std::shared_ptr<State> state) : state_ (std::move (state)) {}
    explicit RpcPromise (const RpcPromise<T>& copy) : state_ (copy.state_) {}
    explicit RpcPromise (const RpcPromise<T>&& copy) : state_ (std::move (copy.state_)) {}

    void result (Future::Result& result) { state_->complete (std::move (result)); }
    void success (T t) { state_->complete (Future::Result::success (t)); }
    void failure (RpcStatus status) { state_->complete (Future::Result::failure (status)); }

    Future future() { return Future (state_); }

private:
    std::shared_ptr<State> state_;
};

} // namespace rpc
