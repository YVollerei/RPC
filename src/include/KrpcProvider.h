#pragma once
#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <google/protobuf/descriptor.h>
#include <functional>
#include <string>
#include <unordered_map>

class KrpcProvider{
public:
    ~KrpcProvider();

    // 提供给外部使用，用于发布rpc方法
    // 多态：所有服务都继承自google::protobuf::Service
    void NotifyService(google::protobuf::Service* service);

    // 启动Rpc服务节点，开始提供Rpc远程调用服务
    void Run();

private:
    // 服务结构体，用于保存具体的服务对象和它的方法
    struct ServiceInfo{
        google::protobuf::Service* service;
        std::unordered_map<std::string, const google::protobuf::MethodDescriptor*> method_map;
    };

    // 处理新连接
    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    // 处理已有连接上发来的消息
    void OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receive_time);

    // 发送Rpc响应
    void SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response);

private:
    muduo::net::EventLoop m_eventloop;

    // 保存服务对象的容器
    std::unordered_map<std::string, ServiceInfo> m_services;
};