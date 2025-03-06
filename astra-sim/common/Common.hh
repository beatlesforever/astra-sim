/******************************************************************************
This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*******************************************************************************/

#ifndef __COMMON_HH__
#define __COMMON_HH__

#include <cstdint>  // 包含固定宽度整数类型定义，例如 uint32_t、uint64_t
#include <string>   // 包含 std::string 类，用于字符串处理

namespace AstraSim {  // 定义 AstraSim 命名空间，避免名称冲突

/**
 * @typedef Tick
 * @brief 定义时间步长的类型，表示时间单位。
 */
typedef unsigned long long Tick;  // Tick 使用 64 位无符号整数表示模拟时间的步长

/**
 * @brief 定义 AstraSim 的时钟参数。
 */
constexpr uint64_t CLOCK_PERIOD = 1;           // 时钟周期：1 纳秒 (ns)
constexpr uint64_t FREQ = 1000 * 1000 * 1000;  // 时钟频率：1 GHz (10^9 Hz)

/**
 * @enum time_type_e
 * @brief 定义时间单位类型，用于表示模拟系统中的时间刻度。
 */
enum time_type_e { 
    SE = 0,  ///< 以秒 (Seconds) 作为时间单位
    MS,      ///< 以毫秒 (Milliseconds) 作为时间单位
    US,      ///< 以微秒 (Microseconds) 作为时间单位
    NS,      ///< 以纳秒 (Nanoseconds) 作为时间单位
    FS       ///< 以飞秒 (Femtoseconds) 作为时间单位
};

/**
 * @enum req_type_e
 * @brief 定义请求的数据类型。
 * 
 * 该枚举用于指定不同的数据格式，在模拟数据传输时使用。
 */
enum req_type_e { 
    UINT8 = 0,  ///< 8 位无符号整数
    BFLOAT16,   ///< 16 位 BFloat 数值格式
    FP32        ///< 32 位 IEEE 浮点数
};

/**
 * @struct timespec_t
 * @brief 存储时间信息的结构体。
 * 
 * 该结构体用于存储某个事件发生的时间，包括时间单位和具体的时间值。
 */
struct timespec_t {
    time_type_e time_res; ///< 该时间值的单位（秒、毫秒、微秒等）
    long double time_val; ///< 时间值，使用长双精度浮点数表示，以提高精度
};

/**
 * @struct sim_request
 * @brief 存储模拟请求的信息。
 * 
 * 该结构体用于表示一个模拟的通信请求，包括发送方、接收方、数据类型等。
 */
struct sim_request {
    uint32_t srcRank;    ///< 发送方的 Rank（节点 ID）
    uint32_t dstRank;    ///< 接收方的 Rank（节点 ID）
    uint32_t tag;        ///< 消息标签，用于标识不同的消息
    req_type_e reqType;  ///< 请求的数据类型（UINT8、BFLOAT16、FP32）
    uint64_t reqCount;   ///< 传输的数据数量（以数据项为单位）
    uint32_t vnet;       ///< 虚拟网络编号（用于区分多个网络通道）
    uint32_t layerNum;   ///< 计算层编号（用于标识计算的层次）
};

/**
 * @class MetaData
 * @brief 用于存储元数据，例如时间戳。
 */
class MetaData {
    public:
      timespec_t timestamp; ///< 记录事件的时间戳
  };

/**
 * @enum ComType
 * @brief 定义集合通信（Collective Communication）类型。
 */
enum class ComType {
    None = 0,               ///< 无集合通信
    Reduce_Scatter,         ///< 归约-散播（Reduce-Scatter）
    All_Gather,             ///< 全部收集（All-Gather）
    All_Reduce,             ///< 全部归约（All-Reduce）
    All_to_All,             ///< 全部对全部通信（All-to-All）
    All_Reduce_All_to_All   ///< 归约 + 全部对全部（混合模式）
};

/**
 * @enum CollectiveOptimization
 * @brief 定义集合通信（Collective Communication）的优化策略。
 * 
 * 该枚举表示不同的集合通信优化方式，例如是否基于本地带宽感知进行优化。
 */
enum class CollectiveOptimization {
    Baseline = 0,  ///< 无优化，采用默认的集合通信方式
    LocalBWAware   ///< 启用基于本地带宽（Bandwidth-Aware）的优化
};

/**
 * @enum CollectiveImplType
 * @brief 定义集合通信（Collective Communication）的实现方式。
 * 
 * 该枚举描述了不同的集合通信算法，每种算法适用于不同的网络拓扑结构。
 */
enum class CollectiveImplType {
    Ring = 0,                      ///< 环形（Ring）通信拓扑
    OneRing,                        ///< 单环拓扑（OneRing）
    Direct,                         ///< 直接通信（Direct）
    OneDirect,                      ///< 单层直接通信（OneDirect）
    AllToAll,                       ///< 全部对全部（All-to-All）
    DoubleBinaryTreeLocalAllToAll,   ///< 本地双二叉树 + 全部对全部（DBT + A2A）
    LocalRingNodeA2AGlobalDBT,       ///< 本地环 + 全局双二叉树
    HierarchicalRing,                ///< 分层环形拓扑
    DoubleBinaryTree,                ///< 双二叉树（Double Binary Tree）
    HalvingDoubling,                 ///< 二分合并（Halving-Doubling）
    OneHalvingDoubling,              ///< 单层二分合并（One Halving-Doubling）
    ChakraImpl                       ///< 使用 Chakra 框架实现的集合通信
};

/**
 * @enum CollectiveBarrier
 * @brief 定义集合通信中的同步机制。
 */
enum class CollectiveBarrier {
    Blocking = 0,  ///< 阻塞式同步，必须等待通信完成
    Non_Blocking   ///< 非阻塞同步，可以在通信未完成时执行其他操作
};

/**
 * @enum SchedulingPolicy
 * @brief 定义任务调度策略（Job Scheduling Policy）。
 */
enum class SchedulingPolicy {
    LIFO = 0,     ///< 后进先出（Last In, First Out）
    FIFO,         ///< 先进先出（First In, First Out）
    EXPLICIT,     ///< 显式调度（Explicitly Specified）
    None          ///< 无调度策略
};

/**
 * @enum IntraDimensionScheduling
 * @brief 定义同一维度内的任务调度策略。
 */
enum class IntraDimensionScheduling {
    FIFO = 0,               ///< 先进先出调度（FIFO）
    RG,                     ///< 轮询调度（Round-Robin Greedy）
    SmallestFirst,          ///< 优先处理最小的任务
    LessRemainingPhaseFirst ///< 优先处理剩余阶段最少的任务
};

/**
 * @enum InterDimensionScheduling
 * @brief 定义跨维度的任务调度策略。
 */
enum class InterDimensionScheduling {
    Ascending = 0,        ///< 按照维度升序调度
    OnlineGreedy,         ///< 在线贪心调度（Online Greedy）
    RoundRobin,           ///< 轮询调度（Round-Robin）
    OfflineGreedy,        ///< 离线贪心调度（Offline Greedy）
    OfflineGreedyFlex     ///< 灵活的离线贪心调度
};

/**
 * @enum InjectionPolicy
 * @brief 定义负载注入策略（Load Injection Policy）。
 */
enum class InjectionPolicy {
    Infinite = 0,      ///< 无限带宽注入
    Aggressive,        ///< 激进策略
    SemiAggressive,    ///< 半激进策略
    ExtraAggressive,   ///< 超激进策略
    Normal             ///< 正常策略
};

/**
 * @enum PacketRouting
 * @brief 定义数据包路由方式。
 */
enum class PacketRouting {
    Hardware = 0,  ///< 硬件路由
    Software       ///< 软件路由
};

/**
 * @enum BusType
 * @brief 定义系统中使用的总线类型。
 */
enum class BusType {
    Both = 0,   ///< 共享总线和内存总线均可用
    Shared,     ///< 共享总线
    Mem         ///< 内存总线
};

/**
 * @enum StreamState
 * @brief 定义流的生命周期状态（Stream State）。
 */
enum class StreamState {
    Created = 0,      ///< 流已创建（Created）
    Transferring,     ///< 数据正在传输（Transferring）
    Ready,           ///< 数据传输完成，准备执行（Ready）
    Executing,       ///< 流正在执行（Executing）
    Zombie,         ///< 进入僵尸状态，可能等待回收（Zombie）
    Dead            ///< 流终止（Dead）
};

/**
 * @enum EventType
 * @brief 定义事件类型，用于模拟计算和通信过程中的不同事件。
 * 
 * 该枚举表示在 AstraSim 模拟过程中可能发生的事件类型，包括数据传输、
 * 计算完成、流状态变化、集合通信等。
 */
enum class EventType {
    CallEvents = 0,   ///< 事件调用，表示一般的事件触发
    General,          ///< 普通事件，未具体分类

    RendezvousSend,   ///< 交会发送（Rendezvous Send）
                      ///< 该事件表示节点正在进行 Rendezvous 发送，即数据尚未发送出去，
                      ///< 但已与接收方进行握手，等待条件满足后执行传输。

    RendezvousRecv,   ///< 交会接收（Rendezvous Receive）
                      ///< 该事件表示节点已准备好接收数据，但数据尚未到达，
                      ///< 需要等待发送方完成数据传输。

    PacketReceived,   ///< 数据包接收完成（Packet Received）
                      ///< 该事件表示一个数据包已经成功到达接收方，并存储在合适的缓冲区。

    PacketSent,       ///< 数据包发送完成（Packet Sent）
                      ///< 该事件表示一个数据包已经成功发送出去，不再位于发送队列中。

    Rec_Finished,     ///< 接收完成（Receive Finished）
                      ///< 该事件表示接收端完成了数据的接收，并可以进行下一步计算或传输。

    Send_Finished,    ///< 发送完成（Send Finished）
                      ///< 该事件表示发送端完成了数据传输，并可以释放发送缓冲区。

    Processing_Finished, ///< 处理完成（Processing Finished）
                         ///< 该事件表示计算处理任务已执行完成，可能涉及张量计算或神经网络推理。

    NPU_to_MA,        ///< 从 NPU 到 Memory Accelerator（NPU to MA）
                      ///< 该事件表示数据从 NPU（神经处理单元）传输到 MA（内存加速器）。

    MA_to_NPU,        ///< 从 Memory Accelerator 到 NPU（MA to NPU）
                      ///< 该事件表示数据从 MA 传输回 NPU，用于计算任务。

    Consider_Process, ///< 考虑处理（Consider Process）
                      ///< 该事件表示需要检查是否可以执行计算任务，
                      ///< 可能涉及计算资源的调度。

    Consider_Retire,  ///< 考虑退休（Consider Retire）
                      ///< 该事件表示当前任务可能完成，检查是否可以释放相关资源。

    Consider_Send_Back, ///< 考虑返回数据（Consider Send Back）
                        ///< 该事件表示系统可能需要将数据返回给请求方，例如层间数据传输。

    StreamInit,       ///< 流初始化（Stream Initialization）
                      ///< 该事件表示一个新的数据流正在初始化，包括分配资源和建立连接。

    CommProcessingFinished, ///< 通信处理完成（Communication Processing Finished）
                            ///< 该事件表示通信任务的处理阶段已完成，例如数据打包、解包等。

    CollectiveCommunicationFinished, ///< 集合通信完成（Collective Communication Finished）
                                     ///< 该事件表示一次集合通信操作（如 AllReduce、AllGather）已完成，
                                     ///< 相关数据已到达所有参与节点。

    CompFinished,     ///< 计算完成（Computation Finished）
                      ///< 该事件表示计算任务已完成，可能涉及矩阵乘法、神经网络推理等操作。

    MemLoadFinished,  ///< 内存加载完成（Memory Load Finished）
                      ///< 该事件表示从内存加载数据的操作已经完成。

    MemStoreFinished  ///< 内存存储完成（Memory Store Finished）
                      ///< 该事件表示数据已成功存储到内存，可能涉及参数更新或缓存操作。
};

/**
 * @class CloneInterface
 * @brief 定义克隆接口，用于支持对象复制。
 */
class CloneInterface {
    public:
      virtual CloneInterface* clone() const = 0; ///< 纯虚函数，要求子类实现克隆方法
      virtual ~CloneInterface() = default;      ///< 虚析构函数，确保正确释放资源
  };

/*
 * CollectiveImpl holds the user's description on how a collective algorithm is
 * implemented, provided in the System layer input.
 */
/**
 * @class CollectiveImpl
 * @brief 定义集合通信的基本实现方式。
 */
class CollectiveImpl : public CloneInterface {
    public:
      /**
       * @brief 构造函数，初始化集合通信的类型。
       */
      CollectiveImpl(CollectiveImplType type) {
          this->type = type;
      };
  
      /**
       * @brief 克隆当前对象，返回 CollectiveImpl 的副本。
       */
      virtual CloneInterface* clone() const {
          return new CollectiveImpl(*this);
      }
  
      CollectiveImplType type; ///< 记录集合通信的类型
  };

/*
 * DirectCollectiveImpl contains user-specified information about Direct
 * implementation of collective algorithms. We have a separte class for
 * DirectCollectiveImpl, because of the collective window, which is also defined
 * in the system layer input.
 */
/**
 * @class DirectCollectiveImpl
 * @brief 直接集合通信的实现，增加窗口参数。
 */
class DirectCollectiveImpl : public CollectiveImpl {
    public:
      /**
       * @brief 克隆当前 DirectCollectiveImpl 对象。
       */
      CloneInterface* clone() const {
          return new DirectCollectiveImpl(*this);
      };
  
      /**
       * @brief 构造函数，初始化直接集合通信的类型和窗口大小。
       */
      DirectCollectiveImpl(CollectiveImplType type, int direct_collective_window)
          : CollectiveImpl(type) {
          this->direct_collective_window = direct_collective_window;
      }
  
      int direct_collective_window; ///< 直接集合通信的窗口大小
  };
/*
 * ChakraCollectiveImpl contains information about a collective implementation
 * represented using the Chakra ET format. It containes the filename of the
 * Chakra ET which holds the implementation, provided in the System layer input.
 */
/**
 * @class ChakraCollectiveImpl
 * @brief 使用 Chakra 形式表示集合通信的实现方式。
 */
class ChakraCollectiveImpl : public CollectiveImpl {
    public:
      /**
       * @brief 克隆当前 ChakraCollectiveImpl 对象。
       */
      CloneInterface* clone() const {
          return new ChakraCollectiveImpl(*this);
      };
  
      /**
       * @brief 构造函数，初始化 Chakra 形式的集合通信实现。
       */
      ChakraCollectiveImpl(CollectiveImplType type, std::string filename)
          : CollectiveImpl(type) {
          this->filename = filename;
      }
  
      std::string filename; ///< 存储 Chakra 具体实现的文件路径
  };
  
}  // namespace AstraSim

#endif /* __COMMON_HH__ */
