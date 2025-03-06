/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/
// Workload.cc 实现 Workload 类，该类用于管理计算任务调度，包括计算、内存访问和通信操作。

#include "astra-sim/workload/Workload.hh" // 包含 Workload 类的声明

#include "astra-sim/common/Logging.hh" // 日志系统
#include "astra-sim/system/IntData.hh" // 整数数据管理
#include "astra-sim/system/MemEventHandlerData.hh" // 内存事件处理
#include "astra-sim/system/RecvPacketEventHandlerData.hh" // 接收数据包处理
#include "astra-sim/system/SendPacketEventHandlerData.hh" // 发送数据包处理
#include "astra-sim/system/WorkloadLayerHandlerData.hh" // 训练层任务数据处理
#include <json/json.hpp> // JSON 解析库

#include <iostream> // 标准输入输出
#include <stdlib.h> // 标准库
#include <unistd.h> // 访问文件系统

using namespace std;
using namespace AstraSim;
using namespace Chakra;
using json = nlohmann::json; // 使用 nlohmann::json 进行 JSON 解析

typedef ChakraProtoMsg::NodeType ChakraNodeType; // 定义计算任务节点类型别名
typedef ChakraProtoMsg::CollectiveCommType ChakraCollectiveCommType; // 定义集合通信类型别名

/**
 * @brief Workload 构造函数
 * 
 * @param sys 指向系统对象的指针
 * @param et_filename 计算任务的输入文件名
 * @param comm_group_filename 通信组配置文件
 */
Workload::Workload(Sys* sys, string et_filename, string comm_group_filename) {

    // 生成 workload 文件名，例如 "et_filename.sys_id.et"
    string workload_filename = et_filename + "." + to_string(sys->id) + ".et";

    // 检查 workload 文件是否存在
    if (access(workload_filename.c_str(), R_OK) < 0) {
        string error_msg;
        if (errno == ENOENT) { // 文件不存在
            error_msg =
                "workload file: " + workload_filename + " does not exist";
        } else if (errno == EACCES) { // 文件存在但无权限
            error_msg = "workload file: " + workload_filename +
                        " exists but is not readable";
        } else { // 其他未知错误
            error_msg =
                "Unknown workload file: " + workload_filename + " access error";
        }
        LoggerFactory::get_logger("workload")->critical(error_msg);
        exit(EXIT_FAILURE); // 终止程序
    }

    // 初始化 ETFeeder 解析任务文件
    this->et_feeder = new ETFeeder(workload_filename);
    this->comm_group = nullptr;
    // TODO: parametrize the number of available hardware resources
    // TODO: 允许参数化硬件资源数量
    this->hw_resource = new HardwareResource(1);
    this->sys = sys;

    // 初始化通信组
    initialize_comm_group(comm_group_filename);

    // 设定工作负载初始状态为未完成
    this->is_finished = false;
}

/**
 * @brief 析构函数，释放 Workload 相关资源
 */
Workload::~Workload() {
    if (this->comm_group != nullptr) {
        delete this->comm_group;
    }
    if (this->et_feeder != nullptr) {
        delete this->et_feeder;
    }
    if (this->hw_resource != nullptr) {
        delete this->hw_resource;
    }
}

/**
 * @brief 初始化通信组
 * 
 * @param comm_group_filename 包含通信组信息的 JSON 文件路径
 */
void Workload::initialize_comm_group(string comm_group_filename) {
    // communicator group input file is not given
    // 如果通信组文件包含 "empty"，则不创建通信组
    if (comm_group_filename.find("empty") != std::string::npos) {
        comm_group = nullptr; // 设置为空
        return;
    }

    ifstream inFile; // 定义输入文件流
    json j; // 定义 JSON 解析对象
    inFile.open(comm_group_filename); // 打开通信组配置文件
    inFile >> j; // 读取 JSON 数据

    // 遍历 JSON 数据，解析通信组信息
    for (json::iterator it = j.begin(); it != j.end(); ++it) {
        bool in_comm_group = false; // 标志当前 ID 是否属于该通信组

        // 检查当前系统 ID 是否属于该通信组
        for (auto id : it.value()) {
            if (id == sys->id) {
                in_comm_group = true;
            }
        }

        // 如果当前系统 ID 属于该通信组，则创建通信组对象
        if (in_comm_group) {
            std::vector<int> involved_NPUs; // 存储通信组中的 NPU ID
            for (auto id : it.value()) {
                involved_NPUs.push_back(id); // 记录该通信组的所有 NPU ID
            }
            comm_group = new CommunicatorGroup(1, involved_NPUs, sys);
            // 注意：所有 NPU 必须创建相同 ID 的通信组，否则无法通信
            // Note: All NPUs should create comm group with identical ids if
            // they want to communicate with each other
        }
    }
}

/**
 * @brief 处理无依赖的任务节点，并将可执行的任务调度出去
 */
void Workload::issue_dep_free_nodes() {

    std::queue<shared_ptr<Chakra::ETFeederNode>> push_back_queue; // 用于存储暂时无法执行的任务
    shared_ptr<Chakra::ETFeederNode> node = et_feeder->getNextIssuableNode(); // 获取下一个可执行的任务节点

    // 遍历所有可执行任务节点
    while (node != nullptr) {
        if (hw_resource->is_available(node)) { // 如果硬件资源可用
            issue(node); // 立即执行任务
        } else { // 如果资源不可用
            push_back_queue.push(node); // 将任务加入待执行队列
        }
        node = et_feeder->getNextIssuableNode(); // 获取下一个任务
    }

    // 重新将无法执行的任务放回任务队列
    while (!push_back_queue.empty()) {
        shared_ptr<Chakra::ETFeederNode> node = push_back_queue.front(); // 取出队列中的任务
        et_feeder->pushBackIssuableNode(node->id()); // 将任务重新放回任务池
        push_back_queue.pop(); // 移除队列中的任务
    }
}

/**
 * @brief 处理任务的执行
 * 
 * @param node 需要执行的任务节点
 */
void Workload::issue(shared_ptr<Chakra::ETFeederNode> node) {
    auto logger = LoggerFactory::get_logger("workload"); // 获取日志记录器

    if (sys->replay_only) { // 如果系统处于回放模式
        hw_resource->occupy(node); // 占用计算资源
        issue_replay(node); // 直接回放任务
    } else {
        if ((node->type() == ChakraNodeType::MEM_LOAD_NODE) ||
            (node->type() == ChakraNodeType::MEM_STORE_NODE)) { // 处理内存加载或存储任务
            if (sys->trace_enabled) {  // 如果启用日志追踪，则记录调试信息
                logger->debug("issue,sys->id={}, tick={}, node->id={}, "
                              "node->name={}, node->type={}",
                              sys->id, Sys::boostedTick(), node->id(),
                              node->name(),
                              static_cast<uint64_t>(node->type()));
            }
            issue_remote_mem(node); // 执行远程内存操作
        } else if (node->is_cpu_op() ||
                   (!node->is_cpu_op() &&
                    node->type() == ChakraNodeType::COMP_NODE)) { // 处理 CPU 或 GPU 计算任务
            if ((node->runtime() == 0) && (node->num_ops() == 0)) { // 如果任务无效
                skip_invalid(node); // 跳过无效任务
            } else { // 任务有效，执行计算
                if (sys->trace_enabled) { // 记录调试信息
                    logger->debug("issue,sys->id={}, tick={}, node->id={}, "
                                  "node->name={}, node->type={}",
                                  sys->id, Sys::boostedTick(), node->id(),
                                  node->name(),
                                  static_cast<uint64_t>(node->type()));
                }
                issue_comp(node); // 执行计算任务
            }
        } else if (!node->is_cpu_op() &&
                   (node->type() == ChakraNodeType::COMM_COLL_NODE ||
                    (node->type() == ChakraNodeType::COMM_SEND_NODE) ||
                    (node->type() == ChakraNodeType::COMM_RECV_NODE))) {  // 处理通信任务
            if (sys->trace_enabled) { // 记录调试信息
                if (sys->trace_enabled) {
                    logger->debug("issue,sys->id={}, tick={}, node->id={}, " //当前NPU的ID,获取当前仿真时间的时间戳(tick),任务节点ID
                                  "node->name={}, node->type={}", //任务名称,任务的类型
                                  sys->id, Sys::boostedTick(), node->id(),
                                  node->name(),
                                  static_cast<uint64_t>(node->type()));
                }
            }
            issue_comm(node); // 执行通信任务
        } else if (node->type() == ChakraNodeType::INVALID_NODE) { // 如果任务无效
            skip_invalid(node);  // 跳过任务
        }
    }
}

/**
 * @brief 回放任务，直接按照记录的执行时间模拟任务执行
 * 
 * @param node 需要回放执行的任务节点
 */
void Workload::issue_replay(shared_ptr<Chakra::ETFeederNode> node) {
    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData; // 创建任务处理数据对象
    wlhd->node_id = node->id(); // 记录任务节点 ID

    uint64_t runtime = 1ul; // 默认执行时间为 1 纳秒
    if (node->runtime() != 0ul) { // 如果任务有实际运行时间
        // chakra runtimes are in microseconds and we should convert it into
        // nanoseconds
        // Chakra 框架中的任务运行时间单位是微秒 (μs)，需要转换为纳秒 (ns)
        runtime = node->runtime() * 1000;
    }
    if (node->is_cpu_op()) { // 如果是 CPU 计算任务
        hw_resource->tics_cpu_ops += runtime; // 记录 CPU 执行时间
    } else { // 如果是 GPU 计算任务
        hw_resource->tics_gpu_ops += runtime; // 记录 GPU 执行时间
    }

    // 向系统注册任务执行事件，模拟任务的执行时间
    //通过 sys->register_event 注册一个 “延迟时间”，让系统模拟这个任务已经执行了 runtime 时间。
    sys->register_event(this, EventType::General, wlhd, runtime);
}

/**
 * @brief 处理远程内存操作任务
 * 
 * @param node 需要执行的任务节点
 */
void Workload::issue_remote_mem(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node); // 占用计算资源，确保任务不会并发执行

    WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData;// 创建任务处理数据对象
    wlhd->sys_id = sys->id; // 记录当前系统 ID
    wlhd->workload = this; // 关联当前 Workload 实例
    wlhd->node_id = node->id(); // 记录任务节点 ID

    // 远程内存访问，读取或写入指定张量大小的数据
    sys->remote_mem->issue(node->tensor_size(), wlhd);
}

/**
 * @brief 处理计算任务 (CPU 或 GPU)
 * 
 * @param node 需要执行的计算任务节点
 */
void Workload::issue_comp(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node); // 占用计算资源

    if (sys->roofline_enabled) { // 如果启用了 Roofline 模型
        WorkloadLayerHandlerData* wlhd = new WorkloadLayerHandlerData; // 创建任务处理数据对象
        wlhd->node_id = node->id(); // 记录任务节点 ID

        // 计算任务的操作强度 (Operational Intensity, OI)
        double operational_intensity = static_cast<double>(node->num_ops()) /
                                       static_cast<double>(node->tensor_size());

        // 通过 Roofline 模型计算当前计算任务的性能
        double perf = sys->roofline->get_perf(operational_intensity);

        // 计算任务执行时间 = 操作数 / 计算性能 (单位: 秒)
        double elapsed_time =
            static_cast<double>(node->num_ops()) / perf;  // sec

        // 将时间转换为纳秒 (ns)
        uint64_t runtime =
            static_cast<uint64_t>(elapsed_time * 1e9);  // sec -> ns
        
        if (node->is_cpu_op()) { // 如果是 CPU 计算任务
            hw_resource->tics_cpu_ops += runtime; // 记录 CPU 执行时间
        } else { // 如果是 GPU 计算任务
            hw_resource->tics_gpu_ops += runtime; // 记录 GPU 执行时间
        }

        // 注册任务执行事件，模拟 Roofline 模型计算
        sys->register_event(this, EventType::General, wlhd, runtime);
    } else {
        // advance this node forward the recorded "replayed" time specificed in
        // the ET.
        // 如果 Roofline 模型未启用，则直接进行任务回放
        issue_replay(node);
    }
}

/**
 * @brief 处理通信任务，包括集合通信 (Collective Communication) 和点对点通信
 * 
 * @param node 需要执行通信的任务节点
 */
void Workload::issue_comm(shared_ptr<Chakra::ETFeederNode> node) {
    hw_resource->occupy(node); // 占用计算资源，确保任务不会与其他任务并行执行

    vector<bool> involved_dim; // 记录当前任务涉及的通信维度信息


    // 检查任务是否包含 "involved_dim" 属性，该属性定义了涉及的通信维度
    if (node->has_other_attr("involved_dim")) {

        // 获取 "involved_dim" 的属性值
        const ChakraProtoMsg::AttributeProto& attr =
            node->get_other_attr("involved_dim");

        // Ensure the attribute is of type bool_list before accessing
        // 确保 "involved_dim" 的数据类型是 bool_list
        if (attr.has_bool_list()) {
            const ChakraProtoMsg::BoolList& bool_list = attr.bool_list();

            // Traverse bool_list and add values to involved_dim
            // 遍历 bool_list 并将值存入 involved_dim
            for (int i = 0; i < bool_list.values_size(); ++i) {
                involved_dim.push_back(bool_list.values(i));
            }
        } else {
            cerr << "Expected bool_list in involved_dim but found another type."
                 << endl;
            exit(EXIT_FAILURE); // 如果数据格式错误，则终止程序
        }
    } else {
        // involved_dim does not exist in ETFeeder.
        // Assume involved_dim = [1,1,1,1,1] which we could simulate 5-Dimension.
	    // Could use Process Group to build involved_dim later. 
	    // Once process group is implemented, you should get
        // that with node->pg_name()

        // 如果 "involved_dim" 在 ETFeeder 中不存在，则假设它是 [1,1,1,1,1]（即默认模拟 5 维通信）
        // 未来可以通过 Process Group 来构建 involved_dim
	
	for(int i = 0; i < 4; i++) // 这里假设 4 维通信
            involved_dim.push_back(true);
    }

    // 处理集合通信 (Collective Communication) 任务
    if (!node->is_cpu_op() &&
        (node->type() == ChakraNodeType::COMM_COLL_NODE)) {

        // 调用系统接口，生成 All-Reduce 集合通信任务
        if (node->comm_type() == ChakraCollectiveCommType::ALL_REDUCE) {

            DataSet* fp =
                sys->generate_all_reduce(node->comm_size(), // 通信数据大小
                                         involved_dim, // 涉及的通信维度
                                         comm_group, // 通信组
                                         node->comm_priority()); // 任务的通信优先级
            // 记录集合通信节点 ID 对应的任务 ID
            collective_comm_node_id_map[fp->my_id] = node->id();
            // 存储任务 DataSet 结构，以便后续管理
            collective_comm_wrapper_map[fp->my_id] = fp;
            // 设置通知回调，任务完成后触发 `CollectiveCommunicationFinished` 事件
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() == ChakraCollectiveCommType::ALL_TO_ALL) { // 处理 All-to-All 操作
            
            // 调用系统接口，生成 All-to-All 集合通信任务
            DataSet* fp =
                sys->generate_all_to_all(node->comm_size(), involved_dim,
                                         comm_group, node->comm_priority());
            // 记录任务 ID 映射
            collective_comm_node_id_map[fp->my_id] = node->id();
            // 存储任务 DataSet 结构
            collective_comm_wrapper_map[fp->my_id] = fp;
            // 设置任务完成事件通知
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

            // 处理 All-Gather 操作
        } else if (node->comm_type() == ChakraCollectiveCommType::ALL_GATHER) {
            DataSet* fp =
                sys->generate_all_gather(node->comm_size(), involved_dim,
                                         comm_group, node->comm_priority());

             // 记录任务 ID 映射
             collective_comm_node_id_map[fp->my_id] = node->id();
             // 存储任务 DataSet 结构
             collective_comm_wrapper_map[fp->my_id] = fp;
             // 设置任务完成事件通知
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

            // 处理 Reduce-Scatter 操作
        } else if (node->comm_type() ==
                   ChakraCollectiveCommType::REDUCE_SCATTER) {
            // 调用系统接口，生成 Reduce-Scatter 集合通信任务
            DataSet* fp =
                sys->generate_reduce_scatter(node->comm_size(), involved_dim,
                                             comm_group, node->comm_priority());

             // 记录任务 ID 映射
             collective_comm_node_id_map[fp->my_id] = node->id();
             // 存储任务 DataSet 结构
             collective_comm_wrapper_map[fp->my_id] = fp;
             // 设置任务完成事件通知
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);

        } else if (node->comm_type() == ChakraCollectiveCommType::BROADCAST) {
            // broadcast colelctive has not been implemented in ASTRA-SIM yet.
            // So, we just use its real system mesurements
    
            // ASTRA-SIM 目前尚未实现 Broadcast 集合通信
            // 这里直接使用真实系统测量的运行时间

            uint64_t runtime = 1ul; // 默认设定 1 纳秒
            if (node->runtime() != 0ul) {
                // chakra runtimes are in microseconds and we should convert it
                // into nanoseconds
                // Chakra 框架中的任务运行时间单位是微秒 (μs)，需要转换为纳秒 (ns)
                runtime = node->runtime() * 1000;
            }


             // 创建新的 DataSet 对象
             DataSet* fp = new DataSet(1);
             // 设置任务完成事件通知
             fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
             // 记录任务 ID 映射
             collective_comm_node_id_map[fp->my_id] = node->id();
             // 存储任务 DataSet 结构
             collective_comm_wrapper_map[fp->my_id] = fp;
             // 注册事件，让系统模拟任务执行时间
            sys->register_event(fp, EventType::General, nullptr,
                                // chakra runtimes are in microseconds and we
                                // should convert it into nanoseconds
                                runtime);
            // 再次设置任务完成事件通知，确保任务完成时被正确处理
            fp->set_notifier(this, EventType::CollectiveCommunicationFinished);
        }
        // 处理点对点发送 (Send) 任务
    } else if (node->type() == ChakraNodeType::COMM_SEND_NODE) {
        // 创建一个 sim_request 结构体来存储发送请求信息
        sim_request snd_req;
        snd_req.srcRank = node->comm_src(); // 设置数据包的源节点 ID
        snd_req.dstRank = node->comm_dst(); // 设置数据包的目标节点 ID
        snd_req.reqType = UINT8; // 设置请求类型为 UINT8（8 位无符号整数）

        // 创建一个 SendPacketEventHandlerData 结构体，用于处理发送数据包的事件
        SendPacketEventHandlerData* sehd = new SendPacketEventHandlerData;
        sehd->callable = this; // 设定当前 Workload 实例为事件的处理者
        sehd->wlhd = new WorkloadLayerHandlerData; // 分配新的 Workload 任务处理数据
        sehd->wlhd->node_id = node->id(); // 记录当前任务的节点 ID
        sehd->event = EventType::PacketSent; // 设置事件类型为 "数据包已发送"
        
        // 调用前端接口，模拟数据包的发送过程
        sys->front_end_sim_send(0, // 设备 ID，0 代表默认设备
            Sys::dummy_data, // 发送的数据（默认占位数据）
            node->comm_size(), // 数据包大小
            UINT8, // 数据类型
            node->comm_dst(), // 目标节点 ID
            node->comm_tag(), // 通信标签（用于标记不同的通信流）
            &snd_req, // 发送请求信息
            Sys::FrontEndSendRecvType::NATIVE, // 采用本地传输模式
            &Sys::handleEvent, // 事件处理函数
            sehd); // 事件处理数据
        // 处理点对点接收 (Recv) 任务
    } else if (node->type() == ChakraNodeType::COMM_RECV_NODE) {
        // 创建一个 sim_request 结构体来存储接收请求信息
        sim_request rcv_req;

        // 创建一个 RecvPacketEventHandlerData 结构体，用于处理接收数据包的事件
        RecvPacketEventHandlerData* rcehd = new RecvPacketEventHandlerData;
        rcehd->wlhd = new WorkloadLayerHandlerData; // 分配新的 Workload 任务处理数据
        rcehd->wlhd->node_id = node->id(); // 记录当前任务的节点 ID
        rcehd->workload = this; // 设定当前 Workload 实例为事件的处理者
        rcehd->event = EventType::PacketReceived; // 设置事件类型为 "数据包已接收"

        // 调用前端接口，模拟数据包的接收过程
        sys->front_end_sim_recv(0, // 设备 ID，0 代表默认设备
            Sys::dummy_data, // 接收的数据（默认占位数据）
            node->comm_size(), // 数据包大小
            UINT8, // 数据类型
            node->comm_src(), // 源节点 ID
            node->comm_tag(), // 通信标签（用于标记不同的通信流）
            &rcv_req, // 接收请求信息
            Sys::FrontEndSendRecvType::NATIVE, // 采用本地传输模式
            &Sys::handleEvent, // 事件处理函数
            rcehd); // 事件处理数据                            
    } else {
        // 处理未知的通信类型，记录错误并终止程序
        LoggerFactory::get_logger("workload") // 获取 "workload" 相关的日志记录器
            ->critical("Unknown communication node type"); // 记录关键错误信息
        exit(EXIT_FAILURE);
    }
}

// 释放无效节点，移除其子节点及自身
void Workload::skip_invalid(shared_ptr<Chakra::ETFeederNode> node) {
    et_feeder->freeChildrenNodes(node->id()); // 释放当前节点的所有子节点
    et_feeder->removeNode(node->id()); // 从 ETFeeder 中移除当前节点
}

// 事件回调函数，处理计算任务或通信任务的完成事件
void Workload::call(EventType event, CallData* data) {
    if (is_finished) { // 如果 workload 已完成，则直接返回
        return;
    }

    // 处理集合通信完成事件
    if (event == EventType::CollectiveCommunicationFinished) {
        IntData* int_data = (IntData*)data; // 将 CallData 转换为 IntData 类型
        hw_resource->tics_gpu_comms += int_data->execution_time; // 记录 GPU 通信时间
        uint64_t node_id = collective_comm_node_id_map[int_data->data]; // 获取对应的节点 ID
        shared_ptr<Chakra::ETFeederNode> node = et_feeder->lookupNode(node_id); // 查找对应的任务节点

        // 如果启用了 trace 记录，则记录调试信息
        if (sys->trace_enabled) {
            LoggerFactory::get_logger("workload")
                ->debug("callback,sys->id={}, tick={}, node->id={}, "
                        "node->name={}, node->type={}",
                        sys->id, Sys::boostedTick(), node->id(), node->name(),
                        static_cast<uint64_t>(node->type()));
        }

        hw_resource->release(node); // 释放该节点占用的硬件资源

        et_feeder->freeChildrenNodes(node_id); // 释放该节点的子节点

        issue_dep_free_nodes(); // 继续调度下一个可执行的任务

        // 清理 DataSet 相关数据，删除记录并释放内存
      
        // The Dataset class provides statistics that should be used later to dump
        // more statistics in the workload layer
        delete collective_comm_wrapper_map[int_data->data];
        collective_comm_wrapper_map.erase(int_data->data);
        et_feeder->removeNode(node_id); // 从 ETFeeder 中移除该任务节点

    } else { // 处理非集合通信任务的回调
        if (data == nullptr) { // 如果 data 为空，说明没有具体的任务数据
            issue_dep_free_nodes(); // 直接调度下一个可执行的任务
        } else {  // 处理常规任务完成事件
            WorkloadLayerHandlerData* wlhd = (WorkloadLayerHandlerData*)data; // 将 CallData 转换为 WorkloadLayerHandlerData
            shared_ptr<Chakra::ETFeederNode> node =
                et_feeder->lookupNode(wlhd->node_id); // 查找任务节点

            // 如果启用了 trace 记录，则记录调试信息
            if (sys->trace_enabled) {
                LoggerFactory::get_logger("workload")
                    ->debug("callback,sys->id={}, tick={}, node->id={}, "
                            "node->name={}, node->type={}",
                            sys->id, Sys::boostedTick(), node->id(),
                            node->name(), static_cast<uint64_t>(node->type()));
            }

            hw_resource->release(node);

            et_feeder->freeChildrenNodes(node->id());

            issue_dep_free_nodes();

            et_feeder->removeNode(wlhd->node_id);
            delete wlhd;
        }
    }

    // 检查是否所有任务都已完成
    if (!et_feeder->hasNodesToIssue() && // 没有可调度的任务
        (hw_resource->num_in_flight_cpu_ops == 0) && // 无 CPU 计算任务
        (hw_resource->num_in_flight_gpu_comp_ops == 0) && // 无 GPU 计算任务
        (hw_resource->num_in_flight_gpu_comm_ops == 0)) { // 无 GPU 通信任务
        report(); // 记录任务完成情况
        sys->comm_NI->sim_notify_finished(); // 通知系统所有任务已完成
        is_finished = true; // 标记 Workload 任务已完成
    }
}

// 触发通用事件，调用 call 进行处理
void Workload::fire() {
    call(EventType::General, NULL); // 触发通用事件处理，不附带数据
}

// 记录 Workload 执行的统计信息
void Workload::report() {
    Tick curr_tick = Sys::boostedTick(); // 获取当前的仿真时间
    // 在模拟器中，时间通常不是连续的，而是 事件驱动的，boostedTick() 记录了到目前为止 仿真器运行的总周期数。

    LoggerFactory::get_logger("workload")
        ->info("sys[{}] finished, {} cycles, exposed communication {} cycles.",
               sys->id, curr_tick, curr_tick - hw_resource->tics_gpu_ops);
    // 记录系统 ID，完成的总周期数，以及未被计算隐藏的通信时间
    // hw_resource->tics_gpu_ops 是 Workload 总 GPU 计算时间，即 GPU 真正执行计算任务的时间
    // curr_tick - hw_resource->tics_gpu_ops 计算的是 暴露的通信时间，即 通信操作无法隐藏在计算之下的时间。
}
