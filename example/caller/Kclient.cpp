#include "KrpcApplication.h"
#include "KrpcChannel.h"
#include "../user.pb.h"
#include <iostream>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>


// Rpc客户端远端调用Rpc服务端的服务方法
void send_request(int thread_id, std::atomic<int>& success_cnt, std::atomic<int>& fail_cnt){
    Kuser::UserServiceRpc_Stub stub(new KrpcChannel(false));

    // 设置Rpc请求参数
    Kuser::LoginRequest request;
    request.set_name("yu");
    request.set_pwd("123456");
    // 创建Rpc响应参数
    Kuser::LoginResponse response;

    // 远程调用Login方法
    KrpcController controller;
    // 这里Login实际就是通过KrpcChannel::CallMethod间接调用的
    stub.Login(&controller, &request, &response, nullptr);
    // 读取响应
    if(controller.Failed()){
        // 调用失败
        std::cout << controller.ErrorText() << std::endl;
    } else {
        if(response.result().errcode() == 0){
            std::cout << "Rpc login response success: " << response.success() << std::endl;
            ++success_cnt;
        } else {
            std::cout << "Rpc login response success: " << response.result().errmsg() << std::endl;
            ++fail_cnt;
        }
    }
}


int main(int argc, char** argv){
    // 程序启动后，调用KrpcApplication类进行初始化
    KrpcApplication::Init(argc, argv);

    // 设置线程配置属性
    const int thread_cnt = 1000;         // 并发线程数
    const int request_per_thread = 10;  // 每个线程发送的请求数

    std::vector<std::thread> threads;
    std::atomic<int> success_cnt(0);
    std::atomic<int> fail_cnt(0);

    // send_request(0, success_cnt, fail_cnt);

    // 获取程序开始时间
    auto start_time = std::chrono::high_resolution_clock::now();

    // 启动多线程并发测试
    for(int i = 0; i < thread_cnt; ++i){
        threads.emplace_back([argc, argv, i, &success_cnt, & fail_cnt](){
            for(int j = 0; j < request_per_thread; ++j){
                send_request(i, success_cnt, fail_cnt);
            }
        });
    }

    // 阻塞等待所有线程执行完成
    for(auto& t : threads){
        t.join();
    }

    // 获取程序结束时间，并计算程序执行时间
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    // 输出统计结果
    std::cout << "Total requests: " << thread_cnt * request_per_thread << std::endl;
    std::cout << "Success count: " << success_cnt << std::endl;
    std::cout << "Fail count: " << fail_cnt << std::endl;
    std::cout << "Elapsed time: " << elapsed.count() << " seconds" << std::endl;
    std::cout << "QPS: " << (thread_cnt * request_per_thread) / elapsed.count() << std::endl;  

    return 0;
}
