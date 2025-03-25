#include "KrpcApplication.h"
#include <unistd.h>
#include <iostream>


// 重新声明静态变量
KrpcConfig KrpcApplication::m_config;
std::mutex KrpcApplication::m_mutex;
// 懒汉模式初始化单例对象
KrpcApplication* KrpcApplication::m_application = nullptr;


// 初始化框架
void KrpcApplication::Init(int argc, char** argv){
    std::cout << "KrpcApplication::Init" << std::endl;
    if(argc < 2){
        // -i后必须有配置文件路径，文件中记录zookeeper服务器的ip和端口，以及服务器的ip和端口
        std::cout << "format::command -i <configfile>" << std::endl;
        exit(EXIT_FAILURE);
    }

    int opt;
    std::string config_file;
    // getopt用于解析命令行字符，第三个参数表示接收的参数，这里只指定i
    while(-1 != (opt = getopt(argc, argv, "i:"))){
        switch(opt){
            case 'i':{
                std::cout << "KrpcApplication: case -i" << std::endl;
                // -i表示指定配置文件路径
                // optarg为命令行参数对应的值，这里即为指定的配置文件路径
                config_file = optarg;
                break;
            }
            case '?':{
                // 不接受i以外的命令行参数
                std::cout << "format::command -i <configfile>" << std::endl;
                exit(EXIT_FAILURE);
                break;
            }
            case ':':{
                // 出现了i但后面没有对应的值
                std::cout << "format::command -i <configfile>" << std::endl;
                exit(EXIT_FAILURE);
                break;
            }
            default:
                break;
        }
    }
    // 从配置文件中载入配置项
    m_config.LoadConfigFile(config_file.c_str());
}

// 单例模式，返回单例对象
KrpcApplication& KrpcApplication::GetInstance(){
    // 获取单例时上锁
    std::lock_guard<std::mutex> lock(m_mutex);

    if(!m_application){
        m_application = new KrpcApplication();
        // 设置程序退出时自动销毁单例对象
        atexit(DeleteInstance);
    }

    return *m_application;
}

// 删除单例
void KrpcApplication::DeleteInstance(){
    if(m_application){
        delete m_application;
    }
}

// 获取配置
KrpcConfig& KrpcApplication::GetConfig(){
    return m_config;
}

