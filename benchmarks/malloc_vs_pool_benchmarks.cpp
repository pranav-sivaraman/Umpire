#include <benchmark/benchmark.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include <cstring>
#include <umpire/Allocator.hpp>
#include <umpire/ResourceManager.hpp>
#include <umpire/strategy/QuickPool.hpp>

constexpr std::size_t KibiByte = std::size_t{1} << 10;
constexpr std::size_t MibiByte = std::size_t{1} << 20;
constexpr std::size_t GibiByte = std::size_t{1} << 30;
constexpr std::size_t TebiByte = std::size_t{1} << 40;

void malloc_allocate(std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (void *ptr : allocations) {
    ptr = std::malloc(allocation_size);
    benchmark::DoNotOptimize(ptr);
    if (memset)
      std::memset(ptr, 0, allocation_size);
    benchmark::ClobberMemory();
  }
}

void pool_allocate(umpire::ResourceManager &rm, umpire::Allocator &pool_alloc, std::vector<void *> &allocations,
                   std::size_t allocation_size, bool memset)
{
  for (void *ptr : allocations) {
    ptr = pool_alloc.allocate(allocation_size);
    benchmark::DoNotOptimize(ptr);
    if (memset)
      rm.memset(ptr, 0, allocation_size);
    benchmark::ClobberMemory();
  }
}

void malloc_driver(benchmark::State &state)
{
  std::size_t pool_size = state.range(0);
  std::size_t allocation_size = state.range(1);
  bool memset = state.range(2);

  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  for (auto _ : state) {
    malloc_allocate(allocations, allocation_size, memset);
  }

  for (void *ptr : allocations) {
    std::free(ptr);
  }
}

template <class T, bool introspection>
void pool_driver(benchmark::State &state)
{
  auto &rm = umpire::ResourceManager::getInstance();
  std::size_t pool_size = state.range(0);
  std::size_t allocation_size = state.range(1);
  bool memset = state.range(2);

  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  auto alloc = rm.getAllocator("HOST");
  auto pool_alloc = rm.makeAllocator<T, introspection>("", alloc, pool_size);

  for (auto _ : state) {
    pool_allocate(rm, pool_alloc, allocations, allocation_size, memset);
  }

  for (void *ptr : allocations) {
    pool_alloc.deallocate(ptr);
  }
}

BENCHMARK(malloc_driver)
    ->ArgsProduct({
        {GibiByte},    // Pool Sizes
        {KibiByte},    // Allocation Sizes
        {true, false}, // Memset
    });

BENCHMARK(pool_driver<umpire::strategy::QuickPool, true>)
    ->ArgsProduct({
        {GibiByte},    // Pool Sizes
        {KibiByte},    // Allocation Sizes
        {true, false}, // Memset
    });

BENCHMARK_MAIN();
