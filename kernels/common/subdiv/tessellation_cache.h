// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "common/default.h"
#include "subdivpatch1base.h"

#define CACHE_DBG(x) 

/* force a complete cache invalidation when running out of allocation space */
#define FORCE_SIMPLE_FLUSH 0

#if defined(DEBUG)
#define CACHE_STATS(x) 
#else
#define CACHE_STATS(x) 
#endif


namespace embree
{
  void resizeTessellationCache(const size_t new_size);
  void clearTessellationCache();

  /* alloc cache memory */
  float *alloc_tessellation_cache_mem(const size_t blocks);

  /* free cache memory */
  void free_tessellation_cache_mem(void *mem, const size_t blocks = 0);


#if defined(__MIC__)
 typedef unsigned int InputTagType;
#else
 typedef size_t InputTagType;
#endif

 static __forceinline unsigned int toTag(InputTagType prim)
 {
#if defined(__MIC__)
   return prim;
#else
   return prim / 320;
#endif
 }
 ////////////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////////////


 struct LocalTessellationCacheThreadInfo
 {
   unsigned int id;
   LocalTessellationCacheThreadInfo(const unsigned int id) : id(id) {}  
 };

 class __aligned(64) SharedLazyTessellationCache 
 {
 private:
   static const size_t DEFAULT_TESSELLATION_CACHE_SIZE = 250*1024*1024; // 250 MB

   struct __aligned(64) ThreadWorkState {
     AtomicCounter counter;
     ThreadWorkState() { counter = 0; }
   __forceinline void reset() { counter = 0; }
   };

   float *data;
   size_t size;
   size_t maxBlocks;
   size_t numMaxRenderThreads;
   ThreadWorkState *threadWorkState;
      
   __aligned(64) AtomicCounter index;
   __aligned(64) AtomicCounter next_block;
   __aligned(64) AtomicMutex   reset_state;
   __aligned(64) AtomicCounter switch_block_threshold;
   __aligned(64) AtomicCounter numRenderThreads;
   __aligned(64) AtomicMutex   mtx_threads;



 public:

   static const size_t NUM_CACHE_REGIONS = 4;

      
   SharedLazyTessellationCache();

   size_t getNextRenderThreadID();

   __forceinline size_t getCurrentIndex() { return index; }
   __forceinline void   addCurrentIndex(const size_t i=1) { index.add(i); }

   __forceinline unsigned int lockThread  (const unsigned int threadID) { return threadWorkState[threadID].counter.add(1);  }
   __forceinline unsigned int unlockThread(const unsigned int threadID) { return threadWorkState[threadID].counter.add(-1); }

   __forceinline bool validCacheIndex(const size_t i)
   {
#if FORCE_SIMPLE_FLUSH == 1
     return i == index;
#else
     return i+(NUM_CACHE_REGIONS-1) >= index;
#endif
   }

   void waitForUsersLessEqual(const unsigned int threadID,
			      const unsigned int users);
    
   __forceinline size_t alloc(const size_t blocks)
   {
     size_t index = next_block.add(blocks);
     if (unlikely(index + blocks >= switch_block_threshold)) return (size_t)-1;
     return index;
   }

   __forceinline void *getBlockPtr(const size_t block_index)
   {
     assert(block_index < maxBlocks);
     return (void*)&data[block_index*16];
   }

   __forceinline void*  getDataPtr()      { return data; }
   __forceinline size_t getNumUsedBytes() { return next_block * 64; }
   __forceinline size_t getMaxBlocks()    { return maxBlocks; }
   __forceinline size_t getSize()         { return size; }

   void resetCache();
   void realloc(const size_t newSize);

   static SharedLazyTessellationCache sharedLazyTessellationCache;
    
 };

 ////////////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////////////


 class SharedTessellationCacheStats
 {
 public:
    /* stats */
   static AtomicCounter cache_accesses;
   static AtomicCounter cache_hits;
   static AtomicCounter cache_misses;
   static AtomicCounter cache_flushes;                

    /* print stats for debugging */                 
    static void printStats();
    static void clearStats();

 };

  // =========================================================================================================
  // =========================================================================================================
  // =========================================================================================================

};
