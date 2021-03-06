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

#include "scene.h"

#if !defined(__MIC__)
#include "bvh4/bvh4.h"
#include "bvh8/bvh8.h"
#else
#include "xeonphi/bvh4i/bvh4i.h"
#include "xeonphi/bvh4mb/bvh4mb.h"
#include "xeonphi/bvh4hair/bvh4hair.h"
#endif

namespace embree
{
  Scene::Scene (RTCSceneFlags sflags, RTCAlgorithmFlags aflags)
    : flags(sflags), aflags(aflags), numMappedBuffers(0), is_build(false), modified(true), needTriangles(false), needVertices(false),
      numTriangles(0), numTriangles2(0), 
      numBezierCurves(0), numBezierCurves2(0), 
      numSubdivPatches(0), numSubdivPatches2(0), 
      numUserGeometries1(0), 
      numIntersectionFilters4(0), numIntersectionFilters8(0), numIntersectionFilters16(0),
      commitCounter(0), 
      progress_monitor_function(NULL), progress_monitor_ptr(NULL), progress_monitor_counter(0)
  {
#if defined(TASKING_LOCKSTEP) 
    lockstep_scheduler.taskBarrier.init(MAX_MIC_THREADS);
#elif defined(TASKING_TBB_INTERNAL)
    scheduler = NULL;
#else
    group = new tbb::task_group;
#endif

    if (g_scene_flags != -1)
      flags = (RTCSceneFlags) g_scene_flags;

#if defined(__MIC__)
    accels.add( BVH4mb::BVH4mbTriangle1ObjectSplitBinnedSAH(this) );
    accels.add( BVH4i::BVH4iVirtualGeometryBinnedSAH(this, isRobust()));
    accels.add( BVH4Hair::BVH4HairBinnedSAH(this));
    accels.add( BVH4i::BVH4iSubdivMeshBinnedSAH(this, isRobust() ));

    if (g_verbose >= 1)
    {
      std::cout << "scene flags: static " << isStatic() << " compact = " << isCompact() << " high quality = " << isHighQuality() << " robust = " << isRobust() << std::endl;
    }
    
    if (g_tri_accel == "default" || g_tri_accel == "bvh4i")   
    {
      if (g_tri_builder == "default") 
      {
        if (isStatic())
        {
          if (g_verbose >= 1) std::cout << "STATIC BUILDER MODE" << std::endl;
          if ( isCompact() )
            accels.add(BVH4i::BVH4iTriangle1MemoryConservativeBinnedSAH(this,isRobust()));		    
          else if ( isHighQuality() )
            accels.add(BVH4i::BVH4iTriangle1ObjectSplitBinnedSAH(this,isRobust()));
          else
            accels.add(BVH4i::BVH4iTriangle1ObjectSplitBinnedSAH(this,isRobust()));
        }
        else
        {
          if (g_verbose >= 1) std::cout << "DYNAMIC BUILDER MODE" << std::endl;
          accels.add(BVH4i::BVH4iTriangle1ObjectSplitMorton(this,isRobust()));
        }
      }
      else
      {
        if (g_tri_builder == "sah" || g_tri_builder == "bvh4i" || g_tri_builder == "bvh4i.sah") {
          accels.add(BVH4i::BVH4iTriangle1ObjectSplitBinnedSAH(this,isRobust()));
        }
        else if (g_tri_builder == "fast" || g_tri_builder == "morton") {
          accels.add(BVH4i::BVH4iTriangle1ObjectSplitMorton(this,isRobust()));
        }
        else if (g_tri_builder == "fast_enhanced" || g_tri_builder == "morton.enhanced") {
          accels.add(BVH4i::BVH4iTriangle1ObjectSplitEnhancedMorton(this,isRobust()));
        }
        else if (g_tri_builder == "high_quality" || g_tri_builder == "presplits") {
          accels.add(BVH4i::BVH4iTriangle1PreSplitsBinnedSAH(this,isRobust()));
        }
        else if (g_tri_builder == "compact" ||
                 g_tri_builder == "memory_conservative") {
          accels.add(BVH4i::BVH4iTriangle1MemoryConservativeBinnedSAH(this,isRobust()));
        }
        else if (g_tri_builder == "morton64") {
          accels.add(BVH4i::BVH4iTriangle1ObjectSplitMorton64Bit(this,isRobust()));
        }
        
        else THROW_RUNTIME_ERROR("unknown builder "+g_tri_builder+" for BVH4i<Triangle1>");
      }
    }
    else THROW_RUNTIME_ERROR("unknown accel "+g_tri_accel);
    
#else
    createTriangleAccel();
    accels.add(BVH4::BVH4Triangle4vMB(this));
    accels.add(BVH4::BVH4UserGeometry(this));
    createHairAccel();
    accels.add(BVH4::BVH4OBBBezier1iMB(this,false));
    createSubdivAccel();
#endif
  }

#if !defined(__MIC__)

  void Scene::createTriangleAccel()
  {
    if (g_tri_accel == "default") 
    {
      if (isStatic()) {
        int mode =  2*(int)isCompact() + 1*(int)isRobust(); 
        switch (mode) {
        case /*0b00*/ 0: 
#if defined (__TARGET_AVX__)
          if (has_feature(AVX))
	  {
            if (isHighQuality()) accels.add(BVH8::BVH8Triangle4SpatialSplit(this)); 
            else                 accels.add(BVH8::BVH8Triangle4ObjectSplit(this)); 
          }
          else 
#endif
          {
            if (isHighQuality()) accels.add(BVH4::BVH4Triangle4SpatialSplit(this));
            else                 accels.add(BVH4::BVH4Triangle4ObjectSplit(this)); 
          }
          break;

        case /*0b01*/ 1: accels.add(BVH4::BVH4Triangle4vObjectSplit(this)); break;
        case /*0b10*/ 2: accels.add(BVH4::BVH4Triangle4iObjectSplit(this)); break;
        case /*0b11*/ 3: accels.add(BVH4::BVH4Triangle4iObjectSplit(this)); break;
        }
      } 
      else 
      {
        int mode =  2*(int)isCompact() + 1*(int)isRobust();
        switch (mode) {
        case /*0b00*/ 0: accels.add(BVH4::BVH4BVH4Triangle4ObjectSplit(this)); break;
        case /*0b01*/ 1: accels.add(BVH4::BVH4BVH4Triangle4vObjectSplit(this)); break;
        case /*0b10*/ 2: accels.add(BVH4::BVH4BVH4Triangle4iObjectSplit(this)); break;
        case /*0b11*/ 3: accels.add(BVH4::BVH4BVH4Triangle4iObjectSplit(this)); break;
        }
      }
    }
    else if (g_tri_accel == "bvh4.bvh4.triangle1")    accels.add(BVH4::BVH4BVH4Triangle1ObjectSplit(this));
    else if (g_tri_accel == "bvh4.bvh4.triangle4")    accels.add(BVH4::BVH4BVH4Triangle4ObjectSplit(this));
    else if (g_tri_accel == "bvh4.bvh4.triangle1v")   accels.add(BVH4::BVH4BVH4Triangle1vObjectSplit(this));
    else if (g_tri_accel == "bvh4.bvh4.triangle4v")   accels.add(BVH4::BVH4BVH4Triangle4vObjectSplit(this));
    else if (g_tri_accel == "bvh4.triangle1")         accels.add(BVH4::BVH4Triangle1(this));
    else if (g_tri_accel == "bvh4.triangle4")         accels.add(BVH4::BVH4Triangle4(this));
    else if (g_tri_accel == "bvh4.triangle1v")        accels.add(BVH4::BVH4Triangle1v(this));
    else if (g_tri_accel == "bvh4.triangle4v")        accels.add(BVH4::BVH4Triangle4v(this));
    else if (g_tri_accel == "bvh4.triangle4i")        accels.add(BVH4::BVH4Triangle4i(this));
#if defined (__TARGET_AVX__)
    else if (g_tri_accel == "bvh4.triangle8")         accels.add(BVH4::BVH4Triangle8(this));
    else if (g_tri_accel == "bvh8.triangle4")         accels.add(BVH8::BVH8Triangle4(this));
    else if (g_tri_accel == "bvh8.triangle8")         accels.add(BVH8::BVH8Triangle8(this));
#endif
    else THROW_RUNTIME_ERROR("unknown triangle acceleration structure "+g_tri_accel);
  }

  void Scene::createHairAccel()
  {
    if (g_hair_accel == "default") 
    {
      if (isStatic()) {
        int mode =  2*(int)isCompact() + 1*(int)isRobust(); 
        switch (mode) {
        case /*0b00*/ 0: accels.add(BVH4::BVH4OBBBezier1v(this,isHighQuality())); break;
        case /*0b01*/ 1: accels.add(BVH4::BVH4OBBBezier1v(this,isHighQuality())); break;
        case /*0b10*/ 2: accels.add(BVH4::BVH4OBBBezier1i(this,isHighQuality())); break;
        case /*0b11*/ 3: accels.add(BVH4::BVH4OBBBezier1i(this,isHighQuality())); break;
        }
      } 
      else 
      {
        int mode =  2*(int)isCompact() + 1*(int)isRobust();
        switch (mode) {
	case /*0b00*/ 0: accels.add(BVH4::BVH4Bezier1v(this)); break;
        case /*0b01*/ 1: accels.add(BVH4::BVH4Bezier1v(this)); break;
        case /*0b10*/ 2: accels.add(BVH4::BVH4Bezier1i(this)); break;
        case /*0b11*/ 3: accels.add(BVH4::BVH4Bezier1i(this)); break;
        }
      }   
    }
    else if (g_hair_accel == "bvh4.bezier1v"    ) accels.add(BVH4::BVH4Bezier1v(this));
    else if (g_hair_accel == "bvh4.bezier1i"    ) accels.add(BVH4::BVH4Bezier1i(this));
    else if (g_hair_accel == "bvh4obb.bezier1v" ) accels.add(BVH4::BVH4OBBBezier1v(this,false));
    else if (g_hair_accel == "bvh4obb.bezier1i" ) accels.add(BVH4::BVH4OBBBezier1i(this,false));
    else THROW_RUNTIME_ERROR("unknown hair acceleration structure "+g_hair_accel);
  }

  void Scene::createSubdivAccel()
  {
    if (g_subdiv_accel == "default") 
    {
      if (isIncoherent(flags) && isStatic())
        accels.add(BVH4::BVH4SubdivGridEager(this));
      else
        accels.add(BVH4::BVH4SubdivPatch1Cached(this));
    }
    else if (g_subdiv_accel == "bvh4.subdivpatch1"      ) accels.add(BVH4::BVH4SubdivPatch1(this));
    else if (g_subdiv_accel == "bvh4.subdivpatch1cached") accels.add(BVH4::BVH4SubdivPatch1Cached(this));
    else if (g_subdiv_accel == "bvh4.grid.adaptive"     ) accels.add(BVH4::BVH4SubdivGrid(this));
    else if (g_subdiv_accel == "bvh4.grid.eager"        ) accels.add(BVH4::BVH4SubdivGridEager(this));
    else if (g_subdiv_accel == "bvh4.grid.lazy"         ) accels.add(BVH4::BVH4SubdivGridLazy(this));
    else THROW_RUNTIME_ERROR("unknown subdiv accel "+g_subdiv_accel);
  }

#endif

  Scene::~Scene () 
  {
    for (size_t i=0; i<geometries.size(); i++)
      delete geometries[i];

#if TASKING_TBB
    delete group; group = NULL;
#endif
  }

  unsigned Scene::newUserGeometry (size_t items) 
  {
    Geometry* geom = new UserGeometry(this,items);
    return geom->id;
  }
  
  unsigned Scene::newInstance (Scene* scene) {
    Geometry* geom = new Instance(this,scene);
    return geom->id;
  }

  unsigned Scene::newTriangleMesh (RTCGeometryFlags gflags, size_t numTriangles, size_t numVertices, size_t numTimeSteps) 
  {
    if (isStatic() && (gflags != RTC_GEOMETRY_STATIC)) {
      process_error(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
      return -1;
    }

    if (numTimeSteps == 0 || numTimeSteps > 2) {
      process_error(RTC_INVALID_OPERATION,"only 1 or 2 time steps supported");
      return -1;
    }
    
    Geometry* geom = new TriangleMesh(this,gflags,numTriangles,numVertices,numTimeSteps);
    return geom->id;
  }

  unsigned Scene::newSubdivisionMesh (RTCGeometryFlags gflags, size_t numFaces, size_t numEdges, size_t numVertices, size_t numEdgeCreases, size_t numVertexCreases, size_t numHoles, size_t numTimeSteps) 
  {
    if (isStatic() && (gflags != RTC_GEOMETRY_STATIC)) {
      process_error(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
      return -1;
    }

    if (numTimeSteps == 0 || numTimeSteps > 2) {
      process_error(RTC_INVALID_OPERATION,"only 1 or 2 time steps supported");
      return -1;
    }
    
    Geometry* geom = new SubdivMesh(this,gflags,numFaces,numEdges,numVertices,numEdgeCreases,numVertexCreases,numHoles,numTimeSteps);
    return geom->id;
  }


  unsigned Scene::newBezierCurves (RTCGeometryFlags gflags, size_t numCurves, size_t numVertices, size_t numTimeSteps) 
  {
    if (isStatic() && (gflags != RTC_GEOMETRY_STATIC)) {
      process_error(RTC_INVALID_OPERATION,"static scenes can only contain static geometries");
      return -1;
    }

    if (numTimeSteps == 0 || numTimeSteps > 2) {
      process_error(RTC_INVALID_OPERATION,"only 1 or 2 time steps supported");
      return -1;
    }
    
    Geometry* geom = new BezierCurves(this,gflags,numCurves,numVertices,numTimeSteps);
    return geom->id;
  }

  unsigned Scene::add(Geometry* geometry) 
  {
    Lock<AtomicMutex> lock(geometriesMutex);

    if (usedIDs.size()) {
      int id = usedIDs.back(); 
      usedIDs.pop_back();
      geometries[id] = geometry;
      return id;
    } else {
      geometries.push_back(geometry);
      return geometries.size()-1;
    }
  }
  
  void Scene::remove(Geometry* geometry) 
  {
    Lock<AtomicMutex> lock(geometriesMutex);
    usedIDs.push_back(geometry->id);
    geometries[geometry->id] = NULL;
    delete geometry;
  }

  void Scene::updateInterface()
  {
    /* update bounds */
    is_build = true;
    bounds = accels.bounds;
    intersectors = accels.intersectors;

    /* enable only algorithms choosen by application */
    if ((aflags & RTC_INTERSECT1) == 0) {
      intersectors.intersector1.intersect = NULL;
      intersectors.intersector1.occluded = NULL;
    }
    if ((aflags & RTC_INTERSECT4) == 0) {
      intersectors.intersector4.intersect = NULL;
      intersectors.intersector4.occluded = NULL;
    }
    if ((aflags & RTC_INTERSECT8) == 0) {
      intersectors.intersector8.intersect = NULL;
      intersectors.intersector8.occluded = NULL;
    }
    if ((aflags & RTC_INTERSECT16) == 0) {
      intersectors.intersector16.intersect = NULL;
      intersectors.intersector16.occluded = NULL;
    }

    /* update commit counter */
    commitCounter++;
  }

  void Scene::build_task ()
  {
    progress_monitor_counter = 0;

    /* select fast code path if no intersection filter is present */
    accels.select(numIntersectionFilters4,numIntersectionFilters8,numIntersectionFilters16);
  
    /* build all hierarchies of this scene */
    accels.build(0,0);
    
    /* make static geometry immutable */
    if (isStatic()) 
    {
      accels.immutable();
      for (size_t i=0; i<geometries.size(); i++)
        if (geometries[i]) geometries[i]->immutable();
    }

    /* delete geometry that is scheduled for delete */
    for (size_t i=0; i<geometries.size(); i++) // FIXME: this late deletion is inefficient in case of many geometries
    {
      Geometry* geom = geometries[i];
      if (!geom) continue;
      if (geom->state == Geometry::ENABLING) geom->state = Geometry::ENABLED;
      if (geom->state == Geometry::MODIFIED) geom->state = Geometry::ENABLED;
      if (geom->state == Geometry::DISABLING) geom->state = Geometry::DISABLED;
      if (geom->state == Geometry::ERASING) remove(geom);
    }

    updateInterface();

    if (g_verbose >= 2) {
      std::cout << "created scene intersector" << std::endl;
      accels.print(2);
      std::cout << "selected scene intersector" << std::endl;
      intersectors.print(2);
    }
  }

#if defined(TASKING_LOCKSTEP)

  void Scene::task_build_parallel(size_t threadIndex, size_t threadCount, size_t taskIndex, size_t taskCount, TaskScheduler::Event* event) 
  {
    LockStepTaskScheduler::Init init(threadIndex,threadCount,&lockstep_scheduler);
    if (threadIndex == 0) accels.build(threadIndex,threadCount);
  }

  void Scene::build (size_t threadIndex, size_t threadCount) 
  {
    LockStepTaskScheduler::Init init(threadIndex,threadCount,&lockstep_scheduler);
    if (threadIndex != 0) return;

    /* allow only one build at a time */
    Lock<MutexSys> lock(buildMutex);

    progress_monitor_counter = 0;

    //if (isStatic() && isBuild()) {
    //  process_error(RTC_INVALID_OPERATION,"static geometries cannot get committed twice");
    //  return;
    //}

    if (!ready()) {
      process_error(RTC_INVALID_OPERATION,"not all buffers are unmapped");
      return;
    }

    /* select fast code path if no intersection filter is present */
    accels.select(numIntersectionFilters4,numIntersectionFilters8,numIntersectionFilters16);

    /* if user provided threads use them */
    if (threadCount)
      accels.build(threadIndex,threadCount);

    /* otherwise use our own threads */
    else
    {
      TaskScheduler::EventSync event;
      new (&task) TaskScheduler::Task(&event,_task_build_parallel,this,TaskScheduler::getNumThreads(),NULL,NULL,"scene_build");
      TaskScheduler::addTask(-1,TaskScheduler::GLOBAL_FRONT,&task);
      event.sync();
    }

    /* make static geometry immutable */
    if (isStatic()) 
    {
      accels.immutable();
      for (size_t i=0; i<geometries.size(); i++)
        if (geometries[i]) geometries[i]->immutable();
    }

    /* delete geometry that is scheduled for delete */
    for (size_t i=0; i<geometries.size(); i++) // FIXME: this late deletion is inefficient in case of many geometries
    {
      Geometry* geom = geometries[i];
      if (!geom) continue;
      if (geom->state == Geometry::ENABLING) geom->state = Geometry::ENABLED;
      if (geom->state == Geometry::MODIFIED) geom->state = Geometry::ENABLED;
      if (geom->state == Geometry::DISABLING) geom->state = Geometry::DISABLED;
      if (geom->state == Geometry::ERASING) remove(geom);
    }

    updateInterface();

    if (g_verbose >= 2) {
      std::cout << "created scene intersector" << std::endl;
      accels.print(2);
      std::cout << "selected scene intersector" << std::endl;
      intersectors.print(2);
    }
  }

#endif

#if defined(TASKING_TBB_INTERNAL)

  void Scene::build (size_t threadIndex, size_t threadCount) 
  {
    if (threadCount != 0) 
    {
      {
        Lock<MutexSys> lock(buildMutex);
        if (scheduler == NULL) scheduler = new TaskSchedulerNew(-1);
      }
      if (threadIndex > 0) {
        scheduler->join();
        return;
      } else
        scheduler->wait_for_threads(threadCount);
    }

    /* allow only one build at a time */
    Lock<MutexSys> lock(buildMutex);

    progress_monitor_counter = 0;

    //if (isStatic() && isBuild()) {
    //  process_error(RTC_INVALID_OPERATION,"static geometries cannot get committed twice");
    //  return;
    //}

    if (!ready()) {
      process_error(RTC_INVALID_OPERATION,"not all buffers are unmapped");
      return;
    }

    if (threadCount) {
      scheduler->spawn_root  ([&]() { build_task(); });
      delete scheduler; scheduler = NULL;
    }
    else {
      TaskSchedulerNew::spawn([&]() { build_task(); });
    }
  }

#endif

#if defined(TASKING_TBB)

  void Scene::build (size_t threadIndex, size_t threadCount) 
  {
    /* let threads wait for build to finish in rtcCommitThread mode */
    if (threadCount != 0) {
      if (threadIndex > 0) {
        group_barrier.wait(threadCount); // FIXME: use barrier that waits in condition
        group->wait();
        return;
      }
    }

    /* try to obtain build lock */
    TryLock<MutexSys> lock(buildMutex);

    /* join hierarchy build */
    if (!lock.isLocked()) {
      group->wait();
      while (!buildMutex.try_lock()) {
        __pause_cpu();
        yield();
        group->wait();
      }
      buildMutex.unlock();
      return;
    }

    if (!isModified()) {
      return;
    }

    if (!ready()) {
      process_error(RTC_INVALID_OPERATION,"not all buffers are unmapped");
      return;
    }

    try {
      group->run([&]{ 
          tbb::task::self().group()->set_priority(tbb::priority_high);
          build_task();
        }); 
      if (threadCount) group_barrier.wait(threadCount);
      group->wait();
      setModified(false);
    } 
    catch (...) {
      accels.clear();
      updateInterface();
      // FIXME: clear cancelling state of task_group_context
      throw;
    }
  }
#endif

  void Scene::write(std::ofstream& file)
  {
    int magick = 0x35238765LL;
    file.write((char*)&magick,sizeof(magick));
    int numGroups = size();
    file.write((char*)&numGroups,sizeof(numGroups));
    for (size_t i=0; i<numGroups; i++) {
      if (geometries[i]) geometries[i]->write(file);
      else { int type = -1; file.write((char*)&type,sizeof(type)); }
    }
  }

  void Scene::setProgressMonitorFunction(RTC_PROGRESS_MONITOR_FUNCTION func, void* ptr) 
  {
    static MutexSys mutex;
    mutex.lock();
    progress_monitor_function = func;
    progress_monitor_ptr      = ptr;
    mutex.unlock();
  }

  void Scene::progressMonitor(double dn)
  {
    if (progress_monitor_function) {
      size_t n = atomic_t(dn) + atomic_add(&progress_monitor_counter, atomic_t(dn));
      if (!progress_monitor_function(progress_monitor_ptr, n / (double(numPrimitives())))) {
#if defined(TASKING_TBB)
        THROW_MY_RUNTIME_ERROR(RTC_CANCELLED,"progress monitor forced termination");
#endif
      }
    }
  }
}
