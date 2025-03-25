#pragma once
#include <google/protobuf/service.h>
#include <string>

// 描述Rpc调用的控制器，主要作用是跟踪RPC方法调用的状态、错误信息并提供控制功能（如取消调用）
class KrpcController: public google::protobuf::RpcController{
public:
    KrpcController();

    // 重置状态与错误信息
    void Reset() override;

    // 返回当前状态是否为错误
    bool Failed() const override { return m_failed; }

    // 返回错误信息
    std::string ErrorText() const override { return m_errmsg; }

    // 设置状态与错误信息
    void SetFailed(const std::string& reason) override;

    // 下面是一些还未实现的功能
    void StartCancel() override;
    bool IsCanceled() const override;
    void NotifyOnCancel(google::protobuf::Closure* callback) override;

private:
    // RPC方法执行过程中的状态
    bool m_failed;

    // RPC方法执行过程中的错误信息
    std::string m_errmsg;
};