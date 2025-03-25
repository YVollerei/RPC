#pragma once
#include "KrpcConfig.h"
#include "KrpcChannel.h"
#include "KrpcController.h"
#include <mutex>


// Krpc基础类，负责框架的初始化操作
class KrpcApplication{
public:
    // 初始化框架
    static void Init(int argc, char** argv);

    // 单例模式，返回单例对象
    static KrpcApplication& GetInstance();
    // 删除单例
    static void DeleteInstance();

    // 获取配置
    static KrpcConfig& GetConfig();

private:
    // 单例模式，私有的构造函数
    KrpcApplication(){};
    ~KrpcApplication(){};

    // 删除拷贝与移动构造
    KrpcApplication(const KrpcApplication&) = delete;
    KrpcApplication(KrpcApplication&&) = delete;

private:
    // 配置
    static KrpcConfig m_config;

    // 单例对象
    static KrpcApplication* m_application;

    // 互斥量
    static std::mutex m_mutex;
};
