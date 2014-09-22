// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
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

#include "primitive.h"
#include "common/scene_subdiv_mesh.h"
#include "common/scene_subdivision.h"

namespace embree
{

  class RegularCatmullClarkPatch : public RegularCatmullClarkPatchT<Vec3fa> 
  {
  public:
    
  };


  struct SubdivPatch1
  {
  public:

    /*! Default constructor. */
    __forceinline SubdivPatch1 (const SubdivMesh::HalfEdge * first_half_edge,
				const Vec3fa *vertices,
				unsigned int geomID,
				unsigned int primID,
				size_t flags = 0) 
    : first_half_edge(first_half_edge),
      vertices(vertices),
      geomID(geomID),
      primID(primID),
      flags(flags)
  
    {}

    __forceinline const Vec3fa &getQuadVertex(const unsigned int i=0) const { 
      const SubdivMesh::HalfEdge *const h = first_half_edge + i;
      return vertices[h->vtx_index];
    }

    __forceinline unsigned int getStartVertexIndexFromHalfEdge(const SubdivMesh::HalfEdge * edge) const { 
      return edge->vtx_index;
    }

    __forceinline const Vec3fa &getStartVertexFromHalfEdge(const SubdivMesh::HalfEdge * edge) const { 
      return vertices[ getStartVertexIndexFromHalfEdge(edge) ];
    }

    __forceinline unsigned int getEndVertexIndexFromHalfEdge(const SubdivMesh::HalfEdge * edge) const { 
      SubdivMesh::HalfEdge *h_base = edge->base();
      return edge->next(h_base)->vtx_index;
    }

    __forceinline const Vec3fa &getEndVertexFromHalfEdge(const SubdivMesh::HalfEdge * edge) const { 
      return vertices[ getEndVertexIndexFromHalfEdge(edge) ];
    }

    __forceinline void init( RegularCatmullClarkPatch& cc_patch)
    {
      SubdivMesh::HalfEdge *base = first_half_edge->base();
      // quad(0,0)
      const SubdivMesh::HalfEdge *e11 = first_half_edge->half_circle( base );
      const SubdivMesh::HalfEdge *e10 = e11->next( base );
      const SubdivMesh::HalfEdge *e00 = e10->next( base );
      const SubdivMesh::HalfEdge *e01 = e10->next( base );

      cc_patch.v[1][1] = vertices[e11->getStartVertexIndex()];
      cc_patch.v[1][0] = vertices[e10->getStartVertexIndex()];
      cc_patch.v[0][0] = vertices[e00->getStartVertexIndex()];
      cc_patch.v[0][1] = vertices[e01->getStartVertexIndex()];

      // quad(0,2)
      const SubdivMesh::HalfEdge *e12 = first_half_edge->opposite( base )->half_circle( base );
      const SubdivMesh::HalfEdge *e13 = e12->next( base );
      const SubdivMesh::HalfEdge *e03 = e13->next( base );
      const SubdivMesh::HalfEdge *e02 = e03->next( base );

      cc_patch.v[1][2] = vertices[e12->getStartVertexIndex()];
      cc_patch.v[1][3] = vertices[e13->getStartVertexIndex()];
      cc_patch.v[0][3] = vertices[e03->getStartVertexIndex()];
      cc_patch.v[0][2] = vertices[e02->getStartVertexIndex()];

      // quad(2,0)
      const SubdivMesh::HalfEdge *e21 = first_half_edge->prev( base )->half_circle( base );
      const SubdivMesh::HalfEdge *e31 = e21->next( base );
      const SubdivMesh::HalfEdge *e30 = e31->next( base );
      const SubdivMesh::HalfEdge *e20 = e30->next( base );

      cc_patch.v[2][1] = vertices[e21->getStartVertexIndex()];
      cc_patch.v[3][1] = vertices[e31->getStartVertexIndex()];
      cc_patch.v[3][0] = vertices[e30->getStartVertexIndex()];
      cc_patch.v[2][0] = vertices[e20->getStartVertexIndex()];

      // quad(2,2)
      const SubdivMesh::HalfEdge *e22 = first_half_edge->next( base )->opposite( base)->half_circle( base );
      const SubdivMesh::HalfEdge *e32 = e22->next( base );
      const SubdivMesh::HalfEdge *e33 = e32->next( base );
      const SubdivMesh::HalfEdge *e23 = e33->next( base );

      cc_patch.v[2][2] = vertices[e22->getStartVertexIndex()];
      cc_patch.v[3][2] = vertices[e32->getStartVertexIndex()];
      cc_patch.v[3][3] = vertices[e33->getStartVertexIndex()];
      cc_patch.v[2][3] = vertices[e23->getStartVertexIndex()];      
    }
   
    const SubdivMesh::HalfEdge * first_half_edge; //!< pointer to first half edge of corresponding quad in the subdivision mesh
    const Vec3fa *vertices;                       //!< pointer to the vertex positions in the subdivison mesh
    size_t flags;
    unsigned int geomID;                          //!< geometry ID of the subdivision mesh this patch belongs to
    unsigned int primID;                          //!< primitive ID of this subdivision patch
  };

  __forceinline std::ostream &operator<<(std::ostream &o, const SubdivPatch1 &p)
    {
      o << "first_half_edge " << p.first_half_edge << " vertices " << p.vertices << " flags " << p.flags << " geomID " << p.geomID << " primID " << p.primID;

      return o;
    } 

  

}