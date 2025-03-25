#include "KrpcChannel.h"
#include <mutex>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <memory>
#include "KrpcApplication.h"
#include "KrpcController.h"
#include "Krpcheader.pb.h"

// 全局的互斥锁
std::mutex g_mutex;

// 构造函数
KrpcChannel::KrpcChannel(bool connectNow)
    : m_clientSock(-1)
    , m_idx(0){
    if(!connectNow){
        return;
    }

    // 尝试与Rpc服务端进行连接，可以重试3次
    auto rt = newConnect(m_ip.c_str(), m_port);
    int cnt = 3;
    while(!rt && cnt--){
        rt = newConnect(m_ip.c_str(), m_port);
    }
}

// 重写继承的CallMethod
// 连接Rpc服务器 - 序列化请求 - 发送请求 - 接受响应 - 解析响应数据
void KrpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor* method,
                            ::google::protobuf::RpcController* controller, 
                            const ::google::protobuf::Message* request,
                            ::google::protobuf::Message* response, 
                            ::google::protobuf::Closure* done){
    // 建立与Rpc服务端的连接
    if(m_clientSock == -1){
        // 获取服务对象名和方法名
        const google::protobuf::ServiceDescriptor* sd = method->service();
        m_service_name = sd->name();
        m_method_name = method->name();

        // 向zookeeper服务器查询服务对象和方法对应的服务端host
        ZkClient zkCli;
        zkCli.Start();
        std::string server_host = QueryServiceHost(&zkCli, m_service_name, m_method_name, m_idx);

        // 解析host
        m_ip = server_host.substr(0, m_idx);
        m_port = atoi(server_host.substr(m_idx + 1, server_host.size() - m_idx).c_str());
        std::cout << "Server ip: " << m_ip << ", port: " << m_port << std::endl;
        
        // 建立连接
        bool rt = newConnect(m_ip.c_str(), m_port);
        if(rt) {
            std::cout << "connect server success" << std::endl;
        } else {
            std::cout << "connect server error" << std::endl;
            // 连接建立失败时返回
            return;
        }
    }

    // 封装KrpcHeader
    Krpc::RpcHeader krpcHeader;
    // - service_name
    krpcHeader.set_service_name(m_service_name);
    // - method_name
    krpcHeader.set_method_name(m_method_name);
    // - args_size
    uint32_t args_size{};
    std::string args_str;
    // 序列化参数到字符串
    if(request->SerializeToString(&args_str)){
        args_size = args_str.size();
    } else {
        // 设置KrpcController的错误信息
        controller->SetFailed("serialize request fail");
        return;
    }
    krpcHeader.set_args_size(args_size);

    // 将(header_size、header_str、args_size、args_str)打包到send_rpc_str
    std::string send_rpc_str;
    uint32_t header_size = 0;
    std::string rpc_header_str;
    // 序列化KrpcHeader到字符串
    if(krpcHeader.SerializeToString(&rpc_header_str)){
        header_size = rpc_header_str.size();
    } else {
        controller->SetFailed("serialize rpc header fail");
        return;
    }

    // 流式写入send_rpc_str
    {
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);
        coded_output.WriteVarint32(static_cast<uint32_t>(header_size));
        coded_output.WriteString(rpc_header_str);
    }
    send_rpc_str += args_str;

    // 发送Rpc请求
    if(send(m_clientSock, send_rpc_str.c_str(), send_rpc_str.size(), 0) == -1){
        // 发送请求失败
        close(m_clientSock);
        perror("send");
        controller->SetFailed("send error");
        return;
    }

    // 接受Rpc请求的响应
    char recv_buf[1024] = {0};
    int recv_size = recv(m_clientSock, recv_buf, sizeof(recv_buf), 0);
    if(recv_size == -1){
        perror("recv");
        controller->SetFailed("recv error");
        return;
    }

    // 反序列化解析响应数据
    if(!response->ParseFromArray(recv_buf, recv_size)){
        // 解析失败
        close(m_clientSock);
        perror("parse");
        controller->SetFailed("parse error");
        return;
    }

    // 请求-响应完毕，关闭连接
    close(m_clientSock);
}



// 建立与Rpc服务端的连接
bool KrpcChannel::newConnect(const char* ip, uint16_t port){
    // socket编程的客户端connect流程
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(clientfd == -1){
        perror("socket");
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if(connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
        perror("KrpcChannel::newConnect: connect");
        return false;
    }

    m_clientSock = clientfd;
    return true;
}

// 向zookeeper服务器查询服务方法对应的Rpc服务端host
std::string KrpcChannel::QueryServiceHost(ZkClient* zkclient, const std::string& service_name, 
                                          const std::string& method_name, int& idx){
    std::string method_path = '/' + service_name + '/' + method_name;

    // 上锁从zookeeper获取Rpc服务器host，保证多线程情况下每一个线程都能拿到信息
    std::unique_lock<std::mutex> lock(g_mutex);
    std::string server_host = zkclient->GetData(method_path.c_str());
    lock.unlock();  

    // 判断host合法性
    if(server_host == ""){
        std::cout << "ERROR: " << method_path << " is no exist! \n";
        // 不能返回空字符串，否则后续substr会出错
        return " ";
    }
    // 以":"为分隔符分隔ip和port
    idx = server_host.find(":");
    if(idx == -1){
        std::cout << "ERROR: " << method_path << " address is invalid! \n";
        return " ";
    }
    return server_host;
}