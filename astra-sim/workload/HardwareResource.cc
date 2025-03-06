/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

// TODO: HardwareResource.cc should be moved to the system layer.

#include "astra-sim/workload/HardwareResource.hh"

using namespace std;
using namespace AstraSim;
using namespace Chakra;

typedef ChakraProtoMsg::NodeType ChakraNodeType; // 定义 Chakra 的节点类型别名

/**
 * @brief 构造函数，初始化硬件资源管理器
 * 
 * @param num_npus 计算资源中的 NPU 数量
 */
HardwareResource::HardwareResource(uint32_t num_npus)
    : num_npus(num_npus),
      num_in_flight_cpu_ops(0), // 当前正在执行的 CPU 操作数
      num_in_flight_gpu_comm_ops(0), // 当前正在执行的 GPU 通信操作数
      num_in_flight_gpu_comp_ops(0) { // 当前正在执行的 GPU 计算操作数

    num_cpu_ops = 0; // 初始化 CPU 任务计数
    num_gpu_ops = 0; // 初始化 GPU 计算任务计数
    num_gpu_comms = 0; // 初始化 GPU 通信任务计数

    tics_cpu_ops = 0; // 初始化 CPU 任务执行的时钟周期
    tics_gpu_ops = 0; // 初始化 GPU 计算任务执行的时钟周期
    tics_gpu_comms = 0; // 初始化 GPU 通信任务执行的时钟周期

    cpu_ops_node = NULL; // 初始化 CPU 计算任务的节点指针
    gpu_ops_node = NULL; // 初始化 GPU 计算任务的节点指针
    gpu_comms_node = NULL; // 初始化 GPU 通信任务的节点指针
}

/**
 * @brief 占用计算资源
 * 
 * @param node 需要占用的计算任务节点
 */
void HardwareResource::occupy(const shared_ptr<Chakra::ETFeederNode> node) {
    if (node->is_cpu_op()) { // 处理 CPU 计算任务
        assert(num_in_flight_cpu_ops == 0); // 确保没有其他 CPU 计算任务正在执行
        ++num_in_flight_cpu_ops; // 标记 CPU 任务正在执行
        ++num_cpu_ops; // 记录 CPU 任务的总数
    } else {  // 处理 GPU 相关任务
        if (node->type() == ChakraNodeType::COMP_NODE) {  // 判断是否是 GPU 计算任务
            assert(num_in_flight_gpu_comp_ops == 0); // 确保 GPU 计算任务互斥
            ++num_in_flight_gpu_comp_ops; // 标记 GPU 计算任务正在执行
            ++num_gpu_ops; // 记录 GPU 计算任务的总数
            gpu_ops_node = node; // 记录当前的 GPU 计算任务节点
        } else { // 否则是 GPU 通信任务
            if (node->type() == ChakraNodeType::COMM_RECV_NODE) { // 如果是通信接收任务
                return; // 通信接收任务不占用资源，直接返回
            }
            assert(num_in_flight_gpu_comm_ops == 0); // 确保 GPU 通信任务互斥
            ++num_in_flight_gpu_comm_ops; // 标记 GPU 通信任务正在执行
            ++num_gpu_comms; // 记录 GPU 通信任务的总数
            gpu_comms_node = node; // 记录当前的 GPU 通信任务节点
        }
    }
}

/**
 * @brief 释放计算资源
 * 
 * @param node 需要释放的计算任务节点
 */
void HardwareResource::release(const shared_ptr<Chakra::ETFeederNode> node) {
    if (node->is_cpu_op()) { // 判断是否是 CPU 计算任务
        --num_in_flight_cpu_ops; // 释放 CPU 任务
        assert(num_in_flight_cpu_ops == 0); // 释放后 CPU 任务数必须为 0
    } else { // 处理 GPU 任务释放
        if (node->type() == ChakraNodeType::COMP_NODE) { // 判断是否是 GPU 计算任务
            --num_in_flight_gpu_comp_ops; // 释放 GPU 计算任务
            assert(num_in_flight_gpu_comp_ops == 0); // 释放后 GPU 计算任务数必须为 0
        } else { // 处理 GPU 通信任务释放
            if (node->type() == ChakraNodeType::COMM_RECV_NODE) { // GPU 接收通信任务无需释放
                return;
            }
            --num_in_flight_gpu_comm_ops; // 释放 GPU 通信任务
            assert(num_in_flight_gpu_comm_ops == 0); // 释放后 GPU 通信任务数必须为 0
        }
    }
}

/**
 * @brief 判断计算资源是否可用
 * 
 * @param node 需要检查的计算任务节点
 * @return true 资源可用
 * @return false 资源不可用
 */
bool HardwareResource::is_available(
    const shared_ptr<Chakra::ETFeederNode> node) const {
    if (node->is_cpu_op()) { // 检查 CPU 资源可用性
        if (num_in_flight_cpu_ops == 0) {
            return true;
        } else {
            return false;
        }
    } else { // 检查 GPU 资源可用性
        if (node->type() == ChakraNodeType::COMP_NODE) { // 如果是 GPU 计算任务
            if (num_in_flight_gpu_comp_ops == 0) {
                return true;
            } else {
                return false;
            }
        } else {
            if (num_in_flight_gpu_comm_ops == 0) { // 如果 GPU 通信资源空闲
                return true;
            } else {
                if (node->type() == ChakraNodeType::COMM_RECV_NODE) { // 处理 GPU 通信接收任务
                    return true; // GPU 接收任务始终可用
                }
                if (num_in_flight_gpu_comm_ops == 0) {
                    return true;
                }
                return false;
            }
        }
    }
}

/**
 * @brief 输出硬件资源的统计信息
 */
void HardwareResource::report() {
    cout << "num_cpu_ops: " << num_cpu_ops << endl; // 输出 CPU 任务数量
    cout << "num_gpu_ops: " << num_gpu_ops << endl; // 输出 GPU 计算任务数量
    cout << "num_gpu_comms: " << num_gpu_comms << endl; // 输出 GPU 通信任务数量

    cout << "tics_cpu_ops: " << tics_cpu_ops << endl; // 输出 CPU 任务执行的时钟周期
    cout << "tics_gpu_ops: " << tics_gpu_ops << endl; // 输出 GPU 计算任务执行的时钟周期
    cout << "tics_gpu_comms: " << tics_gpu_comms << endl; // 输出 GPU 通信任务执行的时钟周期
}
