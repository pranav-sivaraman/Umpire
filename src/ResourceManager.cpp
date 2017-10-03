#include "umpire/config.hpp"

#include "umpire/ResourceManager.hpp"
#include "umpire/AllocatorRegistry.hpp"

#include "umpire/space/HostSpaceFactory.hpp"
#if defined(ENABLE_CUDA)
#include "umpire/space/DeviceSpaceFactory.hpp"
#include "umpire/space/UnifiedMemorySpaceFactory.hpp"
#endif

#include "umpire/util/Macros.hpp"

namespace umpire {

ResourceManager* ResourceManager::s_resource_manager_instance = nullptr;

ResourceManager&
ResourceManager::getInstance()
{
  if (!s_resource_manager_instance) {
    s_resource_manager_instance = new ResourceManager();
  }

  return *s_resource_manager_instance;
}

ResourceManager::ResourceManager() :
  m_allocator_names(),
  m_allocators(),
  m_allocation_to_allocator()
{
  AllocatorRegistry& registry =
    AllocatorRegistry::getInstance();

  registry.registerAllocator(
      std::make_shared<space::HostSpaceFactory>());

#if defined(ENABLE_CUDA)
  registry.registerAllocator(
      std::make_shared<space::DeviceSpaceFactory>());

  registry.registerAllocator(
      std::make_shared<space::UnifiedMemorySpaceFactory>());
#endif
}

Allocator
ResourceManager::getAllocator(const std::string& name)
{
  AllocatorRegistry& registry =
    AllocatorRegistry::getInstance();

  auto allocator = m_allocators.find(name);
  if (allocator == m_allocators.end()) {
    m_allocators[name] = registry.makeAllocator(name);
  }

  return Allocator(m_allocators[name]);
}

// void 
// ResourceManager::setDefaultAllocator(std::shared_ptr<Allocator>& allocator)
// {
//   m_default_allocator = allocator;
// }
// 
// std::shared_ptr<Allocator> ResourceManager::getDefaultAllocator()
// {
//   return m_default_allocator;
// }

void ResourceManager::registerAllocation(void* ptr, std::shared_ptr<AllocatorInterface> space)
{
  m_allocation_to_allocator[ptr] = space;
}

void ResourceManager::deregisterAllocation(void* ptr)
{
  m_allocation_to_allocator.erase(ptr);
}

void ResourceManager::deallocate(void* ptr)
{
  auto allocator = m_allocation_to_allocator.find(ptr);

  if (allocator != m_allocation_to_allocator.end()) {
    allocator->second->deallocate(ptr);
  } else {
    UMPIRE_ERROR("Cannot find allocator for " << ptr);
  }
}

} // end of namespace umpire
