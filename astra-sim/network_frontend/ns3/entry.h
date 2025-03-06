#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include "common.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include <fstream>
#include <iostream>
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>
#include <time.h>
#include <unordered_map>

using namespace ns3;
using namespace std;

/*
 * This file defines the interaction between the System layer and the NS3
 * simulator (Network layer). The system layer issues send/receive events, and
 * waits until the ns3 simulates the conclusion of these events to issue the
 * next collective communication. When ns3 simulates the conclusion of an event,
 * it will call qp_finish to lookup the maps in this file and call the callback
 * handlers. Refer to below comments for further detail.
 */

/*
 * 该文件用于管理 System Layer (系统层) 与 NS3 网络仿真器 (Network Layer) 的交互。
 * System Layer 负责发起 send/receive (发送/接收) 事件，并等待 NS3 进行仿真，
 * 直到这些事件完成后，System Layer 才会继续执行下一个集合通信。
 * 当 NS3 处理完一个事件时，会调用 qp_finish()，
 * 该函数负责查找映射关系并调用回调处理函数。
 */

// MsgEvent represents a single send or receive event, issued by the system
// layer. The system layer will wait for the ns3 backend to simulate the event
// finishing (i.e. node 0 finishes sending message, or node 1 finishes receiving
// the message) The callback handler 'msg_handler' signals the System layer that
// the event has finished in ns3.

// MsgEvent 代表系统层发出的单个发送或接收事件。
// 该事件由系统层触发，等待 ns3 后端模拟事件完成（即节点 0 完成消息发送，
// 或节点 1 完成消息接收）。事件完成后，回调函数 'msg_handler' 会通知系统层。
class MsgEvent {
public:
  int src_id; // 发送方节点 ID
  int dst_id; // 接收方节点 ID
  int type;   // 事件类型（可用于区分不同的消息类别）
  // Indicates the number of bytes remaining to be sent or received.
  // Initialized with the original size of the message, and
  // incremented/decremented depending on how many bytes were sent/received.
  // Eventually, this value will reach 0 when the event has completed.

   // 剩余待发送或接收的字节数：
  // 该变量初始化为消息的原始大小，并根据发送或接收的字节数进行递减或递增。
  // 当该值减小至 0 时，表示消息传输完成。
  int remaining_msg_bytes;

  void *fun_arg; // 传递给回调函数的参数
  void (*msg_handler)(void *fun_arg); // 消息完成后的回调处理函数
  
  // 构造函数：用于初始化消息事件
  MsgEvent(int _src_id, int _dst_id, int _type, int _remaining_msg_bytes,
           void *_fun_arg, void (*_msg_handler)(void *fun_arg))
      : src_id(_src_id), dst_id(_dst_id), type(_type),
        remaining_msg_bytes(_remaining_msg_bytes), fun_arg(_fun_arg),
        msg_handler(_msg_handler) {}

  // Default constructor to prevent compile errors. When looking up MsgEvents
  // from maps such as sim_send_waiting_hash, we should always check that a MsgEvent exists
  // for the given key. (i.e. this default constructor should not be called in
  // runtime.)

   // 默认构造函数：防止编译错误
  // 该默认构造函数主要用于在哈希表（如 sim_send_waiting_hash）中进行查找时，
  // 避免未找到相应 MsgEvent 时出现未初始化对象的错误。
  // 但在运行时不应实际调用该构造函数。
  MsgEvent()
      : src_id(0), dst_id(0), type(0), remaining_msg_bytes(0), fun_arg(nullptr),
        msg_handler(nullptr) {}

  // CallHandler will call the callback handler associated with this MsgEvent.
  // callHandler 方法：调用与该 MsgEvent 关联的回调函数
  void callHandler() {
    msg_handler(fun_arg);
    return;
  }
};

// MsgEventKey is a key to uniquely identify each MsgEvent.
//  - Pair <Tag, Pair <src_id, dst_id>>

// MsgEventKey 是用于唯一标识每个 MsgEvent 的键。
// 该键由一个 `tag` 和一个 `src_id, dst_id` 对组成。
// 其中 `tag` 用于区分不同的消息，而 `src_id, dst_id` 表示消息的发送和接收节点。
typedef pair<int, pair<int, int>> MsgEventKey;

// The ns3 RdmaClient structure cannot hold the 'tag' information, which is a
// Astra-sim specific implementation. We use a mapping with the source port
// number (another unique value) to hold tag information.
//   - key: Pair <port_id, Pair <src_id, dst_id>>
//   - value: tag
// TODO: It seems we *can* obtain the tag through q->GetTag() at qp_finish.
// Verify & Simplify.

// 由于 ns3 的 RdmaClient 结构无法存储 `tag` 信息，而 `tag` 是 Astra-sim 特有的实现，
// 我们通过源端口号（port_id）作为唯一标识来存储 `tag` 信息。
//   - key: <port_id, <src_id, dst_id>>
//   - value: tag
// TODO: 可能可以通过 `q->GetTag()` 在 `qp_finish` 获取 tag，待验证并简化代码。
map<pair<int, pair<int, int>>, int> sender_src_port_map;

// NodeHash is used to count how many bytes were sent/received by this node.
// Refer to sim_finish().
//   - key: Pair <node_id, send/receive>. Where 'send/receive' indicates if the
//   value is for send or receive
//   - value: Number of bytes this node has sent (if send/receive is 0) and
//   received (if send/receive is 1)

// node_to_bytes_sent_map 用于统计每个节点发送或接收的字节数。
// 该映射在 `sim_finish()` 函数中被引用。
//   - key: <node_id, 发送/接收>。其中 `0` 表示发送，`1` 表示接收。
//   - value: 该节点已经发送（send=0）或接收（receive=1）的字节数。
map<pair<int, int>, int> node_to_bytes_sent_map;

// SentHash stores a MsgEvent for sim_send events and its callback handler.
//   - key: A pair of <MsgEventKey, port_id>. 
//          A single collective phase can be split into multiple sim_send messages, which all have the same MsgEventKey. 
//          TODO: Adding port_id as key is a hacky solution. The real solution would be to split this map, similar to sim_recv_waiting_hash and received_msg_standby_hash.
//   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
//   send event to finish

// sim_send_waiting_hash 存储 sim_send 事件的 MsgEvent 及其回调处理函数。
//   - key: <MsgEventKey, port_id>
//   - value: MsgEvent，表示 Sys 层正在等待该事件完成。
// 由于一个集合通信阶段可能会拆分成多个 sim_send 消息，因此需要使用 MsgEventKey 进行索引。
// TODO: 目前 `port_id` 被加入 key 作为临时解决方案，理想的做法是将该映射拆分，
// 类似 `sim_recv_waiting_hash` 和 `received_msg_standby_hash`。
map<pair<MsgEventKey, int>, MsgEvent> sim_send_waiting_hash;

// While ns3 cannot send packets before System layer calls sim_send, it
// is possible for ns3 to simulate Incoming messages before System layer calls
// sim_recv to 'reap' the messages. Therefore, we maintain two maps:
//   - sim_recv_waiting_hash holds messages where sim_recv has been called but ns3 has
//   not yet simulated the message arriving,
//   - received_msg_standby_hash holds messages which ns3 has simulated the arrival, but sim_recv
//   has not yet been called.

//   - key: A MsgEventKey isntance.
//   - value: A MsgEvent instance that indicates that Sys layer is waiting for a
//   receive event to finish

// ns3 不能在 System 层调用 sim_send 之前发送数据，但 ns3 可能会在 System 层
// 调用 sim_recv 之前模拟消息的到达。因此我们维护两个映射：
//   - sim_recv_waiting_hash 存储 System 层已经调用 sim_recv 但 ns3 还未模拟到达的消息。
//   - received_msg_standby_hash 存储 ns3 已经模拟到达但 System 层尚未调用 sim_recv 领取的消息。

// sim_recv_waiting_hash 存储 Sys 层等待接收的事件。
//   - key: MsgEventKey
//   - value: MsgEvent，表示 Sys 层正在等待该接收事件完成。
map<MsgEventKey, MsgEvent> sim_recv_waiting_hash;

//   - key: A MsgEventKey isntance.
//   - value: The number of bytes that ns3 has simulated completed, but the
//   System layer has not yet called sim_recv
// received_msg_standby_hash 记录 ns3 已模拟完成但 System 层尚未调用 sim_recv 领取的数据。
//   - key: MsgEventKey
//   - value: ns3 已模拟完成但未被领取的字节数。
map<MsgEventKey, int> received_msg_standby_hash;

// send_flow commands the ns3 simulator to schedule a RDMA message to be sent
// between two pair of nodes. send_flow is triggered by sim_send.
// send_flow 指示 ns3 模拟器在两个节点之间安排一个 RDMA 消息传输。
// 该函数在 sim_send 触发时被调用。
// 它主要执行以下任务：
// 1. 为 RDMA 传输分配一个新的端口号，并将 `tag` 存入 `sender_src_port_map`。
// 2. 创建 `MsgEvent` 实例，记录传输事件，并存入 `sim_send_waiting_hash` 以便后续查询。
// 3. 创建 `RdmaClientHelper` 并在 ns3 模拟器中安排 RDMA 消息传输。
void send_flow(int src_id, int dst, int maxPacketCount,
               void (*msg_handler)(void *fun_arg), void *fun_arg, int tag) {
  // Get a new port number.
  // 生成新的端口号，并在 `sender_src_port_map` 记录该端口和 `tag` 的映射关系
  uint32_t port = portNumber[src_id][dst]++;
  sender_src_port_map[make_pair(port, make_pair(src_id, dst))] = tag;
  int pg = 3, dport = 100;
  flow_input.idx++;

  // Create a MsgEvent instance and register callback function.
  // 创建 MsgEvent 实例并注册回调函数
  MsgEvent send_event =
      MsgEvent(src_id, dst, 0, maxPacketCount, fun_arg, msg_handler);
  pair<MsgEventKey, int> send_event_key =
      make_pair(make_pair(tag, make_pair(send_event.src_id, send_event.dst_id)),port) ;
  sim_send_waiting_hash[send_event_key] = send_event;

  // Create a queue pair and schedule within the ns3 simulator.
  // 创建队列对（Queue Pair）并在 ns3 模拟器中安排传输
  RdmaClientHelper clientHelper(
      pg, serverAddress[src_id], serverAddress[dst], port, dport,
      maxPacketCount,
      has_win ? (global_t == 1 ? maxBdp : pairBdp[n.Get(src_id)][n.Get(dst)])
              : 0,
      global_t == 1 ? maxRtt : pairRtt[src_id][dst], msg_handler, fun_arg, tag,
      src_id, dst);
  ApplicationContainer appCon = clientHelper.Install(n.Get(src_id));
  appCon.Start(Time(0));
}

// notify_receiver_receive_data looks at whether the System layer has issued
// sim_recv for this message. If the system layer is waiting for this message,
// call the callback handler for the MsgEvent. If the system layer is not *yet*
// waiting for this message, register that this message has arrived,
// so that the system layer can later call the callback handler when sim_recv
// is called.

// notify_receiver_receive_data 处理数据接收逻辑。
// 该函数检查 System 层是否已经调用 sim_recv 领取该消息。
// 1. 如果 System 层已经等待该消息，则调用 `MsgEvent` 的回调函数通知其完成。
// 2. 如果 System 层尚未调用 sim_recv，则将数据存入 `received_msg_standby_hash`，等待后续领取。
// 3. 更新 `node_to_bytes_sent_map` 记录接收方已接收的字节数。
void notify_receiver_receive_data(int src_id, int dst_id, int message_size,
                                  int tag) {

  // 生成 `MsgEventKey` 用于查询接收方等待的消息
  MsgEventKey recv_expect_event_key = make_pair(tag, make_pair(src_id, dst_id));

  if (sim_recv_waiting_hash.find(recv_expect_event_key) != sim_recv_waiting_hash.end()) {
    // The Sys object is waiting for packets to arrive.
    // System 层正在等待该消息
    MsgEvent recv_expect_event = sim_recv_waiting_hash[recv_expect_event_key];
    if (message_size == recv_expect_event.remaining_msg_bytes) {
      // We received exactly the amount of data what Sys object was expecting.
      // 收到的字节数正好等于 System 层期望的字节数
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else if (message_size > recv_expect_event.remaining_msg_bytes) {
      // We received more packets than the Sys object is expecting.
      // Place task in received_msg_standby_hash and wait for Sys object to issue more sim_recv
      // calls. Call callback handler for the amount Sys object was waiting for.
      // 收到的数据量超过 System 层预期，将多余部分存入 `received_msg_standby_hash`
      received_msg_standby_hash[recv_expect_event_key] =
          message_size - recv_expect_event.remaining_msg_bytes;
      sim_recv_waiting_hash.erase(recv_expect_event_key);
      recv_expect_event.callHandler();
    } else {
      // There are still packets to arrive.
      // Reduce the number of packets we are waiting for. Do not call callback
      // handler.
      // 收到的数据量少于 System 层预期，更新 `remaining_msg_bytes`
      recv_expect_event.remaining_msg_bytes -= message_size;
      sim_recv_waiting_hash[recv_expect_event_key] = recv_expect_event;
    }
  } else {
    // The Sys object is not yet waiting for packets to arrive.
    // System 层尚未调用 sim_recv，存入 `received_msg_standby_hash`
    if (received_msg_standby_hash.find(recv_expect_event_key) == received_msg_standby_hash.end()) {
      // Place task in received_msg_standby_hash and wait for Sys object to issue more sim_recv
      // calls.
      received_msg_standby_hash[recv_expect_event_key] = message_size;
    } else {
      // Sys object is still waiting. Add number of bytes we are waiting for.
      received_msg_standby_hash[recv_expect_event_key] += message_size;
    }
  }

  // Add to the number of total bytes received.
  // 记录节点的接收字节数
  if (node_to_bytes_sent_map.find(make_pair(dst_id, 1)) == node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(dst_id, 1)] += message_size;
  }
}

// notify_sender_sending_finished 通知发送方消息已完成传输。
// 该函数在 ns3 完成消息传输时被调用，用于:
// 1. 查找并验证对应的 MsgEvent
// 2. 确保传输的消息大小与系统层期望值匹配
// 3. 记录发送方发送的字节数
// 4. 调用 MsgEvent 的回调函数通知系统层
void notify_sender_sending_finished(int src_id, int dst_id, int message_size,
                                    int tag, int src_port) {
  // Lookup the send_event registered at send_flow().
  // 生成唯一键值，以查找 send_flow() 中注册的 send_event
  pair<MsgEventKey, int> send_event_key = make_pair(make_pair(tag, make_pair(src_id, dst_id)), src_port);
  
  // 查找 sim_send_waiting_hash，确保消息事件存在
  if (sim_send_waiting_hash.find(send_event_key) == sim_send_waiting_hash.end()) {
    cerr << "Cannot find send_event in sent_hash. Something is wrong."
         << "tag, src_id, dst_id: " << tag << " " << src_id << " " << dst_id
         << "\n";
    exit(1); // 发生错误，终止程序
  }

  // Verify that the (ns3 identified) sent message size matches what was
  // expected by the system layer.
  // 获取存储的 MsgEvent
  MsgEvent send_event = sim_send_waiting_hash[send_event_key];

  // 验证 ns3 传输的消息大小是否与系统层期望的大小匹配
  if (send_event.remaining_msg_bytes != message_size) {
    cerr << "The message size does not match what is expected. Something is "
            "wrong."
         << "tag, src_id, dst_id, expected msg_bytes, actual msg_bytes: " << tag
         << " " << src_id << " " << dst_id << " "
         << send_event.remaining_msg_bytes << " " << message_size << "\n";
    exit(1);  // 发生错误，终止程序
  }

  // 消息传输完成，移除该事件
  sim_send_waiting_hash.erase(send_event_key);

  // Add to the number of total bytes sent.
  // 记录节点发送的总字节数
  if (node_to_bytes_sent_map.find(make_pair(src_id, 0)) == node_to_bytes_sent_map.end()) {
    node_to_bytes_sent_map[make_pair(src_id, 0)] = message_size;
  } else {
    node_to_bytes_sent_map[make_pair(src_id, 0)] += message_size;
  }
  // 触发回调函数，通知系统层消息已发送完成
  send_event.callHandler();
}

// qp_finish_print_log 记录 RDMA 传输完成的日志信息。
// 该函数会计算传输的总字节数，并记录相关的网络性能指标，如 FCT（Flow Completion Time）。
void qp_finish_print_log(FILE *fout, Ptr<RdmaQueuePair> q) {
  // 获取源节点 ID (sid) 和目标节点 ID (did)
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);

  // 获取基准往返时间 (RTT) 和带宽 (b)（单位: 比特/秒）
  uint64_t base_rtt = pairRtt[sid][did], b = pairBw[sid][did];

  // 计算传输的总字节数：
  // 包括数据负载 q->m_size 和额外的协议头开销
  uint32_t total_bytes =
      q->m_size +
      ((q->m_size - 1) / packet_payload_size + 1) *
          (CustomHeader::GetStaticWholeHeaderSize() -
           IntHeader::GetStaticSize()); // translate to the minimum bytes
                                        // required (with header but no INT)
                                        // 计算数据包的最小传输字节数（包含头部，但不含 INT）

  // 计算独立流的完成时间（standalone FCT）：基本 RTT + 传输时间
  uint64_t standalone_fct = base_rtt + total_bytes * 8000000000lu / b;

  // 记录 RDMA 传输完成信息：
  //   - 源 IP、目标 IP、源端口、目标端口、传输数据大小、开始时间、完成时间、预计独立流完成时间
  // sip, dip, sport, dport, size (B), start_time, fct (ns), standalone_fct (ns)
  fprintf(fout, "%08x %08x %u %u %lu %lu %lu %lu\n", q->sip.Get(), q->dip.Get(),
          q->sport, q->dport, q->m_size, q->startTime.GetTimeStep(),
          (Simulator::Now() - q->startTime).GetTimeStep(), standalone_fct);
  fflush(fout); // 确保日志立即写入文件
}

// qp_finish is triggered by NS3 to indicate that an RDMA queue pair has
// finished. qp_finish is registered as the callback handlerto the RdmaClient
// instance created at send_flow. This registration is done at
// common.h::SetupNetwork().

// qp_finish 由 ns3 触发，表示 RDMA 队列对 (QP) 传输完成。
// 该函数由 `SetupNetwork()` 注册到 ns3 作为回调函数，在 send_flow 创建的 RdmaClient 实例中使用。
void qp_finish(FILE *fout, Ptr<RdmaQueuePair> q) {

  // 获取源节点 ID (sid) 和目标节点 ID (did)
  uint32_t sid = ip_to_node_id(q->sip), did = ip_to_node_id(q->dip);

  // 记录传输完成的日志信息
  qp_finish_print_log(fout, q);

  // remove rxQp from the receiver.
  // 从接收方节点 (dstNode) 中删除接收队列对 (rxQp)
  Ptr<Node> dstNode = n.Get(did);
  Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
  rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);

  // Identify the tag of this message.
  // 识别消息的 tag
  if (sender_src_port_map.find(make_pair(q->sport, make_pair(sid, did))) ==
      sender_src_port_map.end()) {
    cout << "could not find the tag, there must be something wrong" << endl;
    exit(-1); // 若未找到 tag，则程序终止
  }
  int tag = sender_src_port_map[make_pair(q->sport, make_pair(sid, did))];

  // 删除已使用的端口映射
  sender_src_port_map.erase(make_pair(q->sport, make_pair(sid, did)));

  // Let sender knows that the flow has finished.
  // 通知发送方：数据传输完成
  notify_sender_sending_finished(sid, did, q->m_size, tag, q->sport);

  // Let receiver knows that it has received packets.
  // 通知接收方：数据已成功接收
  notify_receiver_receive_data(sid, did, q->m_size, tag);
}

// setup_ns3_simulation 用于初始化 ns3 模拟环境。
// 该函数执行以下任务：
// 1. 读取网络配置文件，并解析相关参数。
// 2. 设置全局配置，如系统参数、网络拓扑等。
// 3. 调用 SetupNetwork() 初始化 ns3 网络，并注册 qp_finish 作为 RDMA 传输完成的回调函数。
// 如果任一步骤失败，返回 -1；成功则返回 0。
int setup_ns3_simulation(string network_configuration) {

    // 读取并解析网络配置文件
  if (!ReadConf(network_configuration))
    return -1;

  // 设置全局网络配置
  SetConfig();

  // 初始化 ns3 网络，并注册 qp_finish 作为 RDMA 传输完成的回调函数
  if (!SetupNetwork(qp_finish)) {
    return -1; // 网络初始化失败，返回 -1
  }

  return 0; // 成功初始化，返回 0


}
