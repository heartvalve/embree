// ======================================================================== //
// Copyright 2009-2013 Intel Corporation                                    //
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

#include "embree2/rtcore.h"
#include "common/alloc.h"
#include "common/accel.h"
#include "common/scene.h"
#include "geometry/primitive.h"
#include "geometry/bezier1.h"
#include "geometry/bezier1i.h"

#define BVH4HAIR_WIDTH 4
#define BVH4HAIR_COMPRESSION 0
#define BVH4HAIR_NAVIGATION 0
#if BVH4HAIR_NAVIGATION
#define NAVI(x) x
#else
#define NAVI(x)
#endif

namespace embree
{
  /*! BVH4 with unaligned bounds. */
  class BVH4Hair : public Bounded
  {
    ALIGNED_CLASS;
  public:
#if BVH4HAIR_WIDTH == 8
    typedef avxb simdb;
    typedef avxi simdi;
    typedef avxf simdf;
#elif BVH4HAIR_WIDTH == 4
    typedef sseb simdb;
    typedef ssei simdi;
    typedef ssef simdf;
#endif
    
    /*! forward declaration of node type */
    struct Node;
    struct AlignedNode;
    struct UnalignedNode;
    typedef AffineSpaceT<LinearSpace3<Vec3<simdf> > > AffineSpaceSOA4;
    typedef BBox<Vec3<ssef> > BBoxSSE3f;

    /*! branching width of the tree */
    static const size_t N = BVH4HAIR_WIDTH;

    /*! Number of address bits the Node and primitives are aligned
        to. Maximally 2^alignment-2 many primitive blocks per leaf are
        supported. */
    static const size_t alignment = 4;

    /*! Masks the bits that store the number of items per leaf. */
    static const size_t align_mask = (1 << alignment)-1;  
    static const size_t items_mask = (1 << (alignment-1))-1;  

    /*! Empty node */
    static const size_t emptyNode = 2;

    /*! Invalid node, used as marker in traversal */
    static const size_t invalidNode = (((size_t)-1) & (~items_mask)) | 2;
      
    /*! Maximal depth of the BVH. */
    static const size_t maxDepth = 32;
    static const size_t maxBuildDepth = 32;
    
    /*! Maximal number of primitive blocks in a leaf. */
    static const size_t maxLeafBlocks = items_mask-2;

    /*! Cost of one traversal step. */
    static const int travCostAligned = 1;
    static const int travCostUnaligned = 3;
    static const int intCost = 6;

    /*! Pointer that points to a node or a list of primitives */
    struct NodeRef
    {
      /*! Default constructor */
      __forceinline NodeRef () {}

      /*! Construction from integer */
      __forceinline NodeRef (size_t ptr) : ptr(ptr) { }

      /*! Cast to size_t */
      __forceinline operator size_t() const { return ptr; }

      /*! Prefetches the node this reference points to */
      __forceinline void prefetch() const 
      {
#if defined(__AVX2__) // FIXME: test if bring performance also on SNB
	prefetchL1(((char*)ptr)+0*64);
	prefetchL1(((char*)ptr)+1*64);
	prefetchL1(((char*)ptr)+2*64);
	prefetchL1(((char*)ptr)+3*64);
#if BVH4HAIR_WIDTH == 8
	prefetchL1(((char*)ptr)+4*64);
	prefetchL1(((char*)ptr)+5*64);
	prefetchL1(((char*)ptr)+6*64);
	prefetchL1(((char*)ptr)+7*64);
#endif
#endif
      }

      __forceinline void prefetch_L2() const 
      {
#if defined(__AVX2__) // FIXME: test if bring performance also on SNB
	prefetchL2(((char*)ptr)+0*64);
	prefetchL2(((char*)ptr)+1*64);
	prefetchL2(((char*)ptr)+2*64);
	prefetchL2(((char*)ptr)+3*64);
#if BVH4HAIR_WIDTH == 8
	prefetchL2(((char*)ptr)+4*64);
	prefetchL2(((char*)ptr)+5*64);
	prefetchL2(((char*)ptr)+6*64);
	prefetchL2(((char*)ptr)+7*64);
#endif
#endif
      }

      /*! checks if this is a leaf */
      __forceinline int isLeaf() const { return (ptr & (size_t)align_mask) > 1; }

      /*! checks if this is a node */
      __forceinline int isNode() const { return (ptr & (size_t)align_mask) <= 1; }
      
      /*! checks if this is a node with aligned bounding boxes */
      __forceinline int isAlignedNode() const { return (ptr & (size_t)align_mask) == 0; }

      /*! checks if this is a node with aligned bounding boxes */
      __forceinline int isUnalignedNode() const { return (ptr & (size_t)align_mask) == 1; }
      
      /*! returns node pointer */
      __forceinline       Node* node()       { assert(isNode()); return (      Node*)((size_t)ptr & ~(size_t)align_mask); }
      __forceinline const Node* node() const { assert(isNode()); return (const Node*)((size_t)ptr & ~(size_t)align_mask); }

      /*! returns aligned node pointer */
      __forceinline       AlignedNode* alignedNode()       { assert(isAlignedNode()); return (      AlignedNode*)ptr; }
      __forceinline const AlignedNode* alignedNode() const { assert(isAlignedNode()); return (const AlignedNode*)ptr; }

      /*! returns unaligned node pointer */
      __forceinline       UnalignedNode* unalignedNode()       { assert(isUnalignedNode()); return (      UnalignedNode*)((size_t)ptr & ~(size_t)align_mask); }
      __forceinline const UnalignedNode* unalignedNode() const { assert(isUnalignedNode()); return (const UnalignedNode*)((size_t)ptr & ~(size_t)align_mask); }
      
      /*! returns leaf pointer */
      __forceinline char* leaf(size_t& num) const {
        assert(isLeaf());
        num = (ptr & (size_t)items_mask)-2;
        return (char*)(ptr & ~(size_t)align_mask);
      }

    private:
      size_t ptr;
    };

    /*! Non-axis aligned bounds */
    struct NAABBox3fa
    {
    public:
      
      __forceinline NAABBox3fa () {}

      __forceinline NAABBox3fa (EmptyTy) 
        : space(one), bounds(empty) {}
      
      __forceinline NAABBox3fa (const BBox3fa& bounds) 
        : space(one), bounds(bounds) {}
      
      __forceinline NAABBox3fa (const LinearSpace3fa& space, const BBox3fa& bounds) 
        : space(space), bounds(bounds) {}

      friend std::ostream& operator<<(std::ostream& cout, const NAABBox3fa& p) {
        return std::cout << "{ space = " << p.space << ", bounds = " << p.bounds << "}";
      }
      
    public:
      LinearSpace3fa space; //!< orthonormal transformation
      BBox3fa bounds;       //!< bounds in transformed space
    };

    /*! Base Node structure */
    struct Node
    {
      /*! Clears the node. */
      __forceinline void clear() {
        for (size_t i=0; i<N; i++) children[i] = emptyNode;
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const NodeRef& childID) {
        assert(i < N);
        children[i] = childID;
      }

      /*! Returns reference to specified child */
      __forceinline       NodeRef& child(size_t i)       { assert(i<N); return children[i]; }
      __forceinline const NodeRef& child(size_t i) const { assert(i<N); return children[i]; }

    public:
      NodeRef children[N];   //!< Pointer to the children (can be a node or leaf)
    };

    /*! Node with aligned bounds */
    struct AlignedNode : public Node
    {
      /*! Clears the node. */
      __forceinline void clear() {
        lower_x = lower_y = lower_z = pos_inf; 
        upper_x = upper_y = upper_z = neg_inf;
        Node::clear();
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const BBox3fa& bounds, const NodeRef& childID) 
      {
        assert(i < N);
        lower_x[i] = bounds.lower.x; lower_y[i] = bounds.lower.y; lower_z[i] = bounds.lower.z;
        upper_x[i] = bounds.upper.x; upper_y[i] = bounds.upper.y; upper_z[i] = bounds.upper.z;
        Node::set(i,childID);
      }

      /*! Returns bounds of specified child. */
      __forceinline const BBox3fa bounds(size_t i) const { 
        assert(i < N);
        const Vec3fa lower(lower_x[i],lower_y[i],lower_z[i]);
        const Vec3fa upper(upper_x[i],upper_y[i],upper_z[i]);
        return BBox3fa(lower,upper);
      }

      /*! Returns the extend of the bounds of the ith child */
      __forceinline Vec3fa extend(size_t i) const {
        assert(i < N);
        return bounds(i).size();
      }

    public:
      simdf lower_x;           //!< X dimension of lower bounds of all 4 children.
      simdf upper_x;           //!< X dimension of upper bounds of all 4 children.
      simdf lower_y;           //!< Y dimension of lower bounds of all 4 children.
      simdf upper_y;           //!< Y dimension of upper bounds of all 4 children.
      simdf lower_z;           //!< Z dimension of lower bounds of all 4 children.
      simdf upper_z;           //!< Z dimension of upper bounds of all 4 children.
    };

#if BVH4HAIR_COMPRESSION

    /*! Node with unaligned bounds */
    struct UnalignedNode : public Node
    {
      /*! Clears the node. */
      __forceinline void clear() 
      {
        xfm_vx[0] = 1; xfm_vx[1] = 0; xfm_vx[2] = 0;
        xfm_vy[0] = 0; xfm_vy[1] = 1; xfm_vy[2] = 0;
        xfm_vz[0] = 0; xfm_vz[1] = 0; xfm_vz[2] = 1;
        offset = 0.0f; scale = 0.0f;
        lower_x[0] = lower_x[1] = lower_x[2] = lower_x[3] = 127;
        lower_y[0] = lower_y[1] = lower_y[2] = lower_y[3] = 127;
        lower_z[0] = lower_z[1] = lower_z[2] = lower_z[3] = 127;
        upper_x[0] = upper_x[1] = upper_x[2] = upper_x[3] = 127;
        upper_y[0] = upper_y[1] = upper_y[2] = upper_y[3] = 127;
        upper_z[0] = upper_z[1] = upper_z[2] = upper_z[3] = 127;
        align[0] = align[1] = align[2] = align[3] = 0;
        Node::clear();
      }

      /*! Sets non-axis aligned space of node and parent bounding box. */
      __forceinline void set(const LinearSpace3fa& space, const BBox3fa& bounds) 
      {
        xfm_vx[0] = (char) (128.0f*space.vx.x); assert((128.0f*space.vx.x >= -128.0f) && (128.0f*space.vx.x <= 127.0f) && (truncf(128.0f*space.vx.x) == 128.0f*space.vx.x));
        xfm_vx[1] = (char) (128.0f*space.vx.y); assert(128.0f*space.vx.y >= -128.0f && 128.0f*space.vx.y <= 127.0f && truncf(128.0f*space.vx.y) == 128.0f*space.vx.y);
        xfm_vx[2] = (char) (128.0f*space.vx.z); assert(128.0f*space.vx.z >= -128.0f && 128.0f*space.vx.z <= 127.0f && truncf(128.0f*space.vx.z) == 128.0f*space.vx.z);
        xfm_vx[3] = 0;
        xfm_vy[0] = (char) (128.0f*space.vy.x); assert(128.0f*space.vy.x >= -128.0f && 128.0f*space.vy.x <= 127.0f && truncf(128.0f*space.vy.x) == 128.0f*space.vy.x);
        xfm_vy[1] = (char) (128.0f*space.vy.y); assert(128.0f*space.vy.y >= -128.0f && 128.0f*space.vy.y <= 127.0f && truncf(128.0f*space.vy.y) == 128.0f*space.vy.y);
        xfm_vy[2] = (char) (128.0f*space.vy.z); assert(128.0f*space.vy.z >= -128.0f && 128.0f*space.vy.z <= 127.0f && truncf(128.0f*space.vy.z) == 128.0f*space.vy.z);
        xfm_vy[3] = 0;
        xfm_vz[0] = (char) (128.0f*space.vz.x); assert(128.0f*space.vz.x >= -128.0f && 128.0f*space.vz.x <= 127.0f && truncf(128.0f*space.vz.x) == 128.0f*space.vz.x);
        xfm_vz[1] = (char) (128.0f*space.vz.y); assert(128.0f*space.vz.y >= -128.0f && 128.0f*space.vz.y <= 127.0f && truncf(128.0f*space.vz.y) == 128.0f*space.vz.y);
        xfm_vz[2] = (char) (128.0f*space.vz.z); assert(128.0f*space.vz.z >= -128.0f && 128.0f*space.vz.z <= 127.0f && truncf(128.0f*space.vz.z) == 128.0f*space.vz.z);
        xfm_vz[3] = 0;
        offset = 128.0f*bounds.lower;
        scale  = 128.0f*bounds.size()/255.0f;
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const BBox3fa& bounds) 
      {
        assert(i < N);
        //PRINT2("seti",bounds);
        const Vec3fa lower = (128.0f*bounds.lower-Vec3fa(offset))/Vec3fa(scale);
        //PRINT(lower);
        //PRINT(128.0f*bounds.lower-Vec3fa(offset));
        assert(lower.x >= 0.0f && lower.x <= 255.0001f);
        assert(lower.y >= 0.0f && lower.y <= 255.0001f);
        assert(lower.z >= 0.0f && lower.z <= 255.0001f);
        lower_x[i] = (unsigned char) floorf(lower.x);
        lower_y[i] = (unsigned char) floorf(lower.y); 
        lower_z[i] = (unsigned char) floorf(lower.z);
        const Vec3fa upper = (128.0f*bounds.upper-Vec3fa(offset))/Vec3fa(scale);
        //PRINT(upper);
        assert(upper.x >= 0.0f && upper.x <= 255.0001f);
        assert(upper.y >= 0.0f && upper.y <= 255.0001f);
        assert(upper.z >= 0.0f && upper.z <= 255.0001f);
        upper_x[i] = (unsigned char) ceilf (upper.x);
        upper_y[i] = (unsigned char) ceilf (upper.y); 
        upper_z[i] = (unsigned char) ceilf (upper.z);
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const NodeRef& childID) {
        Node::set(i,childID);
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const BBox3fa& bounds, const NodeRef& childID) 
      {
        set(i,bounds);
        Node::set(i,childID);
      }

      /*! returns transformation */
      __forceinline const LinearSpace3fa getXfm() const 
      {
        //const ssei v = *(ssei*)&xfm_vx;
        const ssef vx = ssef(_mm_cvtepi8_epi32(*(ssei*)&xfm_vx));
        const ssef vy = ssef(_mm_cvtepi8_epi32(*(ssei*)&xfm_vy));
        const ssef vz = ssef(_mm_cvtepi8_epi32(*(ssei*)&xfm_vz));
        return LinearSpace3fa((Vec3fa)vx,(Vec3fa)vy,(Vec3fa)vz);
      }

      /*! returns 4 bounding boxes */
      __forceinline BBoxSSE3f getBounds() const 
      {
        const Vec3fa offset = *(Vec3fa*)&this->offset;
        const Vec3fa scale  = *(Vec3fa*)&this->scale;
        //PRINT(offset);
        //PRINT(scale);
        //const ssei lower = *(ssei*)&this->lower_x;
        const ssef lower_x = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->lower_x));
        const ssef lower_y = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->lower_y));
        const ssef lower_z = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->lower_z));
        //PRINT(lower_x);
        //PRINT(lower_y);
        //PRINT(lower_z);
        //const ssei upper = *(ssei*)&this->upper_x;
        const ssef upper_x = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->upper_x));
        const ssef upper_y = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->upper_y));
        const ssef upper_z = ssef(_mm_cvtepu8_epi32(*(ssei*)&this->upper_z));
        //PRINT(upper_x);
        //PRINT(upper_y);
        //PRINT(upper_z);
        //simdf slower_x = scale.x*lower_x + offset.x;
        //PRINT(slower_x);
        return BBoxSSE3f(Vec3<simdf>(scale)*Vec3<simdf>(lower_x,lower_y,lower_z)+Vec3<simdf>(offset),
                         Vec3<simdf>(scale)*Vec3<simdf>(upper_x,upper_y,upper_z)+Vec3<simdf>(offset));
      }

      /*! Returns the extend of the bounds of the ith child */
      __forceinline Vec3fa extend(size_t i) const 
      {
        assert(i < N);
        const sse3f s4 = getBounds().size();
        const Vec3f s(s4.x[i],s4.y[i],s4.z[i]);
        return s/128.0f;
      }

    public:
#if 1
      char xfm_vx[4];             //!< 1st column of transformation
      char xfm_vy[4];             //!< 2nd column of transformation
      char xfm_vz[4];             //!< 3rd column of transformation
      Vec3f offset;               //!< offset to decompress bounds
      Vec3f scale;                //!< scale  to decompress bounds
      unsigned char lower_x[4];   //!< X dimension of lower bounds of all 4 children.
      unsigned char lower_y[4];   //!< Y dimension of lower bounds of all 4 children.
      unsigned char lower_z[4];   //!< Z dimension of lower bounds of all 4 children.
      unsigned char upper_x[4];   //!< X dimension of upper bounds of all 4 children.
      unsigned char upper_y[4];   //!< Y dimension of upper bounds of all 4 children.
      unsigned char upper_z[4];   //!< Z dimension of upper bounds of all 4 children.
      char align[4];
#else
      LinearSpace3fa space;    //!< non-axis aligned space
      simdf lower_x;           //!< X dimension of lower bounds of all 4 children.
      simdf upper_x;           //!< X dimension of upper bounds of all 4 children.
      simdf lower_y;           //!< Y dimension of lower bounds of all 4 children.
      simdf upper_y;           //!< Y dimension of upper bounds of all 4 children.
      simdf lower_z;           //!< Z dimension of lower bounds of all 4 children.
      simdf upper_z;           //!< Z dimension of upper bounds of all 4 children.
#endif
    };

#else

    /*! Node with unaligned bounds */
    struct UnalignedNode : public Node
    {
      /*! Clears the node. */
      __forceinline void clear() 
      {
	AffineSpace3fa empty = AffineSpace3fa::scale(Vec3fa(1E+19));
	naabb.l.vx = empty.l.vx;
	naabb.l.vy = empty.l.vy;
	naabb.l.vz = empty.l.vz;
	naabb.p    = empty.p;
        Node::clear();
      }

      /*! Sets bounding box and ID of child. */
      __forceinline void set(size_t i, const NAABBox3fa& b, const NodeRef& childID) 
      {
        assert(i < N);

        AffineSpace3fa space = b.space;
        space.p -= b.bounds.lower;
        space = AffineSpace3fa::scale(1.0f/max(Vec3fa(1E-19),b.bounds.upper-b.bounds.lower))*space;
        
        naabb.l.vx.x[i] = space.l.vx.x;
        naabb.l.vx.y[i] = space.l.vx.y;
        naabb.l.vx.z[i] = space.l.vx.z;

        naabb.l.vy.x[i] = space.l.vy.x;
        naabb.l.vy.y[i] = space.l.vy.y;
        naabb.l.vy.z[i] = space.l.vy.z;

        naabb.l.vz.x[i] = space.l.vz.x;
        naabb.l.vz.y[i] = space.l.vz.y;
        naabb.l.vz.z[i] = space.l.vz.z;

        naabb.p.x[i] = space.p.x;
        naabb.p.y[i] = space.p.y;
        naabb.p.z[i] = space.p.z;

        Node::set(i,childID);
      }

      /*! Returns the extend of the bounds of the ith child */
      __forceinline Vec3fa extend(size_t i) const {
        assert(i<N);
        const Vec3fa vx(naabb.l.vx.x[i],naabb.l.vx.y[i],naabb.l.vx.z[i]);
        const Vec3fa vy(naabb.l.vy.x[i],naabb.l.vy.y[i],naabb.l.vy.z[i]);
        const Vec3fa vz(naabb.l.vz.x[i],naabb.l.vz.y[i],naabb.l.vz.z[i]);
        const Vec3fa p (naabb.p   .x[i],naabb.p   .y[i],naabb.p   .z[i]);
        return rsqrt(vx*vx + vy*vy + vz*vz);
      }

    public:
      AffineSpaceSOA4 naabb;   //!< non-axis aligned bounding boxes (bounds are [0,1] in specified space)
    };

#endif

  public:

    /*! BVH4Hair default constructor. */
    BVH4Hair (const PrimitiveType& primTy, Scene* scene);

    /*! BVH4Hair destruction */
    ~BVH4Hair ();

    /*! BVH4Hair instantiations */
    static Accel* BVH4HairBezier1(Scene* scene);
    static Accel* BVH4HairBezier1i(Scene* scene);

    /*! initializes the acceleration structure */
    void init (size_t numPrimitivesMin = 0, size_t numPrimitivesMax = 0);

    /*! allocator for nodes */
    LinearAllocatorPerThread alloc;

    /*! allocates a new aligned node */
    __forceinline AlignedNode* allocAlignedNode(size_t thread) {
      AlignedNode* node = (AlignedNode*) alloc.malloc(thread,sizeof(AlignedNode),1 << 4); node->clear(); return node;
    }

    /*! allocates a new unaligned node */
    __forceinline UnalignedNode* allocUnalignedNode(size_t thread) {
      UnalignedNode* node = (UnalignedNode*) alloc.malloc(thread,sizeof(UnalignedNode),1 << 4); node->clear(); return node;
    }

    /*! allocates a block of primitives */
    __forceinline char* allocPrimitiveBlocks(size_t thread, size_t num) {
      return (char*) alloc.malloc(thread,num*primTy.bytes,1 << 4);
    }

    /*! Encodes an alingned node */
    __forceinline NodeRef encodeNode(AlignedNode* node) { 
      return NodeRef((size_t) node);
    }

    /*! Encodes an unaligned node */
    __forceinline NodeRef encodeNode(UnalignedNode* node) { 
      return NodeRef(((size_t) node) | 1);
    }
    
    /*! Encodes a leaf */
    __forceinline NodeRef encodeLeaf(char* data, size_t num) {
      assert(!((size_t)data & align_mask)); 
      return NodeRef((size_t)data | (2+min(num,(size_t)maxLeafBlocks)));
    }

  public:
    const PrimitiveType& primTy;       //!< primitive type stored in the BVH
    Scene* scene;
    NodeRef root;  //!< Root node
    size_t numPrimitives;
    size_t numVertices;
  };
}
