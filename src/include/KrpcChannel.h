#pragma once
#include <google/protobuf/service.h>
#include "zookeeperutil.h"


// 继承自google::protobuf::RpcChannel
// 目的是为了给客户端进行方法调用的时候，统一接收的
class KrpcChannel: public google::protobuf::RpcChannel{
public:
    // 构造函数
    KrpcChannel(bool connectNow);

    // 析构函数
    virtual ~KrpcChannel(){};

    // 重写继承的CallMethod
    void CallMethod(const ::google::protobuf::MethodDescriptor* method,
                    ::google::protobuf::RpcController* controller, 
                    const ::google::protobuf::Message* request,
                    ::google::protobuf::Message* response, 
                    ::google::protobuf::Closure* done) override;

private:
    // 建立与Rpc服务端的连接
    bool newConnect(const char* ip, uint16_t port);

    // 向zookeeper服务器查询服务方法对应的Rpc服务端ip和端口
    std::string QueryServiceHost(ZkClient* zkclient, const std::string& service_name, 
                                 const std::string& method_name, int& idx);

private:
    // 客户端通信的socket
    int m_clientSock;
    // 服务名
    std::string m_service_name; 
    // 方法名
    std::string m_method_name;
    // Rpc服务端的ip和端口
    std::string m_ip;
    uint16_t m_port;
    // 划分服务器ip和端口的下标
    int m_idx;
};