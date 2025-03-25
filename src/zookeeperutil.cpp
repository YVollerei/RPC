#include "zookeeperutil.h"
#include <mutex>
#include <condition_variable>
#include <iostream>
#include "KrpcApplication.h"


// 全局锁
std::mutex cv_mutex;
// 条件变量
std::condition_variable cv;
// 标记zk客户端是否已经连接到zk服务器
bool isConnected = false;

// 全局的watcher观察器，当节点发生变化时，zk服务端会通过该回调函数通知zk客户端
void global_watcher(zhandle_t* zh, int type, int state, const char* path, void* watcher_ctx){
    // 只处理 type==ZOO_SESSION_EVENT && state==ZOO_CONNECTED_STATE 的watcher事件
    if(type == ZOO_SESSION_EVENT){
        if(state == ZOO_CONNECTED_STATE){
            std::lock_guard<std::mutex> lock(cv_mutex);
            // 标记zk客户端已经与zk服务端建立连接
            isConnected = true;
        }
    }
    // 唤醒条件变量
    cv.notify_all();
}

// 构造函数，初始化zk客户端句柄
ZkClient::ZkClient(): m_zhandle(nullptr){
}

// 析构函数，关闭zk客户端句柄
ZkClient::~ZkClient(){
    if(m_zhandle != nullptr){
        std::cout << "ZkClient: close" << std::endl;
        zookeeper_close(m_zhandle);
    }
}

// zk客户端启动，连接zk服务器。封装zookeeper_init
void ZkClient::Start(){
    // 从配置文件中获取zookeeper服务器的ip和端口
    std::string host = KrpcApplication::GetInstance().GetConfig().Load("zookeeperip");
    std::string port = KrpcApplication::GetInstance().GetConfig().Load("zookeeperport");
    // 拼接 ip + port
    std::string conn_str = host + ':' + port;

    // 初始化zk对象，异步建立zk客户端（即Rpc服务端）与zk服务器的连接
    m_zhandle = zookeeper_init(conn_str.c_str(), global_watcher, 6000, nullptr, nullptr, 0);
    if(!m_zhandle){
        // 初始化失败
        std::cout << "zookeeper_init error!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // 等待global_watcher回调通知连接已经建立完成(isConnected == true)
    std::unique_lock<std::mutex> lock(cv_mutex);
    // 第二个参数用于防止虚假唤醒
    cv.wait(lock, []{return isConnected;});
    std::cout << "zookeeper_init success!" << std::endl;
}

// 在zk服务器中根据path新建一个节点。封装zoo_create
void ZkClient::Create(const char* path, const char* data, int datalen, int state){
    // 创建znode节点，可以选择永久性节点还是临时性节点
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);

    // 检查指定的节点是否存在，只有不存在时才创建节点
    int flag = zoo_exists(m_zhandle, path, 0, nullptr);
    // if(flag != ZNONODE){
    //     // 测试用：若节点存在则强制删除节点
    //     std::cout << "znode existed, delete node:" << path << std::endl;
    //     int rt = zoo_delete(m_zhandle, path, -1);
    //     if(rt != ZOK){
    //         std::cout << "delete node fail" << std::endl;
    //     }
    // }
    if(flag == ZNONODE){
        // 创建指定path的znode节点
        flag = zoo_create(m_zhandle, path, data, datalen, &ZOO_OPEN_ACL_UNSAFE, state, path_buffer, bufferlen);
        if(flag == ZOK){
            std::cout << "znode create success, path=" << path << std::endl;
        } else {
            std::cout << "znode create fail, path=" << path << std::endl;
            exit(EXIT_FAILURE);
        }
    } else {
        std::cout << "znode existed, path=" << path << std::endl;
    }
}

// 根据指定的路径，获取znode节点值。封装zoo_get
std::string ZkClient::GetData(const char* path){
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle, path, 0, buffer, &bufferlen, nullptr);
    if(flag == ZOK){
        return buffer;
    } else {
        std::cout << "zoo_get error!" << std::endl;
        return "";
    }

    return "";
}