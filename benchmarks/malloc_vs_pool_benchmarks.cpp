#include <fmt/chrono.h>
#include <fmt/format.h>
#include <mpi.h>

#include <chrono>
#include <cstdint>
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

void malloc_allocate(std::vector<void *> &allocations, std::size_t allocation_size, bool memset)
{
  for (auto &ptr : allocations) {
    ptr = std::malloc(allocation_size);
    if (memset) {
      std::memset(ptr, 1, allocation_size);
    }
  }
}

void pool_allocate(umpire::Allocator &pool_allocator, std::vector<void *> &allocations, std::size_t allocation_size,
                   bool memset)
{
  for (auto &ptr : allocations) {
    ptr = pool_allocator.allocate(allocation_size);
    if (memset) {
      std::memset(ptr, 1, allocation_size);
    }
  }
}

int main(int argc, char *argv[])
{
  MPI_Init(&argc, &argv);
  int rank = 0;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  int64_t N = 10000;
  bool memset = false;

  if (argc > 1) {
    N = std::stoll(argv[1]);
  }

  if (argc > 2) {
    memset = std::stoi(argv[2]);
  }

  std::size_t pool_size = GibiByte;
  std::size_t allocation_size = KibiByte / 2;
  std::size_t num_allocations = pool_size / allocation_size;

  std::vector<void *> allocations(num_allocations);

  if (rank == 0) {
    fmt::println("N: {}, Memset: {}", N, memset);
    fmt::println("Pool Size: {}, Allocation Size: {}", pool_size, allocation_size);
  }

  auto &rm = umpire::ResourceManager::getInstance();
  auto allocator = rm.getAllocator("HOST");
  auto pool_allocator = rm.makeAllocator<umpire::strategy::QuickPool, true>("QuickPool", allocator, pool_size);

  // Malloc
  std::chrono::duration<double> malloc_average{};
  for (int64_t i = 0; i < N; i++) {
    MPI_Barrier(MPI_COMM_WORLD);
    auto start = std::chrono::high_resolution_clock::now();

    malloc_allocate(allocations, allocation_size, memset);

    MPI_Barrier(MPI_COMM_WORLD);
    auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> elapsed = end - start;
    malloc_average += elapsed;

    // Cleanup
    for (auto &ptr : allocations) {
      std::free(ptr);
    }
  }

  malloc_average /= N;
  if (rank == 0)
    fmt::println("Malloc Elapsed Time: {}", malloc_average);

  // Umpire QuickPool
  std::chrono::duration<double> umpire_average{};
  for (int64_t i = 0; i < N; i++) {
    MPI_Barrier(MPI_COMM_WORLD);
    auto start = std::chrono::high_resolution_clock::now();

    pool_allocate(pool_allocator, allocations, allocation_size, memset);

    MPI_Barrier(MPI_COMM_WORLD);
    auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> elapsed = end - start;
    umpire_average += elapsed;

    // Cleanup
    for (auto &ptr : allocations) {
      pool_allocator.deallocate(ptr);
    }
  }

  umpire_average /= N;
  if (rank == 0)
    fmt::println("Umpire Elapsed Time: {}", umpire_average);

  pool_allocator.release();

  fmt::println("{}", umpire_average > malloc_average);

  MPI_Finalize();

  return 0;
}
