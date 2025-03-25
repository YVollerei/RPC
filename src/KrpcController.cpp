#include "KrpcController.h"

KrpcController::KrpcController()
    : m_failed(false)
    , m_errmsg(""){
}

// 重置状态与错误信息
void KrpcController::Reset(){
    m_failed = false;
    m_errmsg = "";
}

// 设置状态与错误信息
void KrpcController::SetFailed(const std::string& reason){
    m_failed = true;
    m_errmsg = reason;
}

// 下面是一些还未实现的功能
void KrpcController::StartCancel(){

}
bool KrpcController::IsCanceled() const {
    return false;
}
void KrpcController::NotifyOnCancel(google::protobuf::Closure* callback){

}