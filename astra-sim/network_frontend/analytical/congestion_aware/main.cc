/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/common/Logging.hh" // 日志管理
#include "common/CmdLineParser.hh" // 命令行参数解析
#include "congestion_aware/CongestionAwareNetworkApi.hh" // 拥塞感知网络 API
#include <astra-network-analytical/common/EventQueue.h> // 事件队列管理
#include <astra-network-analytical/common/NetworkParser.h> // 解析网络配置
#include <astra-network-analytical/congestion_aware/Helper.h> // 辅助函数
#include <remote_memory_backend/analytical/AnalyticalRemoteMemory.hh> // 远程内存管理

// 使用相关命名空间，避免冗长的命名
using namespace AstraSim;
using namespace Analytical;
using namespace AstraSimAnalytical;
using namespace AstraSimAnalyticalCongestionAware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionAware;

/**
 * @brief ASTRA-sim 仿真主函数
 * @param argc 命令行参数个数
 * @param argv 命令行参数列表
 * @return 0 表示程序成功运行
 */
int main(int argc, char* argv[]) {
    // 解析命令行参数
    auto cmd_line_parser = CmdLineParser(argv[0]);
    cmd_line_parser.parse(argc, argv);

    // 获取命令行参数
    const auto workload_configuration =
        cmd_line_parser.get<std::string>("workload-configuration");
    const auto comm_group_configuration =
        cmd_line_parser.get<std::string>("comm-group-configuration");
    const auto system_configuration =
        cmd_line_parser.get<std::string>("system-configuration");
    const auto remote_memory_configuration =
        cmd_line_parser.get<std::string>("remote-memory-configuration");
    const auto network_configuration =
        cmd_line_parser.get<std::string>("network-configuration");
    const auto logging_configuration =
        cmd_line_parser.get<std::string>("logging-configuration");
    const auto num_queues_per_dim =
        cmd_line_parser.get<int>("num-queues-per-dim");
    const auto comm_scale = cmd_line_parser.get<double>("comm-scale");
    const auto injection_scale = cmd_line_parser.get<double>("injection-scale");
    const auto rendezvous_protocol =
        cmd_line_parser.get<bool>("rendezvous-protocol");

    // 初始化日志系统
    AstraSim::LoggerFactory::init(logging_configuration);

    // 创建事件队列，用于管理网络事件的执行时序
    const auto event_queue = std::make_shared<EventQueue>();
    Topology::set_event_queue(event_queue);

    // 解析网络配置并生成拓扑
    const auto network_parser = NetworkParser(network_configuration);
    const auto topology = construct_topology(network_parser);

    // 获取拓扑信息
    const auto npus_count = topology->get_npus_count(); // 计算节点数
    const auto npus_count_per_dim = topology->get_npus_count_per_dim(); // 每个维度的 NPU 数
    const auto dims_count = topology->get_dims_count(); // 维度数

    // 设置拥塞感知网络 API
    CongestionAwareNetworkApi::set_event_queue(event_queue);
    CongestionAwareNetworkApi::set_topology(topology);

    // 创建 ASTRA-sim 相关资源
    auto network_apis =
        std::vector<std::unique_ptr<CongestionAwareNetworkApi>>(); // 存储网络 API 实例
    const auto memory_api =
        std::make_unique<AnalyticalRemoteMemory>(remote_memory_configuration); // 远程内存管理
    auto systems = std::vector<Sys*>(); // 存储计算系统实例

    // 初始化队列数（每个维度的队列数）
    auto queues_per_dim = std::vector<int>();
    for (auto i = 0; i < dims_count; i++) {
        queues_per_dim.push_back(num_queues_per_dim);
    }

    // 为每个计算节点（NPU）创建 `Sys` 和 `CongestionAwareNetworkApi` 实例
    for (int i = 0; i < npus_count; i++) {
        // 创建网络 API 和计算系统
        auto network_api = std::make_unique<CongestionAwareNetworkApi>(i);
        auto* const system =
            new Sys(i, workload_configuration, comm_group_configuration,
                    system_configuration, memory_api.get(), network_api.get(),
                    npus_count_per_dim, queues_per_dim, injection_scale,
                    comm_scale, rendezvous_protocol);

        // 存储 `network_api` 和 `system`
        network_apis.push_back(std::move(network_api));
        systems.push_back(system);
    }

    // 触发所有 `Sys` 实例的 workload
    for (int i = 0; i < npus_count; i++) {
        systems[i]->workload->fire();
    }

    // 运行 ASTRA-sim 仿真，事件队列驱动整个仿真进程
    while (!event_queue->finished()) {
        event_queue->proceed();
    }

    // 终止仿真
    AstraSim::LoggerFactory::shutdown();
    return 0;
}
