/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/
// 该代码基于 MIT 许可证进行开源，具体的许可证信息可在项目根目录的 LICENSE 文件中查看。

#ifndef __ASTRA_MEMORY_API_HH__
#define __ASTRA_MEMORY_API_HH__

#include <cstdint> // 引入标准整数类型库，提供固定宽度整数类型

namespace AstraSim { // 定义 AstraSim 命名空间，防止命名冲突

// 预声明 Sys 和 WorkloadLayerHandlerData 类，避免不必要的头文件依赖
class Sys;  
class WorkloadLayerHandlerData;

/**
 * @class AstraRemoteMemoryAPI
 * @brief 远程内存管理的抽象接口，用于 AstraSim 进行跨节点的张量（Tensor）管理。
 * 
 * 该类提供了一个接口，使得 AstraSim 可以模拟远程内存的访问和操作。
 * 由于是抽象基类 (接口)，子类必须实现所有的纯虚方法。
 */
class AstraRemoteMemoryAPI {
  public:
    /**
     * @brief 虚析构函数，确保子类可以正确释放资源
     */
    virtual ~AstraRemoteMemoryAPI() = default;

    /**
     * @brief 设置系统实例，绑定特定的计算节点
     * @param id 计算节点的唯一标识
     * @param sys 指向 Sys 类型的指针，代表该计算节点的系统信息
     * 
     * 该方法用于将 AstraSim 的远程内存接口与具体的计算节点关联，
     * 这样后续的远程内存操作就可以在正确的系统环境中进行。
     */
    virtual void set_sys(int id, Sys* sys) = 0;

    /**
     * @brief 向远程内存发起数据访问请求
     * @param tensor_size 需要传输的张量大小，单位为字节
     * @param wlhd 指向 WorkloadLayerHandlerData 结构的指针，包含工作负载层的信息
     * 
     * 该方法用于请求远程存储资源，以便 AstraSim 在模拟过程中进行张量传输和访问。
     * 具体实现需要由子类提供，例如模拟 RDMA（远程直接内存访问）或共享存储机制。
     */
    virtual void issue(uint64_t tensor_size,
                       WorkloadLayerHandlerData* wlhd) = 0;
};

}  // namespace AstraSim

#endif /* __ASTRA_MEMORY_API_HH__ */
