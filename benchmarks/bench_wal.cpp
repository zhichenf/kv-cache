#include <benchmark/benchmark.h>
#include "common/wal.h"
#include <filesystem>

// ============================================================
// Google Benchmark 测试原理：
//
// 框架会自动调整 for 循环的次数，直到结果稳定（相对误差 < 1%）。
// 输出的 Time = 总耗时 / 迭代次数 = 每次操作的平均耗时。
//
// 多线程测试（->Threads(n)）：
// 框架创建 n 个线程，每个线程独立运行被测函数，
// 测的是 n 个线程并发时的总吞吐量。
//
// items_per_second = iterations / time，表示每秒能处理多少个元素。
// ============================================================

namespace fs = std::filesystem;

static std::string TempDir() {
    auto p = fs::temp_directory_path() / "kv_cache_bench";
    fs::create_directories(p);
    return p.string();
}

// ============================================================
// 测试1：单线程写入，NO 策略（不主动 fsync）
//
// 测的是：纯写入速度，不考虑磁盘持久化
// 预期结果：最快，因为没有 fsync 开销
// ============================================================
static void BM_WAL_Single_NoSync(benchmark::State& state) {
    std::string dir = TempDir() + "/single_nosync";
    fs::create_directories(dir);
    std::string filepath = dir + "/wal.log";

    WALConfig config;
    config.policy = FsyncPolicy::NO;
    auto& wal = WAL::GetInstance();
    wal.Init(filepath, config);  // 打开 WAL 文件

    std::string value(128, 'x');  // 模拟 128 字节的 value

    // 框架会自动循环这个 for，直到结果稳定
    // 每次循环 = 一次 Append 操作
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key", value};
        wal.Append(rec);  // 写入一条记录
    }

    state.SetItemsProcessed(state.iterations());  // 告诉框架每次迭代处理了 1 个元素
    wal.Shutdown();
    fs::remove_all(dir);
}
BENCHMARK(BM_WAL_Single_NoSync)->Unit(benchmark::kMicrosecond);

// ============================================================
// 测试2：单线程写入，ALWAYS 策略（每条都 fsync）
//
// 测的是：带磁盘持久化的写入速度
// 预期结果：比 NoSync 慢很多（fsync 约 10ms）
// ============================================================
static void BM_WAL_Single_Always(benchmark::State& state) {
    std::string dir = TempDir() + "/single_always";
    fs::create_directories(dir);
    std::string filepath = dir + "/wal.log";

    WALConfig config;
    config.policy = FsyncPolicy::ALWAYS;  // 每条写入后立即 fsync
    auto& wal = WAL::GetInstance();
    wal.Init(filepath, config);

    std::string value(128, 'x');
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key", value};
        wal.Append(rec);  // 内部会调用 Sync() -> fsync()
    }
    state.SetItemsProcessed(state.iterations());
    wal.Shutdown();
    fs::remove_all(dir);
}
BENCHMARK(BM_WAL_Single_Always)->Unit(benchmark::kMicrosecond);

// ============================================================
// 测试3：多线程并发写入，NO 策略
//
// 测的是：多个线程同时写入时，mutex 竞争对性能的影响
// 通过 ->Threads(1/2/4/8) 对比不同线程数的吞吐量
//
// 预期结果：
//   线程数增加 → 吞吐量增加，但不是线性（mutex 是瓶颈）
//   8线程时 CPU 时间远小于墙钟时间 → 大量时间在等锁
// ============================================================
static void BM_WAL_Multi_NoSync(benchmark::State& state) {
    std::string dir = TempDir() + "/multi_nosync";
    std::string filepath = dir + "/wal.log";

    // 只有 thread_index==0 的线程负责初始化（只执行一次）
    // 其他线程等待锁，直到 Init 完成
    if (state.thread_index() == 0) {
        fs::create_directories(dir);
        WAL::GetInstance().Init(filepath, {FsyncPolicy::NO});
    }

    std::string value(128, 'x');
    int i = 0;
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key" + std::to_string(i++), value};
        WAL::GetInstance().Append(rec);  // 内部有 mutex_，线程安全
    }

    // 只有 thread_index==0 负责关闭和清理
    if (state.thread_index() == 0) {
        WAL::GetInstance().Shutdown();
        fs::remove_all(dir);
    }
}
BENCHMARK(BM_WAL_Multi_NoSync)
    ->Unit(benchmark::kMicrosecond)
    ->Threads(1)   // 单线程基准
    ->Threads(2)   // 2 线程并发
    ->Threads(4)   // 4 线程并发
    ->Threads(8);  // 8 线程并发

// ============================================================
// 测试4：单线程写入，EVERYSEC 策略
//
// 测的是：后台同步线程在单线程场景下的性能
// sync_interval_ms=200 每 200ms fsync 一次
//
// 预期结果：与 NoSync 接近（fsync 在后台线程执行，不阻塞写入）
// ============================================================
static void BM_WAL_Single_EverySec(benchmark::State& state) {
    std::string dir = TempDir() + "/single_everysec";
    fs::create_directories(dir);
    std::string filepath = dir + "/wal.log";

    WALConfig config;
    config.policy = FsyncPolicy::EVERYSEC;
    config.sync_interval_ms = 200;
    auto& wal = WAL::GetInstance();
    wal.Init(filepath, config);

    std::string value(128, 'x');
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key", value};
        wal.Append(rec);
    }
    state.SetItemsProcessed(state.iterations());
    wal.Shutdown();
    fs::remove_all(dir);
}
BENCHMARK(BM_WAL_Single_EverySec)->Unit(benchmark::kMicrosecond);

// ============================================================
// 测试5：多线程并发写入，EVERYSEC 策略
//
// 测的是：后台同步线程 + 多线程写入的综合性能
// sync_interval_ms=200 每 200ms fsync 一次
//
// 预期结果：与 NoSync 接近（fsync 在后台线程执行，不阻塞写入）
// ============================================================
static void BM_WAL_Multi_EverySec(benchmark::State& state) {
    std::string dir = TempDir() + "/multi_everysec";
    std::string filepath = dir + "/wal.log";

    if (state.thread_index() == 0) {
        fs::create_directories(dir);
        WALConfig config;
        config.policy = FsyncPolicy::EVERYSEC;
        config.sync_interval_ms = 200;  // 后台线程每 200ms fsync 一次
        WAL::GetInstance().Init(filepath, config);
    }

    std::string value(128, 'x');
    int i = 0;
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key" + std::to_string(i++), value};
        WAL::GetInstance().Append(rec);
    }

    if (state.thread_index() == 0) {
        WAL::GetInstance().Shutdown();
        fs::remove_all(dir);
    }
}
BENCHMARK(BM_WAL_Multi_EverySec)
    ->Unit(benchmark::kMicrosecond)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8);

// ============================================================
// 测试5：多线程并发写入，ALWAYS 策略
//
// 测的是：多线程 + 每条都 fsync 的性能
// 预期结果：最慢，因为每条写入都要等 fsync 完成
// ============================================================
static void BM_WAL_Multi_Always(benchmark::State& state) {
    std::string dir = TempDir() + "/multi_always";
    std::string filepath = dir + "/wal.log";

    if (state.thread_index() == 0) {
        fs::create_directories(dir);
        WAL::GetInstance().Init(filepath, {FsyncPolicy::ALWAYS});
    }

    std::string value(128, 'x');
    int i = 0;
    for (auto _ : state) {
        WALRecord rec{OpType::SET, "key" + std::to_string(i++), value};
        WAL::GetInstance().Append(rec);
    }

    if (state.thread_index() == 0) {
        WAL::GetInstance().Shutdown();
        fs::remove_all(dir);
    }
}
BENCHMARK(BM_WAL_Multi_Always)
    ->Unit(benchmark::kMicrosecond)
    ->Threads(1)
    ->Threads(2)
    ->Threads(4)
    ->Threads(8);

// ============================================================
// 测试6：读取性能（ReadAll）
//
// 测的是：从 WAL 文件中读取所有记录的速度
// 先写入 1000 条记录，再测读取
// 预期结果：比写入慢（涉及反序列化 + CRC32 校验）
// ============================================================
static void BM_WAL_ReadAll(benchmark::State& state) {
    std::string dir = TempDir() + "/readall";
    fs::create_directories(dir);
    std::string filepath = dir + "/wal.log";

    WALConfig config;
    config.policy = FsyncPolicy::NO;
    auto& wal = WAL::GetInstance();
    wal.Init(filepath, config);

    // 先写入 1000 条测试数据
    for (int i = 0; i < 1000; ++i) {
        WALRecord rec{OpType::SET, "key" + std::to_string(i), std::string(128, 'v')};
        wal.Append(rec);
    }

    // 测试读取性能：每次迭代读取全部 1000 条
    for (auto _ : state) {
        auto records = wal.ReadAll();
        benchmark::DoNotOptimize(records);  // 防止编译器优化掉读取操作
    }
    state.SetItemsProcessed(state.iterations() * 1000);  // 每次迭代处理 1000 个元素
    wal.Shutdown();
    fs::remove_all(dir);
}
BENCHMARK(BM_WAL_ReadAll)->Unit(benchmark::kMillisecond);

// BENCHMARK_MAIN() 会替代 main()，自动解析命令行参数并运行所有测试
// 常用参数：
//   --benchmark_filter=<regex>    只运行匹配的测试
//   --benchmark_min_time=0.5s    每项测试最少运行 0.5 秒
//   --benchmark_format=console   输出格式（console/json/csv）
BENCHMARK_MAIN();
