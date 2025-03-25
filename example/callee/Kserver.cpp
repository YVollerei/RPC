#include <iostream>
#include <string>
#include "../user.pb.h"
#include "KrpcProvider.h"
#include "KrpcApplication.h"


// 服务端上的自定义UserService，继承自proto生成的UserServiceRpc
class UserService: public Kuser::UserServiceRpc{
public:
    // 这个版本执行Login的本地任务 
    bool Login(std::string name, std::string pwd){
        std::cout << "doing local service: Login" << std::endl;
        std::cout << "name=" << name << " pwd=" << pwd << std::endl;
        return true;
    }

    // 重写UserServiceRpc的虚函数
    // 这个版本的Login执行：接收远端调用 - 调用执行本地任务 - 写入响应 - 执行回调
    void Login(::google::protobuf::RpcController* controller,
                    const ::Kuser::LoginRequest* request,
                    ::Kuser::LoginResponse* response,
                    ::google::protobuf::Closure* done){
        // 接收远端调用的参数
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 调用重载版本，执行本地任务
        bool login_result = Login(name, pwd);

        // 写入响应，参考user.proto中的定义
        Kuser::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_success(login_result);

        // 执行回调（执行响应对象数据的序列化和网络发送，交给框架来完成）
        done->Run();
    }
};


int main(int argc, char** argv){
    std::cout << "server argc:" << argc << std::endl;

    // 调用框架的初始化操作
    KrpcApplication::Init(argc, argv);

    // provider将UserService发布岛rpc节点上
    KrpcProvider provider;
    provider.NotifyService(new UserService());

    // 启动rpc服务发布节点。Run以后进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}