/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#include "common/CmdLineParser.hh" // 包含 CmdLineParser 类的声明，提供命令行参数解析功能

using namespace AstraSimAnalytical; // 使用 AstraSimAnalytical 命名空间

/**
 * @class CmdLineParser
 * @brief 该类用于解析 ASTRA-sim 的命令行参数，并存储配置信息。
 */

/**
 * @brief CmdLineParser 构造函数
 * @param argv0 可执行文件的名称
 */
CmdLineParser::CmdLineParser(const char* const argv0) noexcept
    : options(argv0, "ASTRA-sim") { // 初始化 `options`，指定程序名称
    parsed = {}; // 初始化解析后的参数存储

    // 定义可接受的命令行选项
    define_options();
}

/**
 * @brief 定义命令行选项
 */
void CmdLineParser::define_options() noexcept {
    options.set_width(70) // 设置选项显示宽度，便于格式化输出
        .allow_unrecognised_options() // 允许解析未定义的选项，避免因额外参数报错
        .add_options() // 添加选项
        ("workload-configuration", "Workload configuration file",
         cxxopts::value<std::string>()) // 训练工作负载配置文件
        ("comm-group-configuration", "Communicator group configuration file",
         cxxopts::value<std::string>()->default_value("empty")) // 通信组配置文件，默认值为 "empty"
        ("system-configuration", "System configuration file",
         cxxopts::value<std::string>()) // 系统配置文件
        ("remote-memory-configuration", "Remote memory configuration file",
         cxxopts::value<std::string>()) // 远程内存配置文件
        ("network-configuration", "Network configuration file",
         cxxopts::value<std::string>()) // 网络配置文件
        ("logging-configuration", "Logging configuration file",
         cxxopts::value<std::string>()->default_value("empty")) // 日志配置文件，默认值为 "empty"
        ("num-queues-per-dim", "Number of queues per each dimension",
         cxxopts::value<int>()->default_value("1")) // 每个维度的队列数，默认为 1
        ("compute-scale", "Compute scale",
         cxxopts::value<double>()->default_value("1")) // 计算资源缩放比例，默认为 1
        ("comm-scale", "Communication scale",
         cxxopts::value<double>()->default_value("1")) // 通信资源缩放比例，默认为 1
        ("injection-scale", "Injection scale",
         cxxopts::value<double>()->default_value("1")) // 数据注入速率缩放比例，默认为 1
        ("rendezvous-protocol", "Whether to enable rendezvous protocol",
         cxxopts::value<bool>()->default_value("false")); // 是否启用 rendezvous 协议，默认为 false
}

/**
 * @brief 解析命令行参数
 * @param argc 参数个数
 * @param argv 参数列表
 */
void CmdLineParser::parse(int argc, char* argv[]) noexcept {
    try {
        // 解析命令行参数，并存入 `parsed`
        parsed = options.parse(argc, argv);
    } catch (const cxxopts::OptionException& e) {
        // 发生解析错误时，输出错误信息，并终止程序
        std::cerr << "[Error] (AstraSim/analytical/common) "
                  << "Error parsing options: " << e.what() << std::endl;
        exit(-1);
    }
}
