#include "KrpcProvider.h"
#include "KrpcApplication.h"
#include "Krpcheader.pb.h"
#include <iostream>


KrpcProvider::~KrpcProvider(){
    // 析构函数，停止Event Loop
    std::cout << "~KrpcProvider()" << std::endl;
    m_eventloop.quit();
}

// 提供给外部使用，用于发布rpc方法（将传入的服务保存到容器中，等待之后调用Run发布到zookeeper服务器上）
// 多态：所有服务都继承自google::protobuf::Service
void KrpcProvider::NotifyService(google::protobuf::Service* service){
    // 存放服务对象及其方法的结构体
    ServiceInfo service_info;

    // 利用多态返回服务类的描述信息
    const google::protobuf::ServiceDescriptor* psd = service->GetDescriptor();

    // 获取服务的名字
    std::string server_name = psd->name();
    // 获取服务对象下的方法数量
    int method_cnt = psd->method_count();

    std::cout << "KrpcProvider::NotifyService: service_name=" << server_name << ", method_cnt=" << method_cnt << std::endl;

    // 将服务对象的方法都存入service_info
    for(int i = 0; i < method_cnt; ++i){
        const google::protobuf::MethodDescriptor* pmd = psd->method(i);
        // 获取方法名
        std::string method_name = pmd->name();
        std::cout << "KrpcProvider::NotifyService: method_name: " << method_name << std::endl;

        service_info.method_map.emplace(method_name, pmd);
    }

    service_info.service = service;
    // 将服务对象放入容器进行管理
    m_services.emplace(psd->name(), service_info);
}

// 启动Rpc服务节点，将Rpc服务注册到zookeeper上，开始提供Rpc远程调用服务
void KrpcProvider::Run(){
    // 从配置文件中读取Rpc服务端的ip和端口
    std::string ip = KrpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    int port = atoi(KrpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    // 创建地址
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    auto server = std::make_shared<muduo::net::TcpServer>(&m_eventloop, address, "KrpcProvider");

    // 分别绑定连接事件和消息事件
    server->setConnectionCallback(std::bind(&KrpcProvider::OnConnection, this, std::placeholders::_1));
    server->setMessageCallback(std::bind(&KrpcProvider::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    // 设置线程数量为4
    server->setThreadNum(4);

    // 把当前Rpc服务端的服务全部注册到zookeeper上，使得Rpc客户端能够从zookeeper上发现服务
    ZkClient zkclient;
    zkclient.Start();
    std::cout << "KrpcProvider: zkclient Start success!" << std::endl;
    // service_name为永久节点，method_name为临时节点
    for(auto& service: m_services){
        // service_name的路径: /service_name
        std::string service_path = "/" + service.first;
        std::cout << "KrpcProvider: zkclient Create znode: " << service_path << std::endl;
        zkclient.Create(service_path.c_str(), nullptr, 0);

        // 创建method_name节点
        for(auto& method: service.second.method_map){
            std::string method_path = service_path + "/" + method.first;
            char method_path_data[128] = {0};
            // 写入节点内容：ip + 端口
            sprintf(method_path_data, "%s:%d", ip.c_str(), port);
            // zookeeper上创建临时节点
            zkclient.Create(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }

    // Rpc服务端准备启动
    std::cout << "RpcProvider start service at ip:" << ip << ", port:" << port << std::endl;

    // 启动muduo网络服务
    server->start();
    m_eventloop.loop();
}

// 处理新连接
void KrpcProvider::OnConnection(const muduo::net::TcpConnectionPtr& conn){
    // 不做特别处理
    if(!conn->connected()){
        conn->shutdown();
    }
}

// 处理已有连接上发来的消息
// OnMessage的主要工作：反序列化解析参数（处理粘包） -> 根据参数找到对应的服务与方法 -> 生成响应
// 字节流的的一般格式：
//   header_size: 4字节，记录header_str的长度
//   header_str: 记录服务名、方法名、参数长度
//   arg_str: 用于调用方法的参数
void KrpcProvider::OnMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buffer, muduo::Timestamp receive_time){
    std::cout << "OnMessage" << std::endl;

    // 接收远端调用的字节流
    std::string recv_buf = buffer->retrieveAllAsString();
    // ArrayInputStream: 将字节流包装为一个可读取的输入溜
    google::protobuf::io::ArrayInputStream raw_input(recv_buf.data(), recv_buf.size());
    // CodedInputStream: 提供​高效的二进制流解析工具
    google::protobuf::io::CodedInputStream coded_input(&raw_input);

    // 读取4字节的header_size
    uint32_t header_size{};
    coded_input.ReadVarint32(&header_size);

    // 根据header_size的值读取header_str，并对其反序列化，得到Rpc请求的详细信息（既服务名、方法名、参数大小）
    std::string rpc_header_str;
    Krpc::RpcHeader krpcHeader;
    std::string service_name;
    std::string method_name;
    uint32_t args_size{};
    // 设置读取规则，读取header_str
    google::protobuf::io::CodedInputStream::Limit msg_limit = coded_input.PushLimit(header_size);
    coded_input.ReadString(&rpc_header_str, header_size);
    // 恢复规则，便于之后安全地读取其他数据
    coded_input.PopLimit(msg_limit);
    // 反序列化，解析KrpcHeader
    if(krpcHeader.ParseFromString(rpc_header_str)){
        service_name = krpcHeader.service_name();
        method_name = krpcHeader.method_name();
        args_size = krpcHeader.args_size();
    } else {
        std::cout << "Error: krpcHeader parse error" << std::endl;
        return;
    }

    // 用于调用Rpc方法的参数
    std::string args_str;
    // 读取args_size长度的字符串
    if(!coded_input.ReadString(&args_str, args_size)){
        std::cout << "Error: read args error" << std::endl;
        return;
    }

    // 在Rpc服务端中搜索service对象和method对象
    auto sit = m_services.find(service_name);
    if(sit == m_services.end()){
        std::cout << service_name << " is no exist!" << std::endl;
        return;
    }
    auto mit = sit->second.method_map.find(method_name);
    if(mit == sit->second.method_map.end()){
        std::cout << method_name << " is no exist!" << std::endl;
        return;
    }
    // 获取服务对象与方法对象
    google::protobuf::Service* service = sit->second.service;
    const google::protobuf::MethodDescriptor* method = mit->second;

    // 生成Rpc方法调用的请求（request）和响应（response）
    // 通过 GetRequestPrototype，可以根据方法描述符动态获取对应的请求消息类型，并New（）实例化该类型的对象
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(args_str)){
        std::cout << service_name << '.' << method_name << " parse error!" << std::endl;
        return;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();

    // NewCallback函数会返回一个google::protobuf::Closure类的对象
    // Closure类对象相当于一个闭包，它捕获了一个成员对象的成员函数(SendRpcResponse)，以及这个成员函数需要的参数(conn、response)）
    google::protobuf::Closure* done = google::protobuf::NewCallback<KrpcProvider,
                                                                    const muduo::net::TcpConnectionPtr&,
                                                                    google::protobuf::Message*>(this, &KrpcProvider::SendRpcResponse,
                                                                                                conn,
                                                                                                response);

    // CallMethod在UserServiceRpc实现，功能为根据远端Rpc请求，调用当前Rpc节点上发布的方法
    // request与response中包含了调用method的参数，done是执行完method后的回调函数，这里指定了SendRpcResponse
    service->CallMethod(method, nullptr, request, response, done);
}

// 发送Rpc响应，作为回调函数
void KrpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr& conn, google::protobuf::Message* response){
    // 序列化响应字符串，并将其发送给Rpc调用方
    std::string response_str;
    if(response->SerializeToString(&response_str)){
        conn->send(response_str);
    } else {
        std::cout << "Serialize Error!" << std::endl;
    }
}
