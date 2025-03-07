/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __ALL_TO_ALL_HH__
#define __ALL_TO_ALL_HH__

#include "astra-sim/system/CallData.hh" // 引入 CallData 头文件
#include "astra-sim/system/collective/Ring.hh" // 引入 Ring 算法基类
#include "astra-sim/system/topology/RingTopology.hh" // 引入 RingTopology

namespace AstraSim {

/**
 * @brief AllToAll 类，继承自 Ring 算法
 *        该类实现了 All-to-All 集合通信算法
 */
class AllToAll : public Ring {
  public:
    /**
     * @brief AllToAll 构造函数，初始化 All-to-All 算法
     * @param type 通信类型 (All_to_All, All_Reduce, etc.)
     * @param window 并行减少窗口大小 (-1 表示默认值)
     * @param id 进程 ID
     * @param allToAllTopology 绑定的 RingTopology（环形拓扑）
     * @param data_size 处理的数据大小
     * @param direction 数据流的方向（顺时针或逆时针）
     * @param injection_policy 数据注入策略
     */
    AllToAll(ComType type,
             int window,
             int id,
             RingTopology* allToAllTopology,
             uint64_t data_size,
             RingTopology::Direction direction,
             InjectionPolicy injection_policy);
    /**
     * @brief 运行 All-To-All 算法
     * @param event 事件类型
     * @param data 事件相关数据
     */
    void run(EventType event, CallData* data);

    /**
     * @brief 处理最大计数逻辑
     */
    void process_max_count();

    /**
     * @brief 获取非零延迟的数据包数量
     * @return 非零延迟的数据包数量
     */
    int get_non_zero_latency_packets();

    // 定义环形拓扑中的中间点
    int middle_point;
};

}  // namespace AstraSim

#endif /* __ALL_TO_ALL_HH__ */
