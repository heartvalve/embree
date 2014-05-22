#include "bvh4hair.h"
#include "bvh4hair_builder.h"
#include "common/accelinstance.h"
#include "geometry/triangle1.h"

namespace embree
{
#define DBG(x) 

  DECLARE_SYMBOL(Accel::Intersector1 ,BVH4HairIntersector1Bezier1i);
  DECLARE_SYMBOL(Accel::Intersector16,BVH4HairIntersector16Bezier1i);

  void BVH4HairRegister () 
  {
    int features = getCPUFeatures();

    SELECT_SYMBOL_KNC(features,BVH4HairIntersector1Bezier1i);
    SELECT_SYMBOL_KNC(features,BVH4HairIntersector16Bezier1i);

  }

  Accel::Intersectors BVH4HairIntersectors(BVH4i* bvh)
  {
    Accel::Intersectors intersectors;
    intersectors.ptr = bvh;
    intersectors.intersector1  = BVH4HairIntersector1Bezier1i;
    intersectors.intersector16 = BVH4HairIntersector16Bezier1i;
    return intersectors;
  }

  Accel* BVH4Hair::BVH4HairBinnedSAH(Scene* scene)
  {
    BVH4Hair* accel = new BVH4Hair(SceneTriangle1::type,scene);    
    Builder* builder = new BVH4HairBuilder(accel,NULL,scene);   
    Accel::Intersectors intersectors = BVH4HairIntersectors(accel);
    return new AccelInstance(accel,builder,intersectors);    
  }

  // =================================================================================
  // =================================================================================
  // =================================================================================


  __aligned(64) float BVH4Hair::UnalignedNode::identityMatrix[16] = {
    1,0,0,0,
    0,1,0,0,
    0,0,1,0,
    0,0,0,1
  };

  void BVH4Hair::UnalignedNode::convertFromBVH4iNode(const BVH4i::Node &bvh4i_node, UnalignedNode *ptr)
  {    
    DBG(PING);
    setIdentityMatrix();
    for (size_t m=0;m<4;m++)
      {
	const float dx = bvh4i_node.upper[m].x - bvh4i_node.lower[m].x;
	const float dy = bvh4i_node.upper[m].y - bvh4i_node.lower[m].y;
	const float dz = bvh4i_node.upper[m].z - bvh4i_node.lower[m].z;
	DBG(
	    DBG_PRINT(bvh4i_node);
	    DBG_PRINT(dx);
	    DBG_PRINT(dy);
	    DBG_PRINT(dz);
	    );

	const float inv_dx = 1.0f / dx;
	const float inv_dy = 1.0f / dy;
	const float inv_dz = 1.0f / dz;
	const float min_x = bvh4i_node.lower[m].x;
	const float min_y = bvh4i_node.lower[m].y;
	const float min_z = bvh4i_node.lower[m].z;
	set_scale(inv_dx,inv_dy,inv_dz,m);
	set_translation(-min_x*inv_dx,-min_y*inv_dy,-min_z*inv_dz,m);

	BVH4i::NodeRef n4i = bvh4i_node.child(m);
	if (n4i.isLeaf())
	  {
	    const size_t index = n4i.offsetIndex();
	    const size_t items = n4i.items();
	    
	    DBG(
		DBG_PRINT(index);
		DBG_PRINT(items);
		);

	    child(m) = (size_t)n4i;
	  }
	else
	  {
	    const size_t nodeID = n4i.nodeID();
	    DBG(
		DBG_PRINT(nodeID);
		);
	    child(m) = (size_t)&ptr[nodeID]; 	    
	  }
      }
    DBG(std::cout << *this << std::endl);

    //exit(0);
  }




};