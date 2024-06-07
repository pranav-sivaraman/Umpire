#include <benchmark/benchmark.h>
#include <fmt/format.h>
#include <mpi.h>

#include <cstdlib>
#include <string>
#include <umpire/Allocator.hpp>
#include <umpire/ResourceManager.hpp>
#include <umpire/strategy/QuickPool.hpp>
#include <vector>

constexpr std::size_t KibiByte = std::size_t{1} << 10;
constexpr std::size_t MibiByte = std::size_t{1} << 20;
constexpr std::size_t GibiByte = std::size_t{1} << 30;
constexpr std::size_t TebiByte = std::size_t{1} << 40;

static int counter = 0;

inline void malloc_allocate(std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (auto &ptr : allocations) {
    ptr = std::malloc(allocation_size);
    if (memset) {
      std::memset(ptr, 1, allocation_size);
    }
  }
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}

inline void pool_allocate(umpire::ResourceManager &rm, umpire::Allocator &pool_allocator,
                          std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (auto &ptr : allocations) {
    ptr = pool_allocator.allocate(allocation_size);
    if (memset) {
      rm.memset(ptr, 1, allocation_size);
    }
  }
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void malloc_driver(benchmark::State &state)
{
  std::size_t pool_size = state.range(0);
  std::size_t allocation_size = state.range(1);
  bool memset = state.range(2);

  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  for (auto _ : state) {
#if defined(UMPIRE_ENABLE_MPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    auto start{std::chrono::high_resolution_clock::now()};
    malloc_allocate(allocations, allocation_size, memset);
    auto end{std::chrono::high_resolution_clock::now()};

    const auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    auto elapsed_seconds = duration.count();
    state.SetIterationTime(elapsed_seconds);
  }

  for (auto &ptr : allocations) {
    std::free(ptr);
  }
}

template <class T, bool introspection>
void umpire_driver(benchmark::State &state)
{
  std::size_t pool_size = state.range(0);
  std::size_t allocation_size = state.range(1);
  bool memset = state.range(2);

  int rank = 0;
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  std::string pool_name = fmt::format("{}-{}", counter++, rank);
  auto &rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  auto pool_allocator = rm.makeAllocator<T, introspection>(pool_name, allocator, pool_size);

  for (auto _ : state) {
#if defined(UMPIRE_ENABLE_MPI)
    MPI_Barrier(MPI_COMM_WORLD);
#endif
    auto start{std::chrono::high_resolution_clock::now()};
    pool_allocate(rm, pool_allocator, allocations, allocation_size, memset);
    auto end{std::chrono::high_resolution_clock::now()};

    const auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    auto elapsed_seconds = duration.count();
    state.SetIterationTime(elapsed_seconds);
  }

  for (auto &ptr : allocations) {
    rm.deallocate(ptr);
  }

  pool_allocator.release();
}

std::vector<int64_t> allocation_sizes = {4096, 8192};

BENCHMARK(malloc_driver)
    ->ArgsProduct({
        {GibiByte},       // Pool Sizes
        allocation_sizes, // Allocation Sizes
        {true, false},    // Memset
    })
    ->UseManualTime();

BENCHMARK(umpire_driver<umpire::strategy::QuickPool, true>)
    ->ArgsProduct({
        {GibiByte},       // Pool Sizes
        allocation_sizes, // Allocation Sizes
        {true, false},    // Memset
    })
    ->UseManualTime();

// BENCHMARK(umpire_driver<umpire::strategy::QuickPool, false>)
//     ->ArgsProduct({
//         {GibiByte},    // Pool Sizes
//         {KibiByte},    // Allocation Sizes
//         {true, false}, // Memset
//     })
//     ->UseManualTime();

class NullReporter : public ::benchmark::BenchmarkReporter {
 public:
  NullReporter()
  {
  }
  virtual bool ReportContext(const Context &)
  {
    return true;
  }
  virtual void ReportRuns(const std::vector<Run> &)
  {
  }
  virtual void Finalize()
  {
  }
};

// The main is rewritten to allow for MPI initializing and for selecting a
// reporter according to the process rank
int main(int argc, char **argv)
{
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Init(&argc, &argv);
#endif

  int rank = 0;
#if defined(UMPIRE_ENABLE_MPI)
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  ::benchmark::Initialize(&argc, argv);

  if (rank == 0)
    // root process will use a reporter from the usual set provided by
    // ::benchmark
    ::benchmark::RunSpecifiedBenchmarks();
  else {
    // reporting from other processes is disabled by passing a custom reporter
    NullReporter null;
    ::benchmark::RunSpecifiedBenchmarks(&null);
  }

#if defined(UMPIRE_ENABLE_MPI)
  MPI_Finalize();
#endif
  return 0;
}
