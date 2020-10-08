//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-20, Lawrence Livermore National Security, LLC and Umpire
// project contributors. See the COPYRIGHT file for details.
//
// SPDX-License-Identifier: (MIT)
//////////////////////////////////////////////////////////////////////////////
#include "gtest/gtest.h"
#include "umpire/Allocator.hpp"
#include "umpire/ResourceManager.hpp"
#include "umpire/Umpire.hpp"
#include "umpire/config.hpp"
#include "umpire/util/MemoryResourceTraits.hpp"

using cPlatform = camp::resources::Platform;
using myResource = umpire::MemoryResourceTraits::resource_type;

struct host_platform {};

template <typename Platform>
struct allocate_and_use{};

template<>
struct allocate_and_use<host_platform>
{
  void test(umpire::Allocator* alloc, size_t size)
  {
    size_t* data = static_cast<size_t*>(alloc->allocate(size * sizeof(size_t)));
    data[0] = size * size;
    alloc->deallocate(data);
  }
};

#if defined(UMPIRE_ENABLE_CUDA) || defined(UMPIRE_ENABLE_HIP)
__global__ void tester(size_t* d_data, size_t size)
{
  int idx = blockDim.x * blockIdx.x + threadIdx.x;
   
  if (idx == 0) {
    d_data[0] = size * size;
  }
}
#endif

#if defined(UMPIRE_ENABLE_CUDA)
struct cuda_platform {};

template<>
struct allocate_and_use<cuda_platform>
{
  void test(umpire::Allocator* alloc, size_t size)
  {
    size_t* data = static_cast<size_t*>(alloc->allocate(size * sizeof(size_t)));
    tester<<<1, 16>>>(data, size);
    cudaDeviceSynchronize();
    alloc->deallocate(data);
  }
};
#endif

#if defined(UMPIRE_ENABLE_HIP)
struct hip_platform{};

template<>
struct allocate_and_use<hip_platform>
{
  void test(umpire::Allocator* alloc, size_t size)
  {
    size_t* data = static_cast<size_t*>(alloc->allocate(size * sizeof(size_t)));
    hipLaunchKernelGGL(tester, dim3(1), dim3(16), 0,0, data, size);
    hipDeviceSynchronize();
    alloc->deallocate(data);
  }
};
#endif

#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
struct omp_target_platform{};

template<>
struct allocate_and_use<omp_target_platform>
{
  void test(umpire::Allocator* alloc, size_t size)
  {
    int dev = alloc->getAllocationStrategy()->getTraits().id;
    size_t* data = static_cast<size_t*>(alloc->allocate(size * sizeof(size_t)));
    size_t* d_data{static_cast<size_t*>(data)};

#pragma omp target is_device_ptr(d_data) device(dev)
#pragma omp teams distribute parallel for schedule(static, 1)
    for (auto i = 0; i < size; ++i) {
      d_data[i] = static_cast<size_t>(i);
    }

    alloc->deallocate(data);
  }
};
#endif

class AllocatorAccessibilityTest : public ::testing::TestWithParam<std::string> {
 public:
  virtual void SetUp()
  {
    auto& rm = umpire::ResourceManager::getInstance();
    m_allocator = new umpire::Allocator(rm.getAllocator(GetParam()));
  }

  virtual void TearDown()
  {
    m_allocator->release();
    delete m_allocator;
  }
  
  umpire::Allocator* m_allocator;
  size_t m_size = 42;
};

TEST_P(AllocatorAccessibilityTest, AccessibilityFromPlatform)
{
  if(umpire::is_accessible(cPlatform::host, *m_allocator)) {
    allocate_and_use<host_platform> h;
    ASSERT_NO_THROW(h.test(m_allocator, m_size));
  }

#if defined(UMPIRE_ENABLE_CUDA)
  if (umpire::is_accessible(cPlatform::cuda, *m_allocator)) {
    allocate_and_use<cuda_platform> c;
    ASSERT_NO_THROW(c.test(m_allocator, m_size));
  }
#endif
  
#if defined(UMPIRE_ENABLE_HIP)
  if (umpire::is_accessible(cPlatform::hip, *m_allocator)) {
    allocate_and_use<hip_platform> hd;
    ASSERT_NO_THROW(hd.test(m_allocator, m_size));
  }
#endif
 
#if defined(UMPIRE_ENABLE_OPENMP_TARGET)
  if (umpire::is_accessible(cPlatform::omp_target, *m_allocator)) {
    allocate_and_use<omp_target_platform> o;
    ASSERT_NO_THROW(o.test(m_allocator, m_size));
  }
#endif

/////////////////////////////
//Sycl test not yet available
/////////////////////////////

  if(umpire::is_accessible(cPlatform::undefined, *m_allocator)) {
    FAIL() << "An Undefined platform is not accessible." << std::endl;
  }
}

std::vector<std::string> get_allocators()
{
  auto& rm = umpire::ResourceManager::getInstance();
  std::vector<std::string> all_allocators = rm.getResourceNames();
  std::vector<std::string> avail_allocators;
  
  std::cout << "Available allocators: ";
  for(auto a : all_allocators) {
    if(a.find("::") == std::string::npos) { 
      avail_allocators.push_back(a);
      std::cout << a << " ";
    }
  }
  std::cout << std::endl;

  return avail_allocators;
}

INSTANTIATE_TEST_SUITE_P(Allocators, AllocatorAccessibilityTest, ::testing::ValuesIn(get_allocators()));

//END gtest
