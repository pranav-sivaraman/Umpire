#ifndef UMPIRE_MemoryAllocator_HPP
#define UMPIRE_MemoryAllocator_HPP

#include <cstddef>

namespace umpire {
namespace alloc {

class MemoryAllocator {
  public:
  virtual void* malloc(size_t bytes) = 0;
  virtual void* calloc(size_t bytes) = 0;
  virtual void* realloc(void* ptr, size_t new_size) = 0;
  virtual void free(void* ptr) = 0;
};

} // end of namespace alloc
} // end of namespace umpire

#endif // UMPIRE_MemoryAllocator_HPP
