/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __WORKLOAD_LAYER_HANDLER_DATA_HH__
#define __WORKLOAD_LAYER_HANDLER_DATA_HH__

#include "astra-sim/common/AstraNetworkAPI.hh"
#include "astra-sim/system/BasicEventHandlerData.hh"

namespace AstraSim {

class Workload;

/**
 * @class WorkloadLayerHandlerData
 * @brief 该类用于存储工作负载（Workload）层的元数据，并继承基础事件处理类。
 * 
 * WorkloadLayerHandlerData 主要用于在 AstraSim 模拟环境中传递与
 * 工作负载相关的信息，例如系统 ID（sys_id）、工作负载指针（workload）
 * 和节点 ID（node_id）。
 */
class WorkloadLayerHandlerData : public BasicEventHandlerData, public MetaData {
  public:
    int sys_id;           ///< 系统 ID，用于唯一标识该 Workload 所属的系统
    Workload* workload;   ///< 指向 Workload 实例的指针，存储当前的计算任务
    uint64_t node_id;     ///< 该 Workload 所属的计算节点 ID

    /**
     * @brief WorkloadLayerHandlerData 的构造函数
     * 
     * 初始化 `node_id` 为 0，并继承 BasicEventHandlerData 和 MetaData。
     */
    WorkloadLayerHandlerData();
};

}  // namespace AstraSim

#endif /* __WORKLOAD_LAYER_HANDLER_DATA_HH__ */
