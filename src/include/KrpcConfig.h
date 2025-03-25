#pragma once
#include <unordered_map>
#include <string>


class KrpcConfig{
public:
    // 加载配置文件
    void LoadConfigFile(const char* config_file);

    // 查找配置项对应的值
    std::string Load(const std::string& key) const;

private:
    // 辅助函数，用于去除字符串前后的空格
    void Trim(std::string& read_buf);

private:
    // 存放配置项的容器
    std::unordered_map<std::string, std::string> m_configs;
};