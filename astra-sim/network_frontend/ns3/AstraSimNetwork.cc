#include "astra-sim/common/AstraNetworkAPI.hh" // Astra-Sim 网络 API
#include "astra-sim/system/Sys.hh" // Astra-Sim 系统层
#include "extern/remote_memory_backend/analytical/AnalyticalRemoteMemory.hh" // 远程内存管理
#include <json/json.hpp> // 解析 JSON 配置文件

#include "entry.h" // 入口头文件
#include "ns3/applications-module.h" // NS3 应用层模块
#include "ns3/core-module.h" // NS3 核心模块
#include "ns3/csma-module.h" // NS3 CSMA 网络模块
#include "ns3/internet-module.h" // NS3 网络层模块
#include "ns3/network-module.h" // NS3 物理网络模块
#include <execinfo.h> // 获取程序调用栈
#include <fstream> // 文件流处理
#include <iostream> // 标准输入输出流
#include <queue> // 队列数据结构
#include <stdio.h> // 标准输入输出库
#include <string> // 字符串处理
#include <thread> // 线程管理
#include <unistd.h> // UNIX 标准库
#include <vector> // 动态数组
#include "astra-sim/common/Logging.hh" // 日志管理

using namespace std;
using namespace ns3;
using json = nlohmann::json;


/**
 * @class NS3BackendCompletionTracker
 * @brief Tracks the completion status of ranks in the NS3 backend.
 *
 * This is a hacky approach to track which ranks have completed their workload.
 * The purpose of this class is to end the ns3 simulation once all ranks have completed. 
 * The hacky approach is necessary because each ASTRASimNetwork instance only corresponds to one rank.
 * That is, someone needs to keep track of the completion status of all of the ranks.
 * Because there is no exit point once the ns3 simulator has started, 
 * we cannot implement such tracker in the main function.
 */
/**
 * @class NS3BackendCompletionTracker
 * @brief 追踪 NS3 后端所有计算节点（ranks）的完成状态
 *
 * 由于 Astra-Sim 的每个 `ASTRASimNetwork` 仅对应一个计算节点（rank），
 * 需要一个全局对象来追踪所有计算节点的完成状态。一旦所有节点完成计算，
 * 该类会终止 NS3 模拟器。
 */
class NS3BackendCompletionTracker {
    public:
        /**
         * @brief 构造函数
         * @param num_ranks 计算节点总数
         */
        NS3BackendCompletionTracker(int num_ranks) {
            num_unfinished_ranks_ = num_ranks;
            completion_tracker_ = vector<int>(num_ranks, 0);
        }
    
        /**
         * @brief 标记某个计算节点已完成计算
         * @param rank 计算节点 ID
         */
        void mark_rank_as_finished(int rank) {
            if (completion_tracker_[rank] == 0) {
                completion_tracker_[rank] = 1;
                num_unfinished_ranks_--;
            }
            if (num_unfinished_ranks_ == 0) {
                AstraSim::LoggerFactory::get_logger("network")
                    ->debug("All ranks have finished. Exiting simulation.");
                // 终止 NS3 模拟器
                Simulator::Stop();
                Simulator::Destroy();
                exit(0);
            }
        }
    
    private:
        int num_unfinished_ranks_; // 记录未完成计算的计算节点数
        vector<int> completion_tracker_; // 存储每个计算节点的完成状态
};

/**
 * @class ASTRASimNetwork
 * @brief 继承 AstraNetworkAPI，封装 NS3 网络仿真逻辑
 */
class ASTRASimNetwork : public AstraSim::AstraNetworkAPI {
  public:
     /**
     * @brief 构造函数
     * @param rank 当前计算节点 ID
     * @param completion_tracker 计算完成追踪器
     */
    ASTRASimNetwork(int rank, NS3BackendCompletionTracker *completion_tracker) : AstraNetworkAPI(rank) {
        completion_tracker_ = completion_tracker;
    }

    ~ASTRASimNetwork() {}

    /**
     * @brief 计算任务完成时的回调
     */
    void sim_notify_finished() {
        // Output to file instead of stdout
        // 之前的代码用于输出每个节点发送和接收的总数据量，但目前被注释掉
        /*
        for (auto it = node_to_bytes_sent_map.begin();
             it != node_to_bytes_sent_map.end(); it++) {
            pair<int, int> p = it->first;
            if (p.second == 0) {
                cout << "All data sent from node " << p.first << " is "
                     << it->second << "\n";
            } else {
                cout << "All data received by node " << p.first << " is "
                     << it->second << "\n";
            }
        }
        */
        completion_tracker_->mark_rank_as_finished(rank);  // 标记当前节点的计算任务完成
        return;
    }

    /**
     * @brief 获取仿真时间精度
     * @return 当前时间精度（返回 0）
     */
    double sim_time_resolution() {
        return 0;
    }

    /**
     * @brief 处理网络事件（当前为空实现）
     * @param dst 目标节点
     * @param cnt 计数（可能代表数据大小或消息数量）
     */
    void handleEvent(int dst, int cnt) {}


    /**
     * @brief 获取当前模拟时间
     * @return 当前时间（纳秒）
     */
    AstraSim::timespec_t sim_get_time() {
        AstraSim::timespec_t timeSpec;
        timeSpec.time_res = AstraSim::NS;  // 设置时间单位为纳秒
        timeSpec.time_val = Simulator::Now().GetNanoSeconds(); // 获取当前 NS3 仿真时间（纳秒）
        return timeSpec;
    }

    /**
     * @brief 调度事件
     * @param delta 延迟时间
     * @param fun_ptr 事件回调函数
     * @param fun_arg 事件参数
     */
    virtual void sim_schedule(AstraSim::timespec_t delta,
                              void (*fun_ptr)(void* fun_arg),
                              void* fun_arg) {
        Simulator::Schedule(NanoSeconds(delta.time_val), fun_ptr, fun_arg); // 在 NS3 中调度事件
        return;
    }

    /**
     * @brief 发送消息
     * @param buffer 数据缓冲区
     * @param message_size 消息大小（字节）
     * @param type 消息类型
     * @param dst_id 目标节点 ID
     * @param tag 消息标签
     * @param request 发送请求（用于跟踪消息状态）
     * @param msg_handler 消息处理回调函数
     * @param fun_arg 传递给回调函数的参数
     * @return 发送状态（当前始终返回 0）
     */
    virtual int sim_send(void* buffer,
                         uint64_t message_size,
                         int type,
                         int dst_id,
                         int tag,
                         AstraSim::sim_request* request,
                         void (*msg_handler)(void* fun_arg),
                         void* fun_arg) {
        int src_id = rank; // 发送方 ID 为当前计算节点

        // Trigger ns3 to schedule RDMA QP event.
        // 触发 NS3 事件，模拟 RDMA 传输
        send_flow(src_id, dst_id, message_size, msg_handler, fun_arg, tag);
        return 0;
    }

     /**
     * @brief 接收消息
     * @param buffer 数据缓冲区
     * @param message_size 期望接收的消息大小（字节）
     * @param type 消息类型
     * @param src_id 发送方 ID
     * @param tag 消息标签
     * @param request 接收请求（用于跟踪消息状态）
     * @param msg_handler 消息处理回调函数
     * @param fun_arg 传递给回调函数的参数
     * @return 接收状态（当前始终返回 0）
     */
    virtual int sim_recv(void* buffer,
                         uint64_t message_size,
                         int type,
                         int src_id,
                         int tag,
                         AstraSim::sim_request* request,
                         void (*msg_handler)(void* fun_arg),
                         void* fun_arg) {
        int dst_id = rank; // 目标 ID 为当前计算节点
        MsgEvent recv_event =
            MsgEvent(src_id, dst_id, 1, message_size, fun_arg, msg_handler);
        MsgEventKey recv_event_key =
            make_pair(tag, make_pair(recv_event.src_id, recv_event.dst_id));

        if (received_msg_standby_hash.find(recv_event_key) !=
            received_msg_standby_hash.end()) {
            // 1) ns3 has already received some message before sim_recv is
            // called.
            // 1) NS3 已收到部分或全部消息，但 sim_recv 还未被调用
            int received_msg_bytes = received_msg_standby_hash[recv_event_key];
            if (received_msg_bytes == message_size) {
                // 1-1) The received message size is same as what we expect.
                // Exit.
                // 1-1) 已收到完整消息，直接调用回调函数并移除记录
                received_msg_standby_hash.erase(recv_event_key);
                recv_event.callHandler();
            } else if (received_msg_bytes > message_size) {
                // 1-2) The node received more than expected.
                // Do trigger the callback handler for this message, but wait
                // for Sys layer to call sim_recv for more messages.
                // 1-2) 收到的消息比期望的多
                // 触发回调函数，并更新剩余数据
                received_msg_standby_hash[recv_event_key] =
                    received_msg_bytes - message_size;
                recv_event.callHandler();
            } else {
                // 1-3) The node received less than what we expected.
                // Reduce the number of bytes we are waiting to receive.
                // 1-3) 收到的消息比期望的少
                // 记录剩余未接收的消息
                received_msg_standby_hash.erase(recv_event_key);
                recv_event.remaining_msg_bytes -= received_msg_bytes;
                sim_recv_waiting_hash[recv_event_key] = recv_event;
            }
        } else {
            // 2) ns3 has not yet received anything.
            // 2) NS3 还未收到任何相关的消息
            if (sim_recv_waiting_hash.find(recv_event_key) ==
                sim_recv_waiting_hash.end()) {
                // 2-1) We have not been expecting anything.
                // 2-1) 之前没有在等待这个消息，记录该消息的等待状态
                sim_recv_waiting_hash[recv_event_key] = recv_event;
            } else {
                // 2-2) We have already been expecting something.
                // Increment the number of bytes we are waiting to receive.
                // 2-2) 之前已经在等待该消息，更新等待状态
                int expecting_msg_bytes =
                    sim_recv_waiting_hash[recv_event_key].remaining_msg_bytes;
                recv_event.remaining_msg_bytes += expecting_msg_bytes;
                sim_recv_waiting_hash[recv_event_key] = recv_event;
            }
        }
        return 0;
    }

    private:
    NS3BackendCompletionTracker* completion_tracker_; // 计算完成追踪器指针
};

// 表示用于定义和解析工作负载配置的文件路径
string workload_configuration;

// 表示用于定义系统层面的配置文件路径
string system_configuration;

// 表示网络层配置文件的路径，通常包含网络拓扑、带宽等信息
string network_configuration;

// 表示远程内存系统的配置文件路径，涉及计算节点如何访问共享或分布式存储
string memory_configuration;

// 指定通信组（communicator group）配置文件路径，默认为 "empty"
// 该文件通常用于定义计算节点之间的逻辑通信关系，如不同通信域
string comm_group_configuration = "empty";

// 逻辑拓扑配置文件的路径，涉及多维度互连拓扑的定义
string logical_topology_configuration;

// 日志配置文件路径，默认为 "empty"
// 该文件可能包含日志级别、日志输出位置等设置
string logging_configuration = "empty";

// 指定每个拓扑维度上的队列数量，默认为 1
// 该参数通常用于并行通信策略，影响消息调度
int num_queues_per_dim = 1;

// 通信规模调整参数，默认为 1
// 该参数用于缩放通信量，以便模拟不同规模的通信负载
double comm_scale = 1;

// 数据注入比例，默认为 1
// 该参数影响数据在网络中的传输速率
double injection_scale = 1;

// 是否启用 rendezvous 协议（即是否使用同步通信机制），默认关闭
// Rendezvous 协议通常用于优化大规模数据传输，减少带宽占用
bool rendezvous_protocol = false;

// 逻辑维度向量（如 [4, 8, 8] 表示一个 3D 互连拓扑）
// 该变量用于存储从 logical_topology_configuration 读取的网络维度信息
auto logical_dims = vector<int>();

// 计算系统中的 NPU（神经处理单元）总数，默认值为 1
// 该变量会根据 logical_dims 的配置进行更新
int num_npus = 1;

// 每个拓扑维度上的队列数
// 用于存储不同拓扑维度上的队列数量，默认为空向量
auto queues_per_dim = vector<int>();



// TODO: Migrate to yaml

/**
 * @brief 读取逻辑拓扑配置
 * @param network_configuration 网络配置文件路径
 * @param logical_dims 存储逻辑拓扑维度信息的向量
 */
void read_logical_topo_config(string network_configuration,
                              vector<int>& logical_dims) {
    ifstream inFile;
    inFile.open(network_configuration); // 打开网络配置文件
    if (!inFile) {
        cerr << "Unable to open file: " << network_configuration << endl;
        exit(1); // 文件打开失败时终止程序
    }

    // Find the size of each dimension.
    // 解析 JSON 配置文件
    json j;
    inFile >> j;

    // 获取逻辑拓扑的维度信息
    if (j.contains("logical-dims")) { // 判断 JSON 中是否包含 "logical-dims" 键
        vector<string> logical_dims_str_vec = j["logical-dims"];  // 读取维度信息
        for (auto logical_dims_str : logical_dims_str_vec) {
            logical_dims.push_back(stoi(logical_dims_str)); // 将字符串转换为整数并存储
        }
    }

    // Find the number of all npus.
    // 计算总 NPU 数量
    stringstream dimstr;
    for (auto num_npus_per_dim : logical_dims) {
        num_npus *= num_npus_per_dim; // 计算 NPU 总数（所有维度的乘积）
        dimstr << num_npus_per_dim << ","; // 记录维度信息
    }
    cout << "There are " << num_npus << " npus: " << dimstr.str() << "\n"; // 输出 NPU 数量

    // 为每个维度初始化队列数量，默认为 num_queues_per_dim
    queues_per_dim = vector<int>(logical_dims.size(), num_queues_per_dim);
}

/**
 * @brief 解析命令行参数
 * @param argc 参数个数
 * @param argv 参数数组
 */
void parse_args(int argc, char* argv[]) {
    CommandLine cmd;

    // 读取工作负载配置文件路径
    cmd.AddValue("workload-configuration", "Workload configuration file.",
                 workload_configuration);
    // 读取系统配置文件路径
    cmd.AddValue("system-configuration", "System configuration file",
                 system_configuration);
    // 读取网络配置文件路径
    cmd.AddValue("network-configuration", "Network configuration file",
                 network_configuration);
    // 读取远程内存配置文件路径
    cmd.AddValue("remote-memory-configuration", "Memory configuration file",
                 memory_configuration);
    // 读取通信组配置文件路径
    cmd.AddValue("comm-group-configuration",
                 "Communicator group configuration file",
                 comm_group_configuration);
    // 读取逻辑拓扑配置文件路径
    cmd.AddValue("logical-topology-configuration",
                 "Logical topology configuration file",
                 logical_topology_configuration);
    // 读取日志配置文件路径
    cmd.AddValue("logging-configuration",
                 "Logging configuration file", 
                 logging_configuration);
    // 读取每个拓扑维度的队列数量
    cmd.AddValue("num-queues-per-dim", "Number of queues per each dimension",
                 num_queues_per_dim);
    // 读取通信规模调整参数
    cmd.AddValue("comm-scale", "Communication scale", comm_scale);
    // 读取数据注入比例
    cmd.AddValue("injection-scale", "Injection scale", injection_scale);
    // 是否启用 rendezvous 协议（同步通信机制）
    cmd.AddValue("rendezvous-protocol", "Whether to enable rendezvous protocol",
                 rendezvous_protocol);
    // 解析命令行参数
    cmd.Parse(argc, argv);
}

/**
 * @brief 主函数，初始化并运行 ASTRA-sim + NS3 仿真
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return 程序执行状态码（0 表示成功，-1 表示失败）
 */
int main(int argc, char* argv[]) {

    // 启用 NS3 组件日志，设置日志级别为 INFO
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    cout << "ASTRA-sim + NS3" << endl; // 输出启动信息

    // Read network config and find logical dims.
    // 解析命令行参数，并读取相关的配置文件
    parse_args(argc, argv);
    AstraSim::LoggerFactory::init(logging_configuration);  // 初始化日志系统
    read_logical_topo_config(logical_topology_configuration, logical_dims); // 解析逻辑拓扑配置

    // 设置网络和系统层
    vector<ASTRASimNetwork*> networks(num_npus, nullptr); // 创建存储 NPU 网络对象的向量
    vector<AstraSim::Sys*> systems(num_npus, nullptr); // 创建存储 NPU 系统对象的向量
    
    // 初始化远程内存模拟组件
    Analytical::AnalyticalRemoteMemory* mem =
        new Analytical::AnalyticalRemoteMemory(memory_configuration);

    // 创建 NS3 后端完成状态追踪器
    NS3BackendCompletionTracker* completion_tracker = new NS3BackendCompletionTracker(num_npus);

    // 遍历所有 NPU，初始化对应的网络和系统组件
    for (int npu_id = 0; npu_id < num_npus; npu_id++) {
        // 为当前 NPU 创建 ASTRASimNetwork 网络实例，并绑定完成追踪器
        networks[npu_id] = new ASTRASimNetwork(npu_id, completion_tracker);
        // 创建 AstraSim 系统实例，并初始化参数（包括计算和通信配置）
        systems[npu_id] = new AstraSim::Sys(
            npu_id, workload_configuration, comm_group_configuration,
            system_configuration, mem, networks[npu_id], logical_dims,
            queues_per_dim, injection_scale, comm_scale, rendezvous_protocol);
    }

    // 初始化 NS3 仿真环境
    if (auto ok = setup_ns3_simulation(network_configuration); ok == -1) {
        std::cerr << "Fail to setup ns3 simulation." << std::endl;
        return -1; // 如果 NS3 仿真环境初始化失败，则终止程序
    }

    // 通知工作负载层（workload layer）调度第一个事件
    for (int i = 0; i < num_npus; i++) {
        systems[i]->workload->fire();
    }

    // 运行仿真，启动 NS3 事件队列
    Simulator::Run();
    return 0;
}
