/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ASTRA_NETWORK_API_HH__
#define __ASTRA_NETWORK_API_HH__

#include "astra-sim/system/Common.hh"

namespace AstraSim {

// AstraNetworkAPI：定义了AstraSim网络接口的抽象类
class AstraNetworkAPI {
  public:
    // 后端类型枚举，表示使用哪种网络模拟器或分析工具
    enum class BackendType { NotSpecified = 0, Garnet, NS3, Analytical };

    // 构造函数，初始化网络的rank（节点标识）
    AstraNetworkAPI(int rank) : rank(rank) {};

    // 虚析构函数，确保派生类能正确析构
    virtual ~AstraNetworkAPI() {};

    // 发送消息的接口，纯虚函数，需要子类实现
    // buffer: 发送数据的缓冲区
    // count: 数据数量
    // type: 数据类型标识
    // dst: 目的节点的rank
    // tag: 消息标签，用于区分消息
    // request: 请求状态的指针
    // msg_handler: 消息处理完成后的回调函数
    // fun_arg: 回调函数的参数
    virtual int sim_send(void* buffer,
                         uint64_t count,
                         int type,
                         int dst,
                         int tag,
                         sim_request* request,
                         void (*msg_handler)(void* fun_arg),
                         void* fun_arg) = 0;

    // 接收消息的接口，纯虚函数，需要子类实现
    // 参数含义同sim_send，但src为消息源节点的rank
    virtual int sim_recv(void* buffer,
                         uint64_t count,
                         int type,
                         int src,
                         int tag,
                         sim_request* request,
                         void (*msg_handler)(void* fun_arg),
                         void* fun_arg) = 0;

    /*
     * sim_schedule用于在网络后端安排一个事件。
     * delta: 事件相对当前时间的延迟。
     * fun_ptr: 事件触发时调用的函数指针。
     * fun_arg: 传入事件处理函数的参数。
     */
    virtual void sim_schedule(timespec_t delta,
                              void (*fun_ptr)(void* fun_arg),
                              void* fun_arg) = 0;

    // 获取网络后端类型，默认为未指定
    virtual BackendType get_backend_type() {
        return BackendType::NotSpecified;
    };

    // 获取当前节点的rank标识
    virtual int sim_comm_get_rank() {
        return rank;
    };

    // 设置当前节点的rank标识，并返回设置后的rank
    virtual int sim_comm_set_rank(int rank) {
        this->rank = rank;
        return this->rank;
    };

    // 获取当前模拟的绝对时间（虚函数，需子类实现）
    virtual timespec_t sim_get_time() = 0;

    // 获取指定维度上的带宽，默认实现返回-1，子类可根据需求重写
    virtual double get_BW_at_dimension(int dim) {
        return -1;
    };

    // 通知网络后端当前节点工作负载已完成
    // 每个rank有一个网络处理器实现，当实现此函数时，需确认所有rank均已完成
    virtual void sim_notify_finished(){
        return;
    }

    // 节点标识(rank)
    int rank;
};

}  // namespace AstraSim

#endif /* __ASTRA_NETWORK_API_HH__ */