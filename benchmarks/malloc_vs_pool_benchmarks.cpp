#include <benchmark/benchmark.h>
#include <fmt/format.h>

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

inline void malloc_allocate(std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (auto &ptr : allocations) {
    ptr = std::malloc(allocation_size);
    if (memset) {
      std::memset(ptr, 0, allocation_size);
    }
  }
}

inline void pool_allocate(umpire::ResourceManager &rm, umpire::Allocator &pool_allocator,
                          std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (auto &ptr : allocations) {
    ptr = pool_allocator.allocate(allocation_size);
    if (memset) {
      rm.memset(ptr, 0, allocation_size);
    }
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

  for (auto &ptr : allocations) {
    std::free(ptr);
  }
}

template <class T, bool introspection>
void umpire_driver(benchmark::State &state)
{
  static int counter = 0;
  std::size_t pool_size = state.range(0);
  std::size_t allocation_size = state.range(1);
  bool memset = state.range(2);

  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  auto &rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  auto pool_allocator = rm.makeAllocator<T, introspection>(std::to_string(counter++), allocator, pool_size);

  for (auto _ : state) {
    pool_allocate(rm, pool_allocator, allocations, allocation_size, memset);
  }

  pool_allocator.release();
}

BENCHMARK(malloc_driver)
    ->ArgsProduct({
        {GibiByte},    // Pool Sizes
        {KibiByte},    // Allocation Sizes
        {true, false}, // Memset
    });

BENCHMARK(umpire_driver<umpire::strategy::QuickPool, true>)
    ->ArgsProduct({
        {GibiByte},    // Pool Sizes
        {KibiByte},    // Allocation Sizes
        {true, false}, // Memset
    });

BENCHMARK_MAIN();
