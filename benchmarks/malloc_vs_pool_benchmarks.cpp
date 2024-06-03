#include <fmt/format.h>

#include <cstdlib>
#include <string>

#include "umpire/ResourceManager.hpp"
#include "umpire/strategy/QuickPool.hpp"

// TODO Can we make these Enums?
constexpr std::size_t KibiByte = std::size_t{1} << 10;
constexpr std::size_t MibiByte = std::size_t{1} << 20;
constexpr std::size_t GibiByte = std::size_t{1} << 30;
constexpr std::size_t TebiByte = std::size_t{1} << 40;

void test_malloc_peformance(std::size_t pool_size, std::size_t allocation_size)
{
  std::size_t num_allocations = pool_size / allocation_size;
  std::vector<void *> allocations(num_allocations);

  for (std::size_t i{0}; i < num_allocations; i++) {
    allocations[i] = std::malloc(allocation_size);
    std::memset(allocations[i], 0, allocation_size);
  }

  for (std::size_t i{0}; i < num_allocations; i++) {
    std::free(allocations[i]);
  }
}

template <class T> // TODO Add introspection
void test_pool_performance()
{
}

int main(int argc, char *argv[])
{
  test_malloc_peformance(MibiByte, KibiByte);
  return 0;
}
