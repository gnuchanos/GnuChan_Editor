/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "KX_NavMeshObject.h"
#include "KX_Mesh.h"
#include "RAS_DisplayArray.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

extern "C" {
#  include "BKE_scene.h"
#  include "BKE_customdata.h"
#  include "BKE_cdderivedmesh.h"
#  include "BKE_DerivedMesh.h"
#  include "BKE_navmesh_conversion.h"

#  include "BLI_alloca.h"
}

#include "KX_Globals.h"
#include "KX_PyMath.h"
#include "EXP_Value.h"
#include "Recast.h"
#include "KX_ObstacleSimulation.h"

#include "CM_Message.h"

#define MAX_PATH_LEN 256
static const float polyPickExt[3] = {2, 4, 2};

static void calcMeshBounds(const float *vert, int nverts, float *bmin, float *bmax)
{
	bmin[0] = bmax[0] = vert[0];
	bmin[1] = bmax[1] = vert[1];
	bmin[2] = bmax[2] = vert[2];
	for (int i = 1; i < nverts; i++) {
		if (bmin[0] > vert[3 * i + 0]) {
			bmin[0] = vert[3 * i + 0];
		}
		if (bmin[1] > vert[3 * i + 1]) {
			bmin[1] = vert[3 * i + 1];
		}
		if (bmin[2] > vert[3 * i + 2]) {
			bmin[2] = vert[3 * i + 2];
		}

		if (bmax[0] < vert[3 * i + 0]) {
			bmax[0] = vert[3 * i + 0];
		}
		if (bmax[1] < vert[3 * i + 1]) {
			bmax[1] = vert[3 * i + 1];
		}
		if (bmax[2] < vert[3 * i + 2]) {
			bmax[2] = vert[3 * i + 2];
		}
	}
}

inline void flipAxes(mt::vec3& vec)
{
	std::swap(vec.y, vec.z);
}

inline void flipAxes(float vec[3])
{
	std::swap(vec[1], vec[2]);
}

KX_NavMeshObject::KX_NavMeshObject(void *sgReplicationInfo, SG_Callbacks callbacks)
	:KX_GameObject(sgReplicationInfo, callbacks),
	m_navMesh(nullptr),
	m_navQuery(nullptr)
{
}

KX_NavMeshObject::~KX_NavMeshObject()
{
	if (m_navQuery) {
		delete m_navQuery;
	}
	if (m_navMesh) {
		delete m_navMesh;
	}
}

EXP_Value *KX_NavMeshObject::GetReplica()
{
	KX_NavMeshObject *replica = new KX_NavMeshObject(*this);
	replica->ProcessReplica();
	return replica;
}

void KX_NavMeshObject::ProcessReplica()
{
	KX_GameObject::ProcessReplica();
	m_navMesh = nullptr;
}

int KX_NavMeshObject::GetGameObjectType() const
{
	return OBJ_NAVMESH;
}


bool KX_NavMeshObject::BuildVertIndArrays(float *&vertices, int& nverts,
                                          unsigned short * &polys, int& npolys, unsigned short *&dmeshes,
                                          float *&dvertices, int &ndvertsuniq, unsigned short *&dtris,
                                          int& ndtris, int &vertsPerPoly)
{
	DerivedMesh *dm = mesh_create_derived_no_virtual(GetScene()->GetBlenderScene(), GetBlenderObject(),
	                                                 nullptr, CD_MASK_MESH);
	CustomData *pdata = dm->getPolyDataLayout(dm);
	int *recastData = (int *)CustomData_get_layer(pdata, CD_RECAST);
	if (recastData) {
		int *dtrisToPolysMap = nullptr, *dtrisToTrisMap = nullptr, *trisToFacesMap = nullptr;
		int nAllVerts = 0;
		float *allVerts = nullptr;
		buildNavMeshDataByDerivedMesh(dm, &vertsPerPoly, &nAllVerts, &allVerts, &ndtris, &dtris,
		                              &npolys, &dmeshes, &polys, &dtrisToPolysMap, &dtrisToTrisMap, &trisToFacesMap);

		MEM_SAFE_FREE(dtrisToPolysMap);
		MEM_SAFE_FREE(dtrisToTrisMap);
		MEM_SAFE_FREE(trisToFacesMap);

		unsigned short *verticesMap = (unsigned short *)MEM_mallocN(sizeof(*verticesMap) * nAllVerts, __func__);
		memset(verticesMap, 0xff, sizeof(*verticesMap) * nAllVerts);
		int curIdx = 0;
		//vertices - mesh verts
		//iterate over all polys and create map for their vertices first...
		for (int polyidx = 0; polyidx < npolys; polyidx++) {
			unsigned short *poly = &polys[polyidx * vertsPerPoly * 2];
			for (int i = 0; i < vertsPerPoly; i++) {
				unsigned short idx = poly[i];
				if (idx == 0xffff) {
					break;
				}
				if (verticesMap[idx] == 0xffff) {
					verticesMap[idx] = curIdx++;
				}
				poly[i] = verticesMap[idx];
			}
		}
		nverts = curIdx;
		//...then iterate over detailed meshes
		//transform indices to local ones (for each navigation polygon)
		for (int polyidx = 0; polyidx < npolys; polyidx++) {
			unsigned short *poly = &polys[polyidx * vertsPerPoly * 2];
			int nv = polyNumVerts(poly, vertsPerPoly);
			unsigned short *dmesh = &dmeshes[4 * polyidx];
			unsigned short tribase = dmesh[2];
			unsigned short trinum = dmesh[3];
			unsigned short vbase = curIdx;
			for (int j = 0; j < trinum; j++) {
				unsigned short *dtri = &dtris[(tribase + j) * 3 * 2];
				for (int k = 0; k < 3; k++) {
					int newVertexIdx = verticesMap[dtri[k]];
					if (newVertexIdx == 0xffff) {
						newVertexIdx = curIdx++;
						verticesMap[dtri[k]] = newVertexIdx;
					}

					if (newVertexIdx < nverts) {
						//it's polygon vertex ("shared")
						int idxInPoly = polyFindVertex(poly, vertsPerPoly, newVertexIdx);
						if (idxInPoly == -1) {
							CM_Error("building NavMeshObject, can't find vertex in polygon\n");
							return false;
						}
						dtri[k] = idxInPoly;
					}
					else {
						dtri[k] = newVertexIdx - vbase + nv;
					}
				}
			}
			dmesh[0] = vbase - nverts; //verts base
			dmesh[1] = curIdx - vbase; //verts num
		}

		vertices = new float[nverts * 3];
		ndvertsuniq = curIdx - nverts;
		if (ndvertsuniq > 0) {
			dvertices = new float[ndvertsuniq * 3];
		}
		for (int vi = 0; vi < nAllVerts; vi++) {
			int newIdx = verticesMap[vi];
			if (newIdx != 0xffff) {
				if (newIdx < nverts) {
					//navigation mesh vertex
					memcpy(vertices + 3 * newIdx, allVerts + 3 * vi, 3 * sizeof(float));
				}
				else {
					//detailed mesh vertex
					memcpy(dvertices + 3 * (newIdx - nverts), allVerts + 3 * vi, 3 * sizeof(float));
				}
			}
		}

		MEM_SAFE_FREE(allVerts);

		MEM_freeN(verticesMap);
	}
	else {
		//create from RAS_Mesh (detailed mesh is fake)
		KX_Mesh *meshobj = m_meshes.front();
		vertsPerPoly = 3;

		// Indices count.
		unsigned int numindices = 0;
		// Original (without split of normal or UV) vertex count.
		unsigned int numvertices = 0;

		for (RAS_MeshMaterial *meshmat : meshobj->GetMeshMaterialList()) {
			RAS_DisplayArray *array = meshmat->GetDisplayArray();

			numindices += array->GetTriangleIndexCount();
			numvertices = std::max(numvertices, array->GetMaxOrigIndex() + 1);
		}

		vertices = new float[numvertices * 3];
		polys = (unsigned short *)MEM_callocN(sizeof(unsigned short) * numindices, "BuildVertIndArrays polys");

		/// Map from original vertex index to m_vertexArray vertex index.
		std::vector<int> vertRemap(numvertices, -1);

		// Current vertex written.
		unsigned int curvert = 0;
		// Current index written.
		unsigned int curind = 0;
		for (RAS_MeshMaterial *meshmat : meshobj->GetMeshMaterialList()) {
			RAS_DisplayArray *array = meshmat->GetDisplayArray();
			// Convert location of all vertices and remap if vertices weren't already converted.
			for (unsigned int j = 0, numvert = array->GetVertexCount(); j < numvert; ++j) {
				const RAS_VertexInfo& info = array->GetVertexInfo(j);
				const unsigned int origIndex = info.GetOrigIndex();
				/* Avoid double conversion of two unique vertices using the same base:
				 * using the same original vertex and so the same position.
				 */
				if (vertRemap[origIndex] != -1) {
					continue;
				}

				copy_v3_v3(&vertices[curvert * 3], array->GetPosition(j).data);

				// Register the vertex index where the position was converted in m_vertexArray.
				vertRemap[origIndex] = curvert++;
			}

			for (unsigned int j = 0, numtris = array->GetTriangleIndexCount(); j < numtris; ++j) {
				const unsigned int index = array->GetTriangleIndex(j);
				const RAS_VertexInfo& info = array->GetVertexInfo(index);
				const unsigned int origIndex = info.GetOrigIndex();
				polys[curind++] = vertRemap[origIndex];
			}
		}

		npolys = numindices;
		dmeshes = nullptr;
		dvertices = nullptr;
		ndvertsuniq = 0;
		dtris = nullptr;
		ndtris = npolys;
	}
	dm->release(dm);

	return true;
}

bool KX_NavMeshObject::BuildNavMesh()
{
  KX_ObstacleSimulation *obssimulation = GetScene()->GetObstacleSimulation();

  if (obssimulation) {
    obssimulation->DestroyObstacleForObj(this);
  }

  if (m_navMesh) {
    delete m_navMesh;
    m_navMesh = nullptr;
  }

  if (m_meshes.empty()) {
    CM_Error("can't find mesh for navmesh object: " << m_name);
    return false;
  }

  // Raw input data (allocated with MEM_mallocN)
  float *vertices = nullptr, *dvertices = nullptr;
  unsigned short *polys = nullptr, *dtris = nullptr, *dmeshes = nullptr;
  int nverts = 0, npolys = 0, ndvertsuniq = 0, ndtris = 0;
  int vertsPerPoly = 0;

  if (!BuildVertIndArrays(vertices,
                          nverts,
                          polys,
                          npolys,
                          dmeshes,
                          dvertices,
                          ndvertsuniq,
                          dtris,
                          ndtris,
                          vertsPerPoly) ||
      vertsPerPoly < 3) {
    CM_Error("can't build navigation mesh data for object: " << m_name);
    if (vertices)
      delete[] vertices;
    if (dvertices)
      delete[] dvertices;
    return false;
  }

  if (!buildMeshAdjacency(polys, npolys, nverts, vertsPerPoly)) {
    CM_FunctionError("unable to build mesh adjacency information.");
    delete[] vertices;
    return false;
  }

  float cs = 0.2f;
  float bmin[3], bmax[3];
  calcMeshBounds(vertices, nverts, bmin, bmax);

  // Vertex quantization
  unsigned short *vertsi = new unsigned short[3 * nverts];
  float ics = 1.f / cs;
  for (int i = 0; i < nverts; i++) {
    vertsi[3 * i + 0] = static_cast<unsigned short>((vertices[3 * i + 0] - bmin[0]) * ics);
    vertsi[3 * i + 1] = static_cast<unsigned short>((vertices[3 * i + 1] - bmin[1]) * ics);
    vertsi[3 * i + 2] = static_cast<unsigned short>((vertices[3 * i + 2] - bmin[2]) * ics);
  }

  dtNavMeshCreateParams params{};
  params.verts = vertsi;
  params.vertCount = nverts;
  params.polys = polys;
  params.polyCount = npolys;
  params.nvp = vertsPerPoly;

  params.walkableHeight = 2.0f;
  params.walkableRadius = 0.6f;
  params.walkableClimb = 0.9f;

  params.cs = cs;
  params.ch = 0.2f;

  dtVcopy(params.bmin, bmin);
  dtVcopy(params.bmax, bmax);

  // Detail mesh disabled, the new version of Detour can do this. (could be enabled in the future)
  params.detailMeshes = nullptr;
  params.detailVerts = nullptr;
  params.detailVertsCount = 0;
  params.detailTris = nullptr;
  params.detailTriCount = 0;

  // Default areas: 0
  unsigned char *polyAreas = new unsigned char[npolys];
  for (int i = 0; i < npolys; ++i) {
    polyAreas[i] = 0;
  }
  params.polyAreas = polyAreas;

  // Default Flags: 1 (walkable)
  unsigned short *polyFlags = new unsigned short[npolys];
  for (int i = 0; i < npolys; ++i) {
    polyFlags[i] = 1;
  }
  params.polyFlags = polyFlags;

  params.buildBvTree = true;

  // Navmesh build
  unsigned char *navData = nullptr;
  int navDataSize = 0;

  if (!dtCreateNavMeshData(&params, &navData, &navDataSize)) {
    CM_FunctionError("Failed to build Detour navmesh");
    goto cleanup;
  }

  m_navMesh = new dtNavMesh();
  if (dtStatusFailed(m_navMesh->init(navData, navDataSize, DT_TILE_FREE_DATA))) {
    CM_FunctionError("Failed to init Detour navmesh");

    dtFree(navData);
    delete m_navMesh;
    m_navMesh = nullptr;
    goto cleanup;
  }

  if (obssimulation) {
    obssimulation->AddObstaclesForNavMesh(this);
  }

cleanup:
  delete[] vertices;
  delete[] vertsi;
  delete[] polyAreas;
  delete[] polyFlags;

  if (polys)
	MEM_freeN(polys);
  if (dmeshes)
    MEM_freeN(dmeshes);
  if (dtris)
    MEM_freeN(dtris);
  if (dvertices)
    delete[] dvertices;

  return true;
}




dtNavMesh *KX_NavMeshObject::GetNavMesh() const
{
	return m_navMesh;
}

dtNavMeshQuery *KX_NavMeshObject::GetNavMeshQuery() const
{
	return m_navQuery;
}

void KX_NavMeshObject::DrawNavMesh(NavMeshRenderMode renderMode) const
{
    if (!m_navMesh) {
        return;
    }

    const mt::vec4 color(0.0f, 0.0f, 0.0f, 1.0f);
    const int maxTiles = m_navMesh->getMaxTiles();

    for (int i = 0; i < maxTiles; ++i) {
        const dtMeshTile *tile = m_navMesh->getTile(i);
        if (!tile || !tile->header) continue;

        const dtPoly* polys = tile->polys;
        const int nPolys = tile->header->polyCount;

        switch (renderMode) {
            case RM_POLYS:
            case RM_WALLS:
                for (int pi = 0; pi < nPolys; ++pi) {
                    const dtPoly* poly = &polys[pi];

                    for (int vi = 0, vj = (int)poly->vertCount - 1; vi < (int)poly->vertCount; vj = vi++) {
                        if (poly->neis[vj] != 0 && renderMode == RM_WALLS) {
                            continue;
                        }
                        const float *v0 = &tile->verts[poly->verts[vi]*3];
                        const float *v1 = &tile->verts[poly->verts[vj]*3];

                        mt::vec3 p0(v0[0], v0[2], v0[1]);
                        mt::vec3 p1(v1[0], v1[2], v1[1]);
                        p0 = TransformToWorldCoords(p0);
                        p1 = TransformToWorldCoords(p1);

                        KX_RasterizerDrawDebugLine(p0, p1, color);
                    }
                }
                break;

            case RM_TRIS:
                if (!tile->detailMeshes || !tile->detailVerts || !tile->detailTris)
                    break;

                for (int pi = 0; pi < nPolys; ++pi) {
                    const dtPoly* p = &polys[pi];
                    const dtPolyDetail* pd = &tile->detailMeshes[pi];

                    for (int j = 0; j < pd->triCount; ++j) {
                        const unsigned char* t = &tile->detailTris[(pd->triBase + j) * 4];
                        mt::vec3 tri[3];

                        for (int k = 0; k < 3; ++k) {
                            const float* v;
                            if (t[k] < p->vertCount) {
                                v = &tile->verts[p->verts[t[k]] * 3];
                            } else {
                                v = &tile->detailVerts[(pd->vertBase + (t[k] - p->vertCount)) * 3];
                            }
                            float pos[3] = { v[0], v[1], v[2] };
                            flipAxes(pos);
                            tri[k] = mt::vec3(pos);
                        }

                        for (int k = 0; k < 3; ++k) {
                            tri[k] = TransformToWorldCoords(tri[k]);
                        }

                        for (int k = 0; k < 3; ++k) {
                            KX_RasterizerDrawDebugLine(tri[k], tri[(k + 1) % 3], color);
                        }
                    }
                }
                break;

            default:
                break;
        }
    }
}


mt::vec3 KX_NavMeshObject::TransformToLocalCoords(const mt::vec3& wpos) const
{
	return (NodeGetWorldTransform().Inverse() * wpos);
}

mt::vec3 KX_NavMeshObject::TransformToWorldCoords(const mt::vec3& lpos) const
{
	return (NodeGetWorldTransform() * lpos);
}

KX_NavMeshObject::PathType KX_NavMeshObject::FindPath(const mt::vec3& from, const mt::vec3& to, unsigned int maxPathLen) const
{
	PathType path;

	if (!m_navMesh) {
		return path;
	}

	mt::vec3 localfrom = TransformToLocalCoords(from);
	mt::vec3 localto = TransformToLocalCoords(to);
	flipAxes(localfrom);
	flipAxes(localto);

	float extents[3] = {2.0f, 4.0f, 2.0f}; // search box size

	// Initialize the query object
	dtNavMeshQuery navQuery;
	if (navQuery.init(m_navMesh, 2048) != DT_SUCCESS) {
		return path;
	}

	dtQueryFilter filter;

	// Find the polygons closest to the start and end points
	dtPolyRef sPolyRef = 0, ePolyRef = 0;
	float nearestStart[3], nearestEnd[3];

	if (navQuery.findNearestPoly(localfrom.Data(), extents, &filter, &sPolyRef, nearestStart) != DT_SUCCESS ||
	    navQuery.findNearestPoly(localto.Data(), extents, &filter, &ePolyRef, nearestEnd) != DT_SUCCESS) {
		return path;
	}

	if (!sPolyRef || !ePolyRef) {
		return path;
	}

	// Find the polygonal path
	dtPolyRef *polys = (dtPolyRef*)BLI_array_alloca(polys, maxPathLen);
	int npolys = 0;
	if (navQuery.findPath(sPolyRef, ePolyRef, nearestStart, nearestEnd, &filter, polys, &npolys, maxPathLen) != DT_SUCCESS || npolys <= 0) {
		return path;
	}

	// Find the direct path between the found polygons
	float (*points)[3] = (float(*)[3])BLI_array_alloca(points, maxPathLen);
	unsigned char *flags = (unsigned char*)BLI_array_alloca(flags, maxPathLen);
	dtPolyRef *straightPolys = (dtPolyRef*)BLI_array_alloca(straightPolys, maxPathLen);
	int pathLen = 0;

	if (navQuery.findStraightPath(nearestStart, nearestEnd, polys, npolys,
	                               &points[0][0], flags, straightPolys,
	                               &pathLen, maxPathLen) != DT_SUCCESS || pathLen <= 0) {
		return path;
	}

	// Transform the points back to world coordinates
	path.resize(pathLen);
	for (int i = 0; i < pathLen; ++i) {
		mt::vec3 waypoint(points[i]);
		flipAxes(waypoint);
		path[i] = TransformToWorldCoords(waypoint);
	}

	return path;
}


float KX_NavMeshObject::Raycast(const mt::vec3& from, const mt::vec3& to) const
{
	if (!m_navMesh) {
		return 0.f;
	}

	mt::vec3 localfrom = TransformToLocalCoords(from);
	mt::vec3 localto = TransformToLocalCoords(to);
	flipAxes(localfrom);
	flipAxes(localto);

	float extents[3] = {2.0f, 4.0f, 2.0f};
	dtNavMeshQuery navQuery;
	if (navQuery.init(m_navMesh, 2048) != DT_SUCCESS) {
		return 0.f;
	}

	dtQueryFilter filter;

	dtPolyRef startPoly = 0;
	float nearestPt[3];
	if (navQuery.findNearestPoly(localfrom.Data(), extents, &filter, &startPoly, nearestPt) != DT_SUCCESS || !startPoly) {
		return 0.f;
	}

	float t = 0.0f;
	float hitNormal[3] = {0,0,0};
	dtPolyRef polys[MAX_PATH_LEN];
	int nPolys = 0;

	if (navQuery.raycast(startPoly, localfrom.Data(), localto.Data(), &filter, &t, hitNormal, polys, &nPolys, MAX_PATH_LEN) != DT_SUCCESS) {
		return 0.f;
	}

	return t;
}

void KX_NavMeshObject::DrawPath(const PathType& path, const mt::vec4& color) const
{
	for (unsigned int i = 0, size = (path.size() - 1); i < size; ++i) {
		KX_RasterizerDrawDebugLine(path[i], path[i + 1], color);
	}
}

#ifdef WITH_PYTHON

PyTypeObject KX_NavMeshObject::Type = {
	PyVarObject_HEAD_INIT(nullptr, 0)
	"KX_NavMeshObject",
	sizeof(EXP_PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	0,
	0,
	0, 0, 0, 0, 0, 0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0, 0, 0, 0, 0, 0, 0,
	Methods,
	0,
	0,
	&KX_GameObject::Type,
	0, 0, 0, 0, 0, 0,
	py_base_new
};

PyAttributeDef KX_NavMeshObject::Attributes[] = {
	EXP_PYATTRIBUTE_NULL // Sentinel.
};

PyMethodDef KX_NavMeshObject::Methods[] = {
	EXP_PYMETHODTABLE(KX_NavMeshObject, findPath),
	EXP_PYMETHODTABLE(KX_NavMeshObject, raycast),
	EXP_PYMETHODTABLE(KX_NavMeshObject, draw),
	EXP_PYMETHODTABLE(KX_NavMeshObject, rebuild),
	{nullptr, nullptr} // Sentinel.
};

EXP_PYMETHODDEF_DOC(KX_NavMeshObject, findPath,
                    "findPath(start, goal): find path from start to goal points\n"
                    "Returns a path as list of points)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args, "OO:getPath", &ob_from, &ob_to)) {
		return nullptr;
	}
	mt::vec3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to)) {
		return nullptr;
	}

	const PathType path = FindPath(from, to, MAX_PATH_LEN);
	const unsigned int pathLen = path.size();
	PyObject *pathList = PyList_New(pathLen);
	for (unsigned int i = 0; i < pathLen; ++i) {
		PyList_SET_ITEM(pathList, i, PyObjectFrom(path[i]));
	}

	return pathList;
}

EXP_PYMETHODDEF_DOC(KX_NavMeshObject, raycast,
                    "raycast(start, goal): raycast from start to goal points\n"
                    "Returns hit factor)\n")
{
	PyObject *ob_from, *ob_to;
	if (!PyArg_ParseTuple(args, "OO:getPath", &ob_from, &ob_to)) {
		return nullptr;
	}
	mt::vec3 from, to;
	if (!PyVecTo(ob_from, from) || !PyVecTo(ob_to, to)) {
		return nullptr;
	}
	float hit = Raycast(from, to);
	return PyFloat_FromDouble(hit);
}

EXP_PYMETHODDEF_DOC(KX_NavMeshObject, draw,
                    "draw(mode): navigation mesh debug drawing\n"
                    "mode: WALLS, POLYS, TRIS\n")
{
	int arg;
	NavMeshRenderMode renderMode = RM_TRIS;
	if (PyArg_ParseTuple(args, "i:rebuild", &arg) && arg >= 0 && arg < RM_MAX) {
		renderMode = (NavMeshRenderMode)arg;
	}
	DrawNavMesh(renderMode);
	Py_RETURN_NONE;
}

EXP_PYMETHODDEF_DOC_NOARGS(KX_NavMeshObject, rebuild,
                           "rebuild(): rebuild navigation mesh\n")
{
	BuildNavMesh();
	Py_RETURN_NONE;
}

#endif  // WITH_PYTHON
