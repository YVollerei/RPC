#pragma once
#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>

// zookeeper客户端，主要封装一下zookeeper相关的api
class ZkClient{
public:
    ZkClient();
    ~ZkClient();

    // zk客户端启动，连接zk服务器。封装zookeeper_init
    void Start();

    // 在zk服务器中根据path新建一个节点。封装zoo_create
    void Create(const char* path, const char* data, int datalen, int state = 0);

    // 根据指定的路径，获取znode节点值。封装zoo_get
    std::string GetData(const char* path);

private:
    // zk客户端的句柄
    zhandle_t* m_zhandle;
};