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

#include "common/ray.h"
#include "geometry/filter.h"

/*! Modified Pluecker ray/triangle intersector. The test first shifts the ray
 *  origin into the origin of the coordinate system and then uses
 *  Pluecker coordinates for the intersection. Due to the shift, the
 *  Pluecker coordinate calculation simplifies. The edge equations
 *  are watertight along the edge for neighboring triangles. */

namespace embree
{
  namespace isa
  {
    /*! Intersect a ray with the 4 triangles and updates the hit. */
    template<typename tsimdb, typename tsimdf, typename tsimdi>
    __forceinline void intersect(Ray& ray, 
                                 const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2,  
                                 const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs, Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          typedef Vec3<tsimdf> tsimd3f;
          const tsimd3f O = tsimd3f(ray.org);
          const tsimd3f D = tsimd3f(ray.dir);
          const tsimd3f v0 = tri_v0-O;
          const tsimd3f v1 = tri_v1-O;
          const tsimd3f v2 = tri_v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear)) & (absDen*tsimdf(ray.tfar) >= T);
          if (unlikely(none(valid))) return;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri_mask & ray.mask) != 0;
          if (unlikely(none(valid))) return;
#endif
          
          /* calculate hit information */
          const tsimdf u = U / absDen;
          const tsimdf v = V / absDen;
          const tsimdf t = T / absDen;
          size_t i = select_min(valid,t);
          int geomID = tri_geomIDs[i];
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasIntersectionFilter1())) 
            {
#endif
              /* update hit information */
              ray.u = u[i];
              ray.v = v[i];
              ray.tfar = t[i];
              ray.Ng.x = Ng.x[i];
              ray.Ng.y = Ng.y[i];
              ray.Ng.z = Ng.z[i];
              ray.geomID = geomID;
              ray.primID = tri_primIDs[i];
              
#if defined(RTCORE_INTERSECTION_FILTER)
              return;
            }
            
            Vec3fa N = Vec3fa(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runIntersectionFilter1(geometry,ray,u[i],v[i],t[i],N,geomID,tri_primIDs[i])) return;
            valid[i] = 0;
            if (none(valid)) return;
            i = select_min(valid,t);
            geomID = tri_geomIDs[i];
          }
#endif
        }
        
        /*! Test if the ray is occluded by one of the triangles. */
    template<typename tsimdb, typename tsimdf, typename tsimdi>
      __forceinline bool occluded(Ray& ray, 
                                 const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2, 
                                  const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs,  Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          typedef Vec3<tsimdf> tsimd3f;
          const tsimd3f O = tsimd3f(ray.org);
          const tsimd3f D = tsimd3f(ray.dir);
          const tsimd3f v0 = tri_v0-O;
          const tsimd3f v1 = tri_v1-O;
          const tsimd3f v2 = tri_v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return false;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear)) & (absDen*tsimdf(ray.tfar) >= T);
          if (unlikely(none(valid))) return false;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return false;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return false;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri_mask & ray.mask) != 0;
          if (unlikely(none(valid))) return false;
#endif
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          size_t m=movemask(valid), i=__bsf(m);
          while (true)
          {  
            const int geomID = tri_geomIDs[i];
            Geometry* geometry = scene->get(geomID);
            
            /* if we have no filter then the test patsimds */
            if (likely(!geometry->hasOcclusionFilter1()))
              break;
            
            /* calculate hit information */
            const tsimdf rcpAbsDen = rcp(absDen);
            const tsimdf u = U * rcpAbsDen;
            const tsimdf v = V * rcpAbsDen;
            const tsimdf t = T * rcpAbsDen;
            const Vec3fa N = Vec3fa(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runOcclusionFilter1(geometry,ray,u[i],v[i],t[i],N,geomID,tri_primIDs[i])) 
              break;
            
            /* test if one more triangle hit */
            m=__btc(m,i); i=__bsf(m);
            if (m == 0) return false;
          }
#endif
          
          return true;
        }


    /*! Intersects a M rays with N triangles. */
    template<typename tsimdf, typename tsimdi, typename RayM>
      __forceinline void intersect(const typename RayM::simdb& valid0, RayM& ray, 
                                   const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2, 
                                   const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs, const size_t i, Scene* scene)
        {
          /* ray SIMD type shortcuts */
          typedef typename RayM::simdb rsimdb;
          typedef typename RayM::simdf rsimdf;
          typedef typename RayM::simdi rsimdi;
          typedef Vec3<rsimdf> rsimd3f;

          /* calculate vertices relative to ray origin */
          rsimdb valid = valid0;
            const rsimd3f O = ray.org;
            const rsimd3f D = ray.dir;
            const rsimd3f v0 = tri_v0-O;
            const rsimd3f v1 = tri_v1-O;
            const rsimd3f v2 = tri_v2-O;
            
            /* calculate triangle edges */
            const rsimd3f e0 = v2-v0;
            const rsimd3f e1 = v0-v1;
            const rsimd3f e2 = v1-v2;
            
            /* calculate geometry normal and denominator */
            const rsimd3f Ng1 = cross(e1,e0);
            const rsimd3f Ng = Ng1+Ng1;
            const rsimdf den = dot(rsimd3f(Ng),D);
            const rsimdf absDen = abs(den);
            const rsimdf sgnDen = signmsk(den);
            
            /* perform edge tests */
            const rsimdf U = dot(rsimd3f(cross(v2+v0,e0)),D) ^ sgnDen;
            valid &= U >= 0.0f;
            if (likely(none(valid))) return;
            const rsimdf V = dot(rsimd3f(cross(v0+v1,e1)),D) ^ sgnDen;
            valid &= V >= 0.0f;
            if (likely(none(valid))) return;
            const rsimdf W = dot(rsimd3f(cross(v1+v2,e2)),D) ^ sgnDen;
            valid &= W >= 0.0f;
            if (likely(none(valid))) return;
            
            /* perform depth test */
            const rsimdf T = dot(v0,rsimd3f(Ng)) ^ sgnDen;
            valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
            if (unlikely(none(valid))) return;
            
            /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
            valid &= den > rsimdf(zero);
            if (unlikely(none(valid))) return;
#else
            valid &= den != rsimdf(zero);
            if (unlikely(none(valid))) return;
#endif
            
            /* ray masking test */
#if defined(RTCORE_RAY_MASK)
            valid &= (tri_mask[i] & ray.mask) != 0;
            if (unlikely(none(valid))) return;
#endif
            
            /* calculate hit information */
            const rsimdf rcpAbsDen = rcp(absDen);
            const rsimdf u = U / absDen;
            const rsimdf v = V / absDen;
            const rsimdf t = T / absDen;
            const int geomID = tri_geomIDs[i];
            const int primID = tri_primIDs[i];
            
            /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
            Geometry* geometry = scene->get(geomID);
            if (unlikely(geometry->hasIntersectionFilter<rsimdf>())) {
              runIntersectionFilter(valid,geometry,ray,u,v,t,Ng,geomID,primID);
              return;
            }
#endif
            
            /* update hit information */
            rsimdf::store(valid,&ray.u,u);
            rsimdf::store(valid,&ray.v,v);
            rsimdf::store(valid,&ray.tfar,t);
            rsimdi::store(valid,&ray.geomID,geomID);
            rsimdi::store(valid,&ray.primID,primID);
            rsimdf::store(valid,&ray.Ng.x,Ng.x);
            rsimdf::store(valid,&ray.Ng.y,Ng.y);
            rsimdf::store(valid,&ray.Ng.z,Ng.z);
          }

        
        /*! Test for M rays if they are occluded by any of the N triangle. */
  template<typename tsimdf, typename tsimdi, typename RayM>
    __forceinline void occluded(typename RayM::simdb& valid0, RayM& ray, 
                                  const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2, 
                                  const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs, const size_t i, Scene* scene)
        {
          /* ray SIMD type shortcuts */
          typedef typename RayM::simdb rsimdb;
          typedef typename RayM::simdf rsimdf;
          typedef typename RayM::simdi rsimdi;
          typedef Vec3<rsimdf> rsimd3f;

            /* calculate vertices relative to ray origin */
          rsimdb valid = valid0;
            const rsimd3f O = ray.org;
            const rsimd3f D = ray.dir;
            const rsimd3f v0 = tri_v0-O;
            const rsimd3f v1 = tri_v1-O;
            const rsimd3f v2 = tri_v2-O;
            
            /* calculate triangle edges */
            const rsimd3f e0 = v2-v0;
            const rsimd3f e1 = v0-v1;
            const rsimd3f e2 = v1-v2;
            
            /* calculate geometry normal and denominator */
            const rsimd3f Ng1 = cross(e1,e0);
            const rsimd3f Ng = Ng1+Ng1;
            const rsimdf den = dot(rsimd3f(Ng),D);
            const rsimdf absDen = abs(den);
            const rsimdf sgnDen = signmsk(den);
            
            /* perform edge tests */
            const rsimdf U = dot(rsimd3f(cross(v2+v0,e0)),D) ^ sgnDen;
            valid &= U >= 0.0f;
            if (likely(none(valid))) return;
            const rsimdf V = dot(rsimd3f(cross(v0+v1,e1)),D) ^ sgnDen;
            valid &= V >= 0.0f;
            if (likely(none(valid))) return;
            const rsimdf W = dot(rsimd3f(cross(v1+v2,e2)),D) ^ sgnDen;
            valid &= W >= 0.0f;
            if (likely(none(valid))) return;
            
            /* perform depth test */
            const rsimdf T = dot(v0,rsimd3f(Ng)) ^ sgnDen;
            valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
            
            /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
            valid &= den > rsimdf(zero);
            if (unlikely(none(valid))) return;
#else
            valid &= den != rsimdf(zero);
            if (unlikely(none(valid))) return;
#endif
            
            /* ray masking test */
#if defined(RTCORE_RAY_MASK)
            valid &= (tri_mask[i] & ray.mask) != 0;
            if (unlikely(none(valid))) return;
#endif
            
            /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
            const int geomID = tri_geomIDs[i];
            Geometry* geometry = scene->get(geomID);
            if (unlikely(geometry->hasOcclusionFilter<rsimdf>()))
            {
              /* calculate hit information */
              const rsimdf rcpAbsDen = rcp(absDen);
              const rsimdf u = U / absDen;
              const rsimdf v = V / absDen;
              const rsimdf t = T / absDen;
              const int primID = tri_primIDs[i];
              valid = runOcclusionFilter(valid,geometry,ray,u,v,t,Ng,geomID,primID);
            }
#endif
            
            /* update occlusion */
            valid0 &= !valid;
        }
        
        /*! Intersect a ray with the N triangles and updates the hit. */
  template<typename tsimdf, typename tsimdi, typename RayM>
        __forceinline void intersect(RayM& ray, size_t k, 
                                     const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2,
                                     const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs, Scene* scene)
        {
          /* type shortcuts */
          typedef typename RayM::simdf rsimdf;
          typedef typename tsimdf::Mask tsimdb;
          typedef Vec3<tsimdf> tsimd3f;

          /* calculate vertices relative to ray origin */
          const tsimd3f O = broadcast<tsimdf>(ray.org,k);
          const tsimd3f D = broadcast<tsimdf>(ray.dir,k);
          const tsimd3f v0 = tri_v0-O;
          const tsimd3f v1 = tri_v1-O;
          const tsimd3f v2 = tri_v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear[k])) & (absDen*tsimdf(ray.tfar[k]) >= T);
          if (unlikely(none(valid))) return;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri_mask & ray.mask[k]) != 0;
          if (unlikely(none(valid))) return;
#endif
          
          /* calculate hit information */
          const tsimdf u = U / absDen;
          const tsimdf v = V / absDen;
          const tsimdf t = T / absDen;
          size_t i = select_min(valid,t);
          int geomID = tri_geomIDs[i];
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasIntersectionFilter<rsimdf>())) 
            {
#endif
              /* update hit information */
              ray.u[k] = u[i];
              ray.v[k] = v[i];
              ray.tfar[k] = t[i];
              ray.Ng.x[k] = Ng.x[i];
              ray.Ng.y[k] = Ng.y[i];
              ray.Ng.z[k] = Ng.z[i];
              ray.geomID[k] = geomID;
              ray.primID[k] = tri_primIDs[i];
              
#if defined(RTCORE_INTERSECTION_FILTER)
              return;
            }
            
            const Vec3fa N(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runIntersectionFilter(geometry,ray,k,u[i],v[i],t[i],N,geomID,tri_primIDs[i])) return;
            valid[i] = 0;
            if (unlikely(none(valid))) return;
            i = select_min(valid,t);
            geomID = tri_geomIDs[i];
          }
#endif
        }
        
        /*! Test if the ray is occluded by one of the triangles. */
  template<typename tsimdf, typename tsimdi, typename RayM>
    __forceinline bool occluded(RayM& ray, size_t k, 
                                const Vec3<tsimdf>& tri_v0, const Vec3<tsimdf>& tri_v1, const Vec3<tsimdf>& tri_v2, 
                                const tsimdi& tri_geomIDs, const tsimdi& tri_primIDs, Scene* scene)
        {
          /* type shortcuts */
          typedef typename RayM::simdf rsimdf;
          typedef typename tsimdf::Mask tsimdb;
          typedef Vec3<tsimdf> tsimd3f;

          /* calculate vertices relative to ray origin */
          const tsimd3f O = broadcast<tsimdf>(ray.org,k);
          const tsimd3f D = broadcast<tsimdf>(ray.dir,k);
          const tsimd3f v0 = tri_v0-O;
          const tsimd3f v1 = tri_v1-O;
          const tsimd3f v2 = tri_v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return false;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear[k])) & (absDen*tsimdf(ray.tfar[k]) >= T);
          if (unlikely(none(valid))) return false;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return false;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return false;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri_mask & ray.mask[k]) != 0;
          if (unlikely(none(valid))) return false;
#endif
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          
          size_t i = select_min(valid,T);
          int geomID = tri_geomIDs[i];
          
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasOcclusionFilter<rsimdf>())) break;
            
            /* calculate hit information */
            const tsimdf u = U / absDen;
            const tsimdf v = V / absDen;
            const tsimdf t = T / absDen;
            const Vec3fa N(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runOcclusionFilter(geometry,ray,k,u[i],v[i],t[i],N,geomID,tri_primIDs[i])) break;
            valid[i] = 0;
            if (unlikely(none(valid))) return false;
            i = select_min(valid,T);
            geomID = tri_geomIDs[i];
          }
#endif
          
          return true;
        }

















    /*! Intersects N triangles with 1 ray */
    template<typename TriangleNv>
      struct TriangleNvIntersector1Pluecker
      {
        typedef TriangleNv Primitive;

        /* type shortcuts */
        typedef typename TriangleNv::simdb tsimdb;
        typedef typename TriangleNv::simdf tsimdf;
        typedef typename TriangleNv::simdi tsimdi;
        typedef Vec3<tsimdf> tsimd3f;
        
        struct Precalculations {
          __forceinline Precalculations (const Ray& ray, const void* ptr) {}
        };
        
        /*! Intersect a ray with the 4 triangles and updates the hit. */
        static __forceinline void intersect(const Precalculations& pre, Ray& ray, const Primitive& tri, Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          STAT3(normal.trav_prims,1,1,1);
#if 1
          embree::isa::intersect<tsimdb,tsimdf,tsimdi>(ray,tri.v0,tri.v1,tri.v2,tri.geomIDs,tri.primIDs,scene);// FIXME: add ray mask support
#else
          const tsimd3f O = tsimd3f(ray.org);
          const tsimd3f D = tsimd3f(ray.dir);
          const tsimd3f v0 = tri.v0-O;
          const tsimd3f v1 = tri.v1-O;
          const tsimd3f v2 = tri.v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear)) & (absDen*tsimdf(ray.tfar) >= T);
          if (unlikely(none(valid))) return;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri.mask & ray.mask) != 0;
          if (unlikely(none(valid))) return;
#endif
          
          /* calculate hit information */
          const tsimdf u = U / absDen;
          const tsimdf v = V / absDen;
          const tsimdf t = T / absDen;
          size_t i = select_min(valid,t);
          int geomID = tri.geomID(i);
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasIntersectionFilter1())) 
            {
#endif
              /* update hit information */
              ray.u = u[i];
              ray.v = v[i];
              ray.tfar = t[i];
              ray.Ng.x = Ng.x[i];
              ray.Ng.y = Ng.y[i];
              ray.Ng.z = Ng.z[i];
              ray.geomID = geomID;
              ray.primID = tri.primID(i);
              
#if defined(RTCORE_INTERSECTION_FILTER)
              return;
            }
            
            Vec3fa N = Vec3fa(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runIntersectionFilter1(geometry,ray,u[i],v[i],t[i],N,geomID,tri.primID(i))) return;
            valid[i] = 0;
            if (none(valid)) return;
            i = select_min(valid,t);
            geomID = tri.geomID(i);
          }
#endif
#endif
        }
        
        /*! Test if the ray is occluded by one of the triangles. */
        static __forceinline bool occluded(const Precalculations& pre, Ray& ray, const Primitive& tri, Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          STAT3(shadow.trav_prims,1,1,1);
#if 1
          return embree::isa::occluded<tsimdb,tsimdf,tsimdi>(ray,tri.v0,tri.v1,tri.v2,tri.geomIDs,tri.primIDs,scene);// FIXME: add ray mask support
#else
          const tsimd3f O = tsimd3f(ray.org);
          const tsimd3f D = tsimd3f(ray.dir);
          const tsimd3f v0 = tri.v0-O;
          const tsimd3f v1 = tri.v1-O;
          const tsimd3f v2 = tri.v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return false;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear)) & (absDen*tsimdf(ray.tfar) >= T);
          if (unlikely(none(valid))) return false;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return false;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return false;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri.mask & ray.mask) != 0;
          if (unlikely(none(valid))) return false;
#endif
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          size_t m=movemask(valid), i=__bsf(m);
          while (true)
          {  
            const int geomID = tri.geomID(i);
            Geometry* geometry = scene->get(geomID);
            
            /* if we have no filter then the test patsimds */
            if (likely(!geometry->hasOcclusionFilter1()))
              break;
            
            /* calculate hit information */
            const tsimdf rcpAbsDen = rcp(absDen);
            const tsimdf u = U * rcpAbsDen;
            const tsimdf v = V * rcpAbsDen;
            const tsimdf t = T * rcpAbsDen;
            const Vec3fa N = Vec3fa(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runOcclusionFilter1(geometry,ray,u[i],v[i],t[i],N,geomID,tri.primID(i))) 
              break;
            
            /* test if one more triangle hit */
            m=__btc(m,i); i=__bsf(m);
            if (m == 0) return false;
          }
#endif
          
          return true;
#endif
        }
      };

    template<typename RayM, typename TriangleNv>
      struct TriangleNvIntersectorMPluecker
      {
        typedef TriangleNv Primitive;
        
        /* triangle SIMD type shortcuts */
        typedef typename TriangleNv::simdb tsimdb;
        typedef typename TriangleNv::simdf tsimdf;
        typedef Vec3<tsimdf> tsimd3f;
      
        /* ray SIMD type shortcuts */
        typedef typename RayM::simdb rsimdb;
        typedef typename RayM::simdf rsimdf;
        typedef typename RayM::simdi rsimdi;
        typedef Vec3<rsimdf> rsimd3f;

        struct Precalculations {
          __forceinline Precalculations (const rsimdb& valid, const RayM& ray) {}
        };
        
        /*! Intersects a M rays with N triangles. */
        static __forceinline void intersect(const rsimdb& valid_i, Precalculations& pre, RayM& ray, const Primitive& tri, Scene* scene)
        {
          for (size_t i=0; i<TriangleNv::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(normal.trav_prims,1,popcnt(valid_i),RayM::size());
            
#if 0
            const rsimd3f v0 = broadcast<rsimdf>(tri.v0,i);
            const rsimd3f v1 = broadcast<rsimdf>(tri.v1,i);
            const rsimd3f v2 = broadcast<rsimdf>(tri.v2,i);
            embree::isa::intersect(valid_i,ray,v0,v1,v2,tri.geomIDs,tri.primIDs,i,scene);
#else

            /* calculate vertices relative to ray origin */
            rsimdb valid = valid_i;
            const rsimd3f O = ray.org;
            const rsimd3f D = ray.dir;
            const rsimd3f v0 = broadcast<rsimdf>(tri.v0,i)-O;
            const rsimd3f v1 = broadcast<rsimdf>(tri.v1,i)-O;
            const rsimd3f v2 = broadcast<rsimdf>(tri.v2,i)-O;
            
            /* calculate triangle edges */
            const rsimd3f e0 = v2-v0;
            const rsimd3f e1 = v0-v1;
            const rsimd3f e2 = v1-v2;
            
            /* calculate geometry normal and denominator */
            const rsimd3f Ng1 = cross(e1,e0);
            const rsimd3f Ng = Ng1+Ng1;
            const rsimdf den = dot(rsimd3f(Ng),D);
            const rsimdf absDen = abs(den);
            const rsimdf sgnDen = signmsk(den);
            
            /* perform edge tests */
            const rsimdf U = dot(rsimd3f(cross(v2+v0,e0)),D) ^ sgnDen;
            valid &= U >= 0.0f;
            if (likely(none(valid))) continue;
            const rsimdf V = dot(rsimd3f(cross(v0+v1,e1)),D) ^ sgnDen;
            valid &= V >= 0.0f;
            if (likely(none(valid))) continue;
            const rsimdf W = dot(rsimd3f(cross(v1+v2,e2)),D) ^ sgnDen;
            valid &= W >= 0.0f;
            if (likely(none(valid))) continue;
            
            /* perform depth test */
            const rsimdf T = dot(v0,rsimd3f(Ng)) ^ sgnDen;
            valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
            if (unlikely(none(valid))) continue;
            
            /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
            valid &= den > rsimdf(zero);
            if (unlikely(none(valid))) continue;
#else
            valid &= den != rsimdf(zero);
            if (unlikely(none(valid))) continue;
#endif
            
            /* ray masking test */
#if defined(RTCORE_RAY_MASK)
            valid &= (tri.mask[i] & ray.mask) != 0;
            if (unlikely(none(valid))) continue;
#endif
            
            /* calculate hit information */
            const rsimdf rcpAbsDen = rcp(absDen);
            const rsimdf u = U / absDen;
            const rsimdf v = V / absDen;
            const rsimdf t = T / absDen;
            const int geomID = tri.geomID(i);
            const int primID = tri.primID(i);
            
            /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
            Geometry* geometry = scene->get(geomID);
            if (unlikely(geometry->hasIntersectionFilter<rsimdf>())) {
              runIntersectionFilter(valid,geometry,ray,u,v,t,Ng,geomID,primID);
              continue;
            }
#endif
            
            /* update hit information */
            rsimdf::store(valid,&ray.u,u);
            rsimdf::store(valid,&ray.v,v);
            rsimdf::store(valid,&ray.tfar,t);
            rsimdi::store(valid,&ray.geomID,geomID);
            rsimdi::store(valid,&ray.primID,primID);
            rsimdf::store(valid,&ray.Ng.x,Ng.x);
            rsimdf::store(valid,&ray.Ng.y,Ng.y);
            rsimdf::store(valid,&ray.Ng.z,Ng.z);
#endif
          }
        }
        
        /*! Test for M rays if they are occluded by any of the N triangle. */
        static __forceinline rsimdb occluded(const rsimdb& valid_i, Precalculations& pre, RayM& ray, const Primitive& tri, Scene* scene)
        {
          rsimdb valid0 = valid_i;
          
          for (size_t i=0; i<TriangleNv::max_size(); i++)
          {
            if (!tri.valid(i)) break;
            STAT3(shadow.trav_prims,1,popcnt(valid_i),RayM::size());

#if 0
            const rsimd3f v0 = broadcast<rsimdf>(tri.v0,i);
            const rsimd3f v1 = broadcast<rsimdf>(tri.v1,i);
            const rsimd3f v2 = broadcast<rsimdf>(tri.v2,i);
            embree::isa::occluded(valid0,ray,v0,v1,v2,tri.geomIDs,tri.primIDs,i,scene);

#else            
            /* calculate vertices relative to ray origin */
            rsimdb valid = valid0;
            const rsimd3f O = ray.org;
            const rsimd3f D = ray.dir;
            const rsimd3f v0 = broadcast<rsimdf>(tri.v0,i)-O;
            const rsimd3f v1 = broadcast<rsimdf>(tri.v1,i)-O;
            const rsimd3f v2 = broadcast<rsimdf>(tri.v2,i)-O;
            
            /* calculate triangle edges */
            const rsimd3f e0 = v2-v0;
            const rsimd3f e1 = v0-v1;
            const rsimd3f e2 = v1-v2;
            
            /* calculate geometry normal and denominator */
            const rsimd3f Ng1 = cross(e1,e0);
            const rsimd3f Ng = Ng1+Ng1;
            const rsimdf den = dot(rsimd3f(Ng),D);
            const rsimdf absDen = abs(den);
            const rsimdf sgnDen = signmsk(den);
            
            /* perform edge tests */
            const rsimdf U = dot(rsimd3f(cross(v2+v0,e0)),D) ^ sgnDen;
            valid &= U >= 0.0f;
            if (likely(none(valid))) continue;
            const rsimdf V = dot(rsimd3f(cross(v0+v1,e1)),D) ^ sgnDen;
            valid &= V >= 0.0f;
            if (likely(none(valid))) continue;
            const rsimdf W = dot(rsimd3f(cross(v1+v2,e2)),D) ^ sgnDen;
            valid &= W >= 0.0f;
            if (likely(none(valid))) continue;
            
            /* perform depth test */
            const rsimdf T = dot(v0,rsimd3f(Ng)) ^ sgnDen;
            valid &= (T >= absDen*ray.tnear) & (absDen*ray.tfar >= T);
            
            /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
            valid &= den > rsimdf(zero);
            if (unlikely(none(valid))) continue;
#else
            valid &= den != rsimdf(zero);
            if (unlikely(none(valid))) continue;
#endif
            
            /* ray masking test */
#if defined(RTCORE_RAY_MASK)
            valid &= (tri.mask[i] & ray.mask) != 0;
            if (unlikely(none(valid))) continue;
#endif
            
            /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
            const int geomID = tri.geomID(i);
            Geometry* geometry = scene->get(geomID);
            if (unlikely(geometry->hasOcclusionFilter<rsimdf>()))
            {
              /* calculate hit information */
              const rsimdf rcpAbsDen = rcp(absDen);
              const rsimdf u = U / absDen;
              const rsimdf v = V / absDen;
              const rsimdf t = T / absDen;
              const int primID = tri.primID(i);
              valid = runOcclusionFilter(valid,geometry,ray,u,v,t,Ng,geomID,primID);
            }
#endif
            
            /* update occlusion */
            valid0 &= !valid;
#endif
            if (none(valid0)) break;
          }
          return !valid0;
        }
        
        /*! Intersect a ray with the N triangles and updates the hit. */
        static __forceinline void intersect(Precalculations& pre, RayM& ray, size_t k, const Primitive& tri, Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          STAT3(normal.trav_prims,1,1,1);
#if 0
          embree::isa::intersect(ray,k,tri.v0,tri.v1,tri.v2,tri.geomIDs,tri.primIDs,scene);
#else
          const tsimd3f O = broadcast<tsimdf>(ray.org,k);
          const tsimd3f D = broadcast<tsimdf>(ray.dir,k);
          const tsimd3f v0 = tri.v0-O;
          const tsimd3f v1 = tri.v1-O;
          const tsimd3f v2 = tri.v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear[k])) & (absDen*tsimdf(ray.tfar[k]) >= T);
          if (unlikely(none(valid))) return;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri.mask & ray.mask[k]) != 0;
          if (unlikely(none(valid))) return;
#endif
          
          /* calculate hit information */
          const tsimdf u = U / absDen;
          const tsimdf v = V / absDen;
          const tsimdf t = T / absDen;
          size_t i = select_min(valid,t);
          int geomID = tri.geomID(i);
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasIntersectionFilter<rsimdf>())) 
            {
#endif
              /* update hit information */
              ray.u[k] = u[i];
              ray.v[k] = v[i];
              ray.tfar[k] = t[i];
              ray.Ng.x[k] = Ng.x[i];
              ray.Ng.y[k] = Ng.y[i];
              ray.Ng.z[k] = Ng.z[i];
              ray.geomID[k] = geomID;
              ray.primID[k] = tri.primID(i);
              
#if defined(RTCORE_INTERSECTION_FILTER)
              return;
            }
            
            const Vec3fa N(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runIntersectionFilter(geometry,ray,k,u[i],v[i],t[i],N,geomID,tri.primID(i))) return;
            valid[i] = 0;
            if (unlikely(none(valid))) return;
            i = select_min(valid,t);
            geomID = tri.geomID(i);
          }
#endif
#endif
        }
        
        /*! Test if the ray is occluded by one of the triangles. */
        static __forceinline bool occluded(Precalculations& pre, RayM& ray, size_t k, const Primitive& tri, Scene* scene)
        {
          /* calculate vertices relative to ray origin */
          STAT3(shadow.trav_prims,1,1,1);
#if 0
          return embree::isa::occluded(ray,k,tri.v0,tri.v1,tri.v2,tri.geomIDs,tri.primIDs,scene);
#else
          const tsimd3f O = broadcast<tsimdf>(ray.org,k);
          const tsimd3f D = broadcast<tsimdf>(ray.dir,k);
          const tsimd3f v0 = tri.v0-O;
          const tsimd3f v1 = tri.v1-O;
          const tsimd3f v2 = tri.v2-O;
          
          /* calculate triangle edges */
          const tsimd3f e0 = v2-v0;
          const tsimd3f e1 = v0-v1;
          const tsimd3f e2 = v1-v2;
          
          /* calculate geometry normal and denominator */
          const tsimd3f Ng1 = cross(e1,e0);
          const tsimd3f Ng = Ng1+Ng1;
          const tsimdf den = dot(Ng,D);
          const tsimdf absDen = abs(den);
          const tsimdf sgnDen = signmsk(den);
          
          /* perform edge tests */
          const tsimdf U = dot(cross(v2+v0,e0),D) ^ sgnDen;
          const tsimdf V = dot(cross(v0+v1,e1),D) ^ sgnDen;
          const tsimdf W = dot(cross(v1+v2,e2),D) ^ sgnDen;
          tsimdb valid = (U >= 0.0f) & (V >= 0.0f) & (W >= 0.0f);
          if (unlikely(none(valid))) return false;
          
          /* perform depth test */
          const tsimdf T = dot(v0,Ng) ^ sgnDen;
          valid &= (T >= absDen*tsimdf(ray.tnear[k])) & (absDen*tsimdf(ray.tfar[k]) >= T);
          if (unlikely(none(valid))) return false;
          
          /* perform backface culling */
#if defined(RTCORE_BACKFACE_CULLING)
          valid &= den > tsimdf(zero);
          if (unlikely(none(valid))) return false;
#else
          valid &= den != tsimdf(zero);
          if (unlikely(none(valid))) return false;
#endif
          
          /* ray masking test */
#if defined(RTCORE_RAY_MASK)
          valid &= (tri.mask & ray.mask[k]) != 0;
          if (unlikely(none(valid))) return false;
#endif
          
          /* intersection filter test */
#if defined(RTCORE_INTERSECTION_FILTER)
          
          size_t i = select_min(valid,T);
          int geomID = tri.geomID(i);
          
          while (true) 
          {
            Geometry* geometry = scene->get(geomID);
            if (likely(!geometry->hasOcclusionFilter<rsimdf>())) break;
            
            /* calculate hit information */
            const tsimdf u = U / absDen;
            const tsimdf v = V / absDen;
            const tsimdf t = T / absDen;
            const Vec3fa N(Ng.x[i],Ng.y[i],Ng.z[i]);
            if (runOcclusionFilter(geometry,ray,k,u[i],v[i],t[i],N,geomID,tri.primID(i))) break;
            valid[i] = 0;
            if (unlikely(none(valid))) return false;
            i = select_min(valid,T);
            geomID = tri.geomID(i);
          }
#endif
          
          return true;
#endif
        }
      };
  }
}