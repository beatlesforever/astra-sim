/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "astra-sim/common/Logging.hh" // 日志管理
#include "common/CmdLineParser.hh" // 解析命令行参数
#include "congestion_unaware/CongestionUnawareNetworkApi.hh" // 非拥塞感知网络 API
#include <astra-network-analytical/common/EventQueue.h> // 事件队列管理
#include <astra-network-analytical/common/NetworkParser.h> // 解析网络配置
#include <astra-network-analytical/congestion_unaware/Helper.h> // 拓扑相关的辅助函数
#include <remote_memory_backend/analytical/AnalyticalRemoteMemory.hh> // 远程内存管理

// 使用相关命名空间，避免冗长的命名
using namespace AstraSim;
using namespace Analytical;
using namespace AstraSimAnalytical;
using namespace AstraSimAnalyticalCongestionUnaware;
using namespace NetworkAnalytical;
using namespace NetworkAnalyticalCongestionUnaware;

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
        cmd_line_parser.get<std::string>("workload-configuration"); // 计算工作负载配置
    const auto comm_group_configuration =
        cmd_line_parser.get<std::string>("comm-group-configuration"); // 通信组配置
    const auto system_configuration =
        cmd_line_parser.get<std::string>("system-configuration"); // 系统配置
    const auto remote_memory_configuration =
        cmd_line_parser.get<std::string>("remote-memory-configuration"); // 远程内存配置
    const auto network_configuration =
        cmd_line_parser.get<std::string>("network-configuration"); // 网络拓扑配置
    const auto logging_configuration =
        cmd_line_parser.get<std::string>("logging-configuration"); // 日志配置
    const auto num_queues_per_dim =
        cmd_line_parser.get<int>("num-queues-per-dim"); // 每个维度的队列数量
    const auto comm_scale = cmd_line_parser.get<double>("comm-scale"); // 通信缩放因子
    const auto injection_scale = cmd_line_parser.get<double>("injection-scale"); // 数据注入速率
    const auto rendezvous_protocol =
        cmd_line_parser.get<bool>("rendezvous-protocol"); // 是否启用 rendezvous 协议

    // 初始化日志系统
    AstraSim::LoggerFactory::init(logging_configuration);

    // 创建事件队列
    const auto event_queue = std::make_shared<EventQueue>();

    // 解析网络配置并生成拓扑
    const auto network_parser = NetworkParser(network_configuration);
    const auto topology = construct_topology(network_parser);

    // 获取拓扑信息
    const auto npus_count = topology->get_npus_count(); // 计算节点数
    const auto npus_count_per_dim = topology->get_npus_count_per_dim(); // 每个维度的 NPU 数
    const auto dims_count = topology->get_dims_count(); // 维度数

    // 设置非拥塞感知网络 API
    CongestionUnawareNetworkApi::set_event_queue(event_queue);
    CongestionUnawareNetworkApi::set_topology(topology);

    // 创建 ASTRA-sim 相关资源
    auto network_apis =
        std::vector<std::unique_ptr<CongestionUnawareNetworkApi>>(); // 存储网络 API 实例
    const auto memory_api =
        std::make_unique<AnalyticalRemoteMemory>(remote_memory_configuration); // 远程内存管理
    auto systems = std::vector<Sys*>(); // 存储计算系统实例

    // 初始化每个维度的队列数
    auto queues_per_dim = std::vector<int>();
    for (auto i = 0; i < dims_count; i++) {
        queues_per_dim.push_back(num_queues_per_dim);
    }

    // 为每个计算节点（NPU）创建 `Sys` 和 `CongestionUnawareNetworkApi` 实例
    for (int i = 0; i < npus_count; i++) {
        // 创建网络 API 和计算系统
        auto network_api = std::make_unique<CongestionUnawareNetworkApi>(i);
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
