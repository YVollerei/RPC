#include "KrpcConfig.h"
#include <memory>
#include <iostream>


// 加载配置文件
void KrpcConfig::LoadConfigFile(const char* config_file){
    // 使用智能指针管理文件指针，并指定删除器为fclose
    std::unique_ptr<FILE, decltype(&fclose)> pf(fopen(config_file, "r"), fclose);
    if(pf == nullptr){
        std::cout << "Error: fopen fail, file=" << config_file << std::endl;
        exit(EXIT_FAILURE);
    }

    // 逐行读取配置项
    char buf[1024];
    while(fgets(buf, 1024, pf.get()) != nullptr){
        std::string read_buf(buf);
        Trim(read_buf);
        // 跳过空行与注释行
        if(read_buf.empty() || read_buf[0] == '#'){
            continue;
        }

        // '='前为配置名，后为配置值
        int ind = read_buf.find('=');
        if(ind == -1){
            continue;
        }
        // 解析key
        std::string key = read_buf.substr(0, ind);
        Trim(key);
        // 解析value
        int endInd = read_buf.find('\n', ind);
        std::string value = read_buf.substr(ind + 1, endInd - ind - 1);
        Trim(value);

        // 插入配置项
        m_configs[key] = value;
    }

}

// 查找配置项对应的值
std::string KrpcConfig::Load(const std::string& key) const {
    auto it = m_configs.find(key);
    if(it == m_configs.end()){
        return "";
    }
    return it->second;
}

// 辅助函数，用于去除字符串前后的空格
void KrpcConfig::Trim(std::string& read_buf){
    // 去除前导空格
    int ind = read_buf.find_first_not_of(' ');
    if(ind != -1){
        read_buf = read_buf.substr(ind, read_buf.size() - ind);
    }

    // 去除尾部空格
    ind = read_buf.find_last_not_of(' ');
    if(ind != -1){
        read_buf = read_buf.substr(0, ind + 1);
    }
}