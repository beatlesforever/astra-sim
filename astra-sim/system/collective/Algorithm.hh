/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALGORITHM_HH__
#define __ALGORITHM_HH__

#include "astra-sim/system/BaseStream.hh"
#include "astra-sim/system/CallData.hh"
#include "astra-sim/system/Callable.hh"
#include "astra-sim/system/topology/LogicalTopology.hh"

namespace AstraSim {

/**
 * @brief Algorithm 是 AstraSim 中所有集合通信算法的基类
 *        提供基本的生命周期管理和通用接口
 */
class Algorithm : public Callable {
  public:

    /**
     * @brief 支持的集合通信算法名称
     */
    enum class Name { Ring = 0, DoubleBinaryTree, AllToAll, HalvingDoubling };

    /**
     * @brief 默认构造函数，初始化 Algorithm
     */
    Algorithm();

    /**
     * @brief 虚析构函数，确保派生类可以正确释放资源
     */
    virtual ~Algorithm() = default;

    /**
     * @brief 运行集合通信算法，由具体算法实现
     * @param event 事件类型
     * @param data 事件相关数据
     */
    virtual void run(EventType event, CallData* data) = 0;

    /**
     * @brief 初始化算法对象
     * @param stream 绑定的 BaseStream 数据流对象
     */
    virtual void init(BaseStream* stream);

    /**
     * @brief 处理事件回调
     * @param event 事件类型
     * @param data 事件相关数据
     */
    virtual void call(EventType event, CallData* data);

    /**
     * @brief 退出算法并通知系统继续执行
     */
    virtual void exit();

    // 算法名称
    Name name;

    // 算法的唯一标识符
    int id;

    // 绑定的数据流
    BaseStream* stream;

    // 逻辑拓扑结构，用于管理数据流在集群中的传输
    LogicalTopology* logical_topo;

    // 处理的数据大小
    uint64_t data_size;

    // 归约后的最终数据大小
    uint64_t final_data_size;

    // 通信类型，例如 All-Reduce, All-Gather 等
    ComType comType;

    // 算法是否启用
    bool enabled;
};

}  // namespace AstraSim

#endif /* __ALGORITHM_HH__ */
