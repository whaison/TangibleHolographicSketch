#include "pch_bcl.h"
#include "SurfaceInspectionPlanner.h"
#include "Scene.h"
#include <Models.h>
#include <VertexTypes.h>
#include <PrimitiveVisualizer.h>
#include <Eigen\Core>
#include "Cca.h"
#include <Geometrics\csg.h>
#include <Geometrics\Primitives.h>
#include "Pointer.h"
#include "Keyboard.h"
#include "TrackerdPen.h"

using namespace Causality;
using namespace Causality::SurfaceInspection;
using namespace DirectX::Scene;
using namespace Causality::Math;
using namespace DirectX;
using namespace DirectX::VertexTraits;

REGISTER_SCENE_OBJECT_IN_PARSER(surface_inspector, SurfaceInspectionPlanner);

float g_ControlPointsRaius = 0.005f;
float g_ControlPointsConnectionRadius = 0.002;
bool  g_VisualizeNormal = false;
static constexpr size_t g_DecalResolution = 1024;

namespace Causality
{
	extern bool		g_DebugView;
}

struct TeapotPatch
{
	bool mirrorZ;
	int indices[16];
};

// Static data array defines the bezier patches that make up the teapot.
extern const TeapotPatch TeapotPatches[10];

// Static array defines the control point positions that make up the teapot.
extern const DirectX::XMVECTORF32 TeapotControlPoints[127];

template <typename _VertexType, typename _IndexType>
inline std::shared_ptr<MeshBuffer> CreateMeshBuffer(IRenderDevice* pDevice,
	const Geometrics::TriangleMesh<_VertexType, _IndexType>& mesh)
{
	auto pMesh = make_shared<MeshBuffer>();
	pMesh->CreateDeviceResources(pDevice, mesh.vertices.data(), mesh.vertices.size(), mesh.indices.data(), mesh.indices.size());
	return pMesh;
}

void SurfaceInspectionPlanner::Parse(const ParamArchive * archive)
{
	using namespace DirectX::VertexTraits;
	SceneObject::Parse(archive);

	const char* cpstr = nullptr;
	std::vector<float> cps;
	const char* mesh_name = nullptr;

	m_cursor = CoreInputs::PrimaryPointer();
	m_decalBackground = Colors::Transparent.v;
	m_decalFill = Colors::LimeGreen.v;
	m_decalStroke = Colors::Black.v;
	//m_decalStroke.A(0.8f);
	//m_decalFill.A(0.8f);

	Color color = DirectX::Colors::White;

	m_cursorMaterial = std::make_shared<PhongMaterial>();
	m_cursorMaterial->Alpha = 0.5f;

	m_isReady = false;
	m_declDirtyFalg = 0;
	int tessellation = 9;
	GetParam(archive, "tessellation", tessellation);
	GetParam(archive, "color", color);
	float patchSize = 0.1f, margin = 0.001f, z_tolerence = 0.01;

	GetParam(archive, "patch_size", patchSize);
	GetParam(archive, "margin", margin);
	GetParam(archive, "z_tolerence", z_tolerence);
	GetParam(archive, "active_fill_color", m_decalFill);
	GetParam(archive, "stroke_color", m_decalStroke);

	//GetParamArray(archive, "control_points", cps);

	//if (cps.size() < 16 * 3)
	//	return;

	//std::copy_n(cps.data(), 3 * 16, (float*)m_patch.data());

	m_requestCancelLoading = 0;
	//m_loadingTask = concurrency::create_task([=]() {
	//	m_mesh.build();

	//	int xsize = (int)ceilf(obb.Extents.x * 2.0f / patchSize);
	//	int ysize = (int)ceilf(obb.Extents.z * 2.0f / patchSize);
	//	xsize = 0;
	//	ysize = 0;

	//	//m_mesh.transform(XMMatrixTranslationFromVector(-XMLoad(bb.Center)));

	//	using Geometrics::csg::BSPNode;

	//	std::cout << "Building mesh BSP tree..." << std::endl;

	//	uptr<BSPNode> meshNode;
	//	try
	//	{
	//		meshNode = BSPNode::create(m_mesh);
	//	}
	//	catch (const std::exception&)
	//	{
	//		return;
	//	}

	//	if (this->m_requestCancelLoading) return;

	//	Geometrics::TriangleMesh<VertexPositionNormalTexture> cubeMesh;
	//	DirectX::GeometricPrimitive::CreateCube(cubeMesh.vertices, cubeMesh.indices, 1.0f);

	//	if (this->m_requestCancelLoading) return;

	//	float zoffset = bb.Center.y;
	//	XMVECTOR sclFactor = XMVectorSet(0.9f * patchSize, 5.0 * z_tolerence, 0.9f *  patchSize, 1.0f);
	//	for (int i = 0; i < xsize; i++)
	//	{
	//		for (int j = 0; j < ysize; j++)
	//		{
	//			float xoffset = bb.Center.x - bb.Extents.x + (i + 0.5f) * patchSize;
	//			float yoffset = bb.Center.z - bb.Extents.z + (j + 0.5f) * patchSize;

	//			XMMATRIX M = XMMatrixAffineTransformation(
	//				sclFactor, XMVectorZero(), XMQuaternionIdentity(),
	//				XMVectorSet(xoffset, zoffset, yoffset, 0.0f));

	//			cubeMesh.transform(M);

	//			if (this->m_requestCancelLoading) return;

	//			// the area of interest for this index
	//			auto cubeNode = BSPNode::create(cubeMesh);

	//			if (this->m_requestCancelLoading) return;

	//			XMVECTOR det;
	//			M = XMMatrixInverse(&det, M);
	//			cubeMesh.transform(M); // transform back

	//			std::cout << "Computing mesh insection (" << i << ',' << j << ')' << std::endl;
	//			uptr<BSPNode> nodeRet;
	//			try
	//			{
	//				nodeRet.reset(Geometrics::csg::nodeSubtract(meshNode.get(), cubeNode.get()));
	//			}
	//			catch (const std::exception&)
	//			{
	//				return;
	//			}

	//			if (this->m_requestCancelLoading) return;

	//			// the segmented mesh
	//			m_fracorizedMeshes.emplace_back();
	//			try
	//			{
	//				nodeRet->convertToMesh(m_fracorizedMeshes.back());
	//			}
	//			catch (const std::exception&)
	//			{
	//				return;
	//			}

	//			if (this->m_requestCancelLoading) return;
	//		}
	//	}

	//	//m_projMesh = m_mesh; // copy the mesh
	//	//for (auto& v : m_projMesh.vertices)
	//	//{
	//	//	XMVECTOR p = get_position(v);
	//	//	p = XMVector3Rotate(p,quat);
	//	//	set_position(v, p);
	//	//}

	//	{
	//		auto pModel = new CompositionModel();
	//		this->m_factorizeModel.reset(pModel);
	//		if (this->m_requestCancelLoading) return;

	//		auto& parts = pModel->Parts;
	//		for (auto& mesh : m_fracorizedMeshes)
	//		{
	//			parts.emplace_back();
	//			auto& part = parts.back();

	//			if (this->m_requestCancelLoading) return;

	//			part.pMesh = CreateMeshBuffer(pDevice, mesh);
	//			part.Name = "factorized_patch";

	//			if (this->m_requestCancelLoading) return;

	//			CreateBoundingBoxesFromPoints(part.BoundBox, part.BoundOrientedBox,
	//				mesh.vertices.size(), &mesh.vertices[0].position, sizeof(TVertex));

	//			if (this->m_requestCancelLoading) return;
	//		}
	//		pModel->CreateBoundingGeometry();

	//		if (this->m_requestCancelLoading) return;

	//		auto pVisual = new VisualObject();
	//		pVisual->Scene = this->Scene;
	//		pVisual->SetRenderModel(pModel);

	//		if (this->m_requestCancelLoading) return;
	//		{
	//			this->AddChild(pVisual);
	//			std::lock_guard<std::mutex>(this->Scene->ContentMutex());
	//			this->Scene->SignalCameraCache();
	//		}
	//		//VisualObject::m_pRenderModel = pModel;
	//	}

	//	m_isReady = true;
	//});
}

// Gernerate a roll-patch-roll rotation that sort ext into 
inline XMVECTOR XM_CALLCONV GetSortRotation(XMFLOAT3& extent)
{
	XMVECTOR q = XMQuaternionIdentity();
	Vector3 ext = extent;

	if (ext.x < ext.y)
	{
		std::swap(ext.x, ext.y);
		q = XMQuaternionMultiply(XMQuaternionRotationRoll(XM_PIDIV2), q);
	}
	if (ext.y < ext.z)
	{
		std::swap(ext.y, ext.z);
		q = XMQuaternionMultiply(XMQuaternionRotationPatch(XM_PIDIV2), q);
	}
	if (ext.x < ext.y)
	{
		std::swap(ext.x, ext.y);
		q = XMQuaternionMultiply(XMQuaternionRotationRoll(XM_PIDIV2), q);
	}

	XMVECTOR rotated = XMVector3Rotate(XMLoad(ext), q);
	rotated = XMVectorAbs(rotated);
	extent = ext;
	return q;
}

void SortOrientedBoundingBox(BoundingOrientedBox& obb)
{
	auto q = GetSortRotation(obb.Extents);
	auto oq = XMLoad(obb.Orientation);
	q = XMQuaternionMultiply(q, oq);
	XMStore(obb.Orientation, q);
}

void SurfaceInspectionPlanner::ExtractWorkloadMesh(const IModelNode* pModel)
{
	if (pModel)
		CopyMesh(m_mesh, pModel);
	else
		TesselateBezeirPatch(10, Color(Colors::AliceBlue.f));

	BoundingBox bb, uvbb;
	BoundingOrientedBox obb;

	using TVertex = decltype(m_mesh.vertices[0]);
	using TIndex = decltype(m_mesh.indices[0]);

	XMVECTOR uvmin = XMVectorReplicate(100.0f);
	XMVECTOR uvmax = XMVectorReplicate(-100.0f);
	for (auto& v : m_mesh.vertices)
	{
		uvmin = XMVectorMin(get_uv(v), uvmin);
		uvmax = XMVectorMax(get_uv(v), uvmax);
	}
	uvmax = XMVectorReciprocal(uvmax - uvmin);
	uvmin = -uvmin;
	for (auto& v : m_mesh.vertices)
		set_uv(v, XMVectorMultiplyAdd(get_uv(v), uvmax, uvmin));

	// the BoundingOrientedBox is computed from an 3x3 svd, (pca of the vertices)
	CreateBoundingBoxesFromPoints(bb, obb,
		m_mesh.vertices.size(), &m_mesh.vertices[0].position, sizeof(TVertex));
	SortOrientedBoundingBox(obb);

	XMVECTOR t = -XMLoad(obb.Center);
	XMVECTOR q = XMQuaternionConjugate(XMLoad(obb.Orientation));
	uvmin = XMVectorReplicate(0.5f);
	uvmax = uvmin * XMVectorReciprocal(XMLoad(obb.Extents));
	for (auto& v : m_mesh.vertices)
	{
		auto p = get_position(v) + t;
		p = XMVector3Rotate(p, q);
		p = XMVectorAndInt(p, g_XMMaskXY.v);
		p = XMVectorMultiplyAdd(p, uvmax, uvmin);
		set_uv(v, p);
	}

	m_mesh.build();
	m_previwPatch.m_opticity = 0.5f;
	m_previwPatch.setSurface(&m_mesh);
	CreateDecalDeviceResources(bb, obb);
}

void SurfaceInspectionPlanner::CreateDecalDeviceResources(DirectX::BoundingBox &bb, DirectX::BoundingOrientedBox &obb)
{
	auto pDevice = this->Scene->GetRenderDevice();
	auto pD2dContext = this->Scene->Get2DContext();

	{
		auto pModel = new MonolithModel();
		pModel->pMesh = CreateMeshBuffer(pDevice, m_mesh);
		pModel->SetName("whole_patch");
		pModel->BoundBox = bb;
		pModel->BoundOrientedBox = obb;
		//VisualObject::m_pRenderModel = pModel;

		m_decal = RenderableTexture2D(pDevice, g_DecalResolution, g_DecalResolution, DXGI_FORMAT_B8G8R8A8_UNORM, 1, 0, true);
		m_decal.CreateD2DBitmapView(pD2dContext);

		auto declMat = make_shared<PhongMaterial>();
		declMat->DiffuseMap = m_decal;
		declMat->UseAlphaDiscard = true;

		pModel->pMaterial = std::move(declMat);
		UpdateDecalGeometry(Scene->Get2DFactory());

		m_decalModel.reset(pModel);
	}
}

void SurfaceInspectionPlanner::CopyMesh(TriangleMeshType& mesh, const IModelNode* pNode)
{
	using DirectX::Scene::DefaultStaticModel;
	using DirectX::Scene::DefaultSkinningModel;
	using namespace DirectX::VertexTraits;

	auto pModel = dynamic_cast<const DefaultStaticModel*>(pNode);
	if (pModel)
	{
		auto& vertics = mesh.vertices;
		auto& indices = mesh.indices;

		TriangleMeshType::VertexType vt;
		for (auto& v : pModel->Vertices)
		{
			convert_vertex(v, vt);
			vertics.push_back(vt);
		}
		for (auto& f : pModel->Facets)
		{
			for (int i = 0; i < 3; ++i)
				indices.push_back(f[i]);
		}
	}
}

void SurfaceInspectionPlanner::TesselateBezeirPatch(int tessellation, DirectX::SimpleMath::Color &color)
{
	//auto& patch = TeapotPatches[9];
	//for (int i = 0; i < std::size(patch.indices); i++)
	//{
	//	m_patch[i] = TeapotControlPoints[patch.indices[i]].v;
	//}

	bool succ = Geometrics::triangluate(m_bezeirPatch, m_mesh, tessellation);

	for (auto& cp : m_bezeirPatch) { cp.x = -cp.x; cp.z = -cp.z; }
	succ = Geometrics::triangluate(m_bezeirPatch, m_mesh, tessellation);

	for (auto& cp : m_bezeirPatch) cp.x = -cp.x;
	succ = Geometrics::triangluate(m_bezeirPatch, m_mesh, tessellation, true);

	for (auto& cp : m_bezeirPatch) { cp.x = -cp.x; cp.z = -cp.z; }
	succ = Geometrics::triangluate(m_bezeirPatch, m_mesh, tessellation, true);

	for (auto& v : m_mesh.vertices)
	{
		DirectX::VertexTraits::set_color(v, color);
	}

	m_mesh.flip();

}

InspectionPatch * SurfaceInspectionPlanner::AddInspectionPatch(FXMVECTOR uv, int fid)
{
	m_isPatches.emplace_back();
	auto& patch = m_isPatches.back();
	patch.setSurface(&m_mesh);

	patch.SetUVRect(uv, patch.m_uvExtent);
	auto pFactory = this->Scene->Get2DFactory();
	patch.UpdateGeometry(pFactory);
	patch.CaculateCameraFrustum();
	return &patch;
}

SurfaceInspectionPlanner::SurfaceInspectionPlanner()
{
	static const size_t g_MaxPatchCount = 64;
	m_state = State_Initializing;
	m_pen = nullptr;
	m_workloadObj = nullptr;
	m_isPatches.reserve(g_MaxPatchCount);
	this->OnParentChanged += [this](SceneObject* _this, SceneObject* old_parent)
	{
		auto vo = Parent()->As<VisualObject>();
		if (vo)
		{
			this->SetWorkload(vo);
		}
	};

	CoreInputs::PrimaryKeyboard()->KeyDown += [this](const KeyboardEventArgs& arg)
	{
		if (arg.Key == VK_RETURN && this->m_state == State_Idle && this->m_isHit)
		{
			auto its = this->m_isInfo;
			auto v = this->m_mesh.persudo_vertex(its.facet, its.barycentric);
			this->AddInspectionPatch(get_uv(v), its.facet);
		}
	};
}

SurfaceInspectionPlanner::~SurfaceInspectionPlanner()
{
	m_requestCancelLoading = 1;

	//if (!m_loadingTask.is_done())
	//	m_loadingTask.wait();

	//m_decal.BitmapView()->Release();
	m_decal.Reset();
	//std::cout << "Decal destroyed" << std::endl;
	//auto pModel = static_cast<MonolithModel*>(m_pModel.release());
	//if (pModel) delete pModel;
}

bool SurfaceInspectionPlanner::IsVisible(const BoundingGeometry & viewFrustum) const
{
	return true;
}

void SurfaceInspectionPlanner::AddChild(SceneObject * child)
{
	SceneObject::AddChild(child);
	if (!m_pen)
		m_pen = child->As<TrackedPen>();
	if (child->Name == "workload")
	{
		auto pVO = child->As<VisualObject>();
		SetWorkload(pVO);
	}
}

void SurfaceInspectionPlanner::SetWorkload(VisualObject * pVO)
{
	std::cout << "[SIP] Workload set to object [" << pVO->Name << ']' << std::endl;
	m_state = State_Initializing;

	auto pModel = pVO ? pVO->RenderModel() : nullptr;
	SetWorkloadFillView(pModel, pVO);

	m_workloadObj = pVO;
	if (pModel)
	{
		m_loadingTask = concurrency::create_task([=]() {
			this->ExtractWorkloadMesh(pModel);
			m_state = State_Idle;
			m_isReady = true;
		});
	}
}

void SurfaceInspectionPlanner::SetWorkloadFillView(DirectX::Scene::IModelNode * pModel, Causality::VisualObject * pVO)
{
	auto bb = pModel->GetOrientedBoundingBox();
	SortOrientedBoundingBox(bb);
	float scl = 1.0f / bb.Extents.x;

	(Vector3&)(bb.Extents) *= scl;

	XMVECTOR tsl = XMLoad(bb.Center) * scl;
	XMVECTOR oq = XMLoad(bb.Orientation);

	XMMATRIX rotm = XMMatrixIdentity();
	XMVECTOR quat = XMQuaternionRotationPatch(-XM_PIDIV2);
	quat = XMQuaternionMultiply(XMQuaternionConjugate(XMLoad(bb.Orientation)), quat);
	tsl = XMVector3Rotate(-tsl, quat) + XMVectorSet(0, bb.Extents.z, 0, 0);

	pVO->SetPosition(tsl);
	pVO->SetOrientation(quat);
	pVO->SetScale(Vector3(scl));

	DirectX::BoundingBox bbx;
	pVO->GetBoundingBox(bbx);
}

void SurfaceInspectionPlanner::Update(time_seconds const & time_delta)
{
	using namespace DirectX::VertexTraits;
	m_isHit = false;
	if (m_isReady && m_pen)
	{
		// These position / directions are in local coordinate
		XMVECTOR pos = m_pen->GetTipPosition();
		XMVECTOR dir = m_pen->GetTipDirection();

		bool hit = HitTest(pos, dir);
		if (hit)
		{
			if (m_state == State_Idle) // Pressing Button 
			{
				auto pv = m_mesh.persudo_vertex(m_isInfo.facet, m_isInfo.barycentric);
				if (m_pen->IsInking())
				{
					auto patch = TrySelectInspectionPatch(get_uv(pv), m_isInfo.facet);
					if (patch)
						SurfacePatchDragBegin(patch);
					else
						SurfaceSketchBegin();
				}
				else
				{
					m_previwPatch.m_uvCenter = get_uv(pv);
					m_previwPatch.UpdateDecalTransformMatrix();
					m_previwPatch.CaculateCameraFrustum();
					m_declDirtyFalg = 1;
				}
			}

			switch (m_state)
			{
			case State_DrawingPatch:
				SrufaceSketchUpdate(pos, dir);
				break;
			case State_DragingPatch:
				SurfacePatchDragUpdate(pos);
				break;
			case State_Idle:
			default:
				break;
			}
		}

		if (m_state == State_DrawingPatch && (!m_pen->IsInking() || !hit))
		{
			SurfaceSketchEnd();
		}
		else if (m_state == State_DragingPatch && (!m_pen->IsInking() || !hit))
		{
			SurfacePatchDragEnd();
		}

	}

}

bool XM_CALLCONV SurfaceInspectionPlanner::HitTest(FXMVECTOR pos_world, FXMVECTOR dir_world)
{
	using namespace DirectX::VertexTraits;
	if (!m_workloadObj || !m_isReady) return false;
	XMMATRIX invworld = m_workloadObj->GetGlobalTransform().Inversed().TransformMatrix();

	m_isHit = false;
	XMVECTOR pos = XMVector3Transform(pos_world, invworld);
	XMVECTOR dir = XMVector3TransformNormal(dir_world, invworld);

	std::vector<Geometrics::MeshRayIntersectionInfo> intersecs;
	float dis;

	dir = XMVector3Normalize(dir);
	// fast rejection path
	if (!m_decalModel->GetOrientedBoundingBox().Intersects(pos, dir, dis))
		return false;
	if (!m_decalModel->GetBoundingBox().Intersects(pos, dir, dis))
		return false;

	m_mesh.intersect(pos, dir, &intersecs);
	if (!intersecs.empty())
	{
		m_isHit = true;
		m_isInfo = intersecs[0];
	}
	return !intersecs.empty();
}

void SurfaceInspectionPlanner::Render(IRenderContext * pContext, IEffect * pEffect)
{
	if (m_declDirtyFalg)
		DrawDecal(Scene->Get2DContext());

	if (m_decalModel)
	{
		auto world = m_workloadObj->GlobalTransformMatrix();
		m_decalModel->Render(pContext, world, pEffect);
		if (m_isHit || m_isPatches.size() > 0)
		{
			auto& darwer = Visualizers::g_PrimitiveDrawer;
			darwer.SetWorld(world);
			darwer.Begin();

			if (m_isHit)
				DrawPatchCamera(m_previwPatch);

			for (auto& patch : m_isPatches)
			{
				DrawPatchCamera(patch);
			}

			darwer.End();
		}
	}

	if (m_pen)
	{
		RenderCursor(pContext, pEffect);
	}

	if (g_DebugView && pEffect)
	{
		DrawBezeirPatchControlPoints();
	}
}

void SurfaceInspectionPlanner::DrawPatchCamera(InspectionPatch& patch)
{
	BoundingGeometry geo(patch.BoundingBox);
	XMVECTOR color = patch.Valiad ? Colors::Lime.v : Colors::Red.v;
	DrawGeometryOutline(geo, color);
	geo = patch.m_cameraFrustum;
	DrawGeometryOutline(geo, color);

}

RenderFlags SurfaceInspectionPlanner::GetRenderFlags() const { return RenderFlags::SpecialEffects; }

void XM_CALLCONV SurfaceInspectionPlanner::UpdateViewMatrix(FXMMATRIX view, CXMMATRIX projection) {}

void SurfaceInspectionPlanner::DrawBezeirPatchControlPoints()
{
	auto& drawer = DirectX::Visualizers::g_PrimitiveDrawer;
	auto world = this->GlobalTransformMatrix();
	//geo.Transform(geo, GlobalTransformMatrix());
	Color color = DirectX::Colors::LimeGreen.v;

	drawer.SetWorld(world);

	if (m_decalModel && g_VisualizeNormal)
	{
		drawer.Begin();
		using namespace DirectX::VertexTraits;
		for (auto& v : m_mesh.vertices)
		{
			XMVECTOR pv = get_position(v);
			XMVECTOR pnv = get_normal(v);
			pnv = pv + 0.01 * pnv;
			drawer.DrawLine(pv, pnv, DirectX::Colors::Red.v);

			pnv = get_tangent(v);
			pnv = pv + 0.01 * pnv;
			drawer.DrawLine(pv, pnv, color);
		}

		drawer.End();
	}

	for (int i = 0; i < 4; i++)
	{
		drawer.DrawSphere(m_bezeirPatch.control_point(i, 0), g_ControlPointsRaius, color);
		drawer.DrawSphere(m_bezeirPatch.control_point(i, 3), g_ControlPointsRaius, color);
	}

	for (int i = 1; i < 3; i++)
	{
		drawer.DrawSphere(m_bezeirPatch.control_point(0, i), g_ControlPointsRaius, color);
		drawer.DrawSphere(m_bezeirPatch.control_point(3, i), g_ControlPointsRaius, color);
	}

	for (int i = 0; i < 3; i++)
	{
		drawer.DrawCylinder(m_bezeirPatch.control_point(i, 0), m_bezeirPatch.control_point((i + 1) % 4, 0), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(i, 3), m_bezeirPatch.control_point((i + 1) % 4, 3), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(0, i), m_bezeirPatch.control_point(0, (i + 1) % 4), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(3, i), m_bezeirPatch.control_point(3, (i + 1) % 4), g_ControlPointsConnectionRadius, color);
	}

	color *= 1.2;
	color.A(1.0f);
	for (int i = 1; i < 3; i++)
	{
		drawer.DrawCylinder(m_bezeirPatch.control_point(i, 0), m_bezeirPatch.control_point(i, 1), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(i, 1), m_bezeirPatch.control_point(i, 2), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(i, 2), m_bezeirPatch.control_point(i, 3), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(0, i), m_bezeirPatch.control_point(1, i), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(1, i), m_bezeirPatch.control_point(2, i), g_ControlPointsConnectionRadius, color);
		drawer.DrawCylinder(m_bezeirPatch.control_point(2, i), m_bezeirPatch.control_point(3, i), g_ControlPointsConnectionRadius, color);
	}
}

void SurfaceInspectionPlanner::RenderCursor(IRenderContext * pContext, IEffect * pEffect)
{
	auto& drawer = DirectX::Visualizers::g_PrimitiveDrawer;

	XMVECTOR pos = m_pen->GetTipPosition();
	XMVECTOR dir = m_pen->GetTipDirection();

	//std::cout << "dir = " << Vector3(dir) << std::endl;
	//std::cout << "orientation = " << m_pen->GetOrientation() << std::endl;

	XMVECTOR color = Colors::Yellow.v;

	if (m_state == State_DrawingPatch)
		color = Colors::LimeGreen.v;
	else if (m_state == State_DragingPatch)
		color = Colors::Red.v;
	else if (m_state == State_Initializing)
		color = Colors::Purple.v;

	float alpha = 0.5f, scl = 1.0f;
	XMMATRIX world = XMMatrixIdentity();

	if (m_isHit)
	{
		world = m_workloadObj->GlobalTransformMatrix();
		alpha = 1.0f;
		//color = Colors::Red;
		scl /= 20.0f;
		pos = m_isInfo.position;
	}

	float length = 0.01f / scl;
	float radius = 0.0035f / scl;

	m_cursorMaterial->DiffuseColor = color;
	m_cursorMaterial->AmbientColor = color;
	m_cursorMaterial->Alpha = alpha;
	//m_cursorMaterial->SetupEffect(pEffect);

	color = XMVectorSetW(color, alpha);

	drawer.SetWorld(world);
	drawer.DrawSphere(pos, radius, color);
	//drawer.DrawCylinder(pos, pos + dir * length, radius * 0.5, color);
}

inline D2D1_COLOR_F XM_CALLCONV GetD2DColor(const Color& color)
{
	D2D1_COLOR_F cf = reinterpret_cast<const D2D1_COLOR_F&>(color);
	return cf;
}

inline D2D1_POINT_2F XM_CALLCONV GetD2DPoint(const Vector2& v)
{
	D2D1_POINT_2F dv = reinterpret_cast<const D2D1_POINT_2F&>(v);
	return dv;
}



void SurfaceInspectionPlanner::UpdateDecalGeometry(I2DFactory* pFactory)
{
	m_previwPatch.SetUVRect(Vector2(0.5f), m_previwPatch.m_uvExtent);
	m_previwPatch.UpdateGeometry(pFactory);
	m_previwPatch.UpdateDecalTransformMatrix();
	m_declDirtyFalg = 1;
}

InspectionPatch * SurfaceInspectionPlanner::TrySelectInspectionPatch(FXMVECTOR uv, int fid)
{
	for (auto& patch : m_isPatches)
	{
		if (patch.uvBoundry().contains2D(uv))
			return &patch;
	}
	return nullptr;
}

void SurfaceInspectionPlanner::DrawDecal(I2DContext *pContext)
{
	D2D1_COLOR_F color;
	color = { .2f,0.7f,.2f,1.0f };

	if (!m_brush)
		ThrowIfFailed(pContext->CreateSolidColorBrush(color, &m_brush));

	//color = { .0f,.0f,.0f,.0f };
	pContext->SetTarget(m_decal);
	pContext->BeginDraw();
	pContext->Clear(GetD2DColor(m_decalBackground));

	m_previwPatch.DrawDecal(pContext, m_brush.Get());
	for (auto& patch : m_isPatches)
		patch.DrawDecal(pContext, m_brush.Get());

	ThrowIfFailed(pContext->EndDraw());
	pContext->SetTarget(nullptr);

	m_declDirtyFalg = 0;
}

void SurfaceInspectionPlanner::SurfaceSketchBegin()
{
	if (!m_mesh.empty())
	{
		m_isPatches.emplace_back();
		m_isPatches.back().setSurface(&m_mesh);
		m_state = State_DrawingPatch;
	}
}

void SurfaceInspectionPlanner::SrufaceSketchUpdate(XMVECTOR pos, XMVECTOR dir)
{
	static float g_contactThred = 0.05f;
	if (m_mesh.empty()) return;

	bool touching = false;
	// Find closest point on mesh using pen direction
	vector<Geometrics::MeshRayIntersectionInfo> interInfos;
	pos -= dir * TrackedPen::TipLength * 0.5f / Parent()->GetScale().x;
	m_mesh.intersect(pos, dir, &interInfos);

	if (interInfos.size() == 0) {
		std::cout << "Pen not touching; no intersections" << std::endl;
		return;
	}

	auto& info = interInfos[0];

	float shortestDist = info.distance;

	// this is cm
	if (shortestDist < g_contactThred) {
		// touching
		auto & patch = m_isPatches.back();
		//auto& curve = patch.boundry();
		patch.append(info.position, info.facet);

		auto& uvCurve = patch.uvBoundry();
		if (uvCurve.size() > 2)
		{
			auto uvs = patch.uvBoundry().sample(std::min((int)uvCurve.size(), 100));
			//UpdateRenderGeometry(uvs, Vector2(g_DecalResolution));
		}
	}
}

void SurfaceInspectionPlanner::SurfaceSketchEnd()
{
	m_state = State_Idle;
	auto& patch = m_isPatches.back();
	auto& curve = patch.boundry();
	if (curve.empty())
	{
		m_isPatches.pop_back();
	}

	patch.closeLoop();

	//curve.append(curve[0]);
	//auto curveMap = Eigen::Matrix<float, -1, 4, Eigen::RowMajor>::MapAligned((float*)curve.data(), curve.size(), 4);
	//Eigen::laplacianSmooth(curveMap, 0.8);
	//curve.updateLength();

	//if (curve.size() > 100)
	//{
	//	curve.resample(100);
	//}
	//curve.smooth(0.8f, 4);
}

void SurfaceInspectionPlanner::SurfacePatchDragBegin(InspectionPatch *patch)
{
	if (m_isPatches.empty())
		return;
	m_state = State_DragingPatch;
	m_activePatch = patch;
}

void SurfaceInspectionPlanner::SurfacePatchDragUpdate(FXMVECTOR pos)
{
	m_activePatch;
}

void SurfaceInspectionPlanner::SurfacePatchDragEnd()
{
	m_state = State_Idle;
	m_activePatch = nullptr;
}

InspectionPatch::InspectionPatch()
{
	Valiad = false;
	m_uvRotation = .0f;
	m_opticity = 1.0f;

	m_defaultCameraFrustum.Far = 8.0f;
	m_defaultCameraFrustum.Near = 1.0f;
	// it is actually half fov
	float hfov = tanf(XMConvertToRadians(10));
	float vfov = tanf(XMConvertToRadians(7.5));
	m_defaultCameraFrustum.RightSlope = hfov;
	m_defaultCameraFrustum.LeftSlope = -hfov;
	m_defaultCameraFrustum.TopSlope = vfov;
	m_defaultCameraFrustum.BottomSlope = -vfov;

	m_decalTransform = D2D1::Matrix3x2F::Identity();

	DecalSize = Vector2(g_DecalResolution);
	// 1/(physical dimension) * sqrt(2), so that the uv rect scales up and misalign with 45 degree  will still contains in
	float phydim = 20.0f;
	float minfov = std::fminf(hfov, vfov);
	ZTolerance = 0.14f;
	m_uvExtent = 1 / (phydim * sqrtf(2)) * m_defaultCameraFrustum.Far * Vector2(minfov, minfov);
}

bool InspectionPatch::CaculateCameraFrustum()
{
	using namespace DirectX::VertexTraits;
	std::vector<XMVECTOR, XMAllocator> positions;
	std::vector<XMVECTOR, XMAllocator> normals;

	CaculateVerticsInPatch(positions);

	if (m_areVertices.size() == 0)
	{
		Valiad = false;
		return false;
	}

	CaculatePatchCurvetures();

	DirectX::CreateBoundingOrientedBoxFromPoints(BoundingBox, positions.size(), (XMFLOAT3*)positions.data(), sizeof(XMVECTOR));

	CaculatePrinciplePatchOrientation();

	SortOrientedBoundingBox(BoundingBox);

	Valiad = BoundingBox.Extents.z * 2.0f < ZTolerance;
	XMVECTOR q = XMLoad(BoundingBox.Orientation);
	XMVECTOR opticsAxis = XMVector3Rotate(g_XMIdentityR2.v, q);

	bool flipZ = XMVectorGetY(opticsAxis) > .0f;
	if (flipZ)
	{
		opticsAxis = -opticsAxis;
		q = XMQuaternionMultiply(XMQuaternionRotationYaw(XM_PI), q);
	}

	XMVECTOR Far = BoundingBox.Center + opticsAxis * BoundingBox.Extents.z;
	XMVECTOR Origin = Far - opticsAxis * m_defaultCameraFrustum.Far;

	m_defaultCameraFrustum.Transform(m_cameraFrustum, XMMatrixRigidTransform(q, Origin));
	return Valiad;
}

void InspectionPatch::CaculateVerticsInPatch(XMVECTOR_ARRAY&positions)
{
	positions.clear();
	m_areVertices.clear();
	XMVECTOR dsize = DecalSize;
	XMVECTOR ext = m_uvExtent;
	XMVECTOR uvmin = m_uvCenter - m_uvExtent * 1.2f;
	XMVECTOR uvmax = m_uvCenter + m_uvExtent * 1.2f;
	auto& vertices = this->m_surface->vertices;

	m_isVertexIn.resize(vertices.size());
	std::fill(m_isVertexIn.begin(), m_isVertexIn.end(), 0);
	//ZeroMemory(vertices.data(), vertices.size() * sizeof(int_fast16_t));
	XMVECTOR avNorm = XMVectorZero();
	for (int i = 0; i < vertices.size(); i++)
	{
		auto& v = vertices[i];
		XMVECTOR uv = get_uv(v);
		BOOL inside;
		// fast rejection
		inside = XMVector2Greater(uv, uvmin) && XMVector2Less(uv, uvmax);
		if (inside)
		{
			//m_deaclGeometry->FillContainsPoint(GetD2DPoint(uv * dsize), m_decalTransform, &inside);
			if (inside)
			{
				m_isVertexIn[i] = 1;
				m_areVertices.push_back(i);
				positions.push_back(get_position(v));
				XMVECTOR norm = get_normal(v);
				//normals.push_back(norm);
				avNorm += norm;
			}
		}
	}
	m_averageNormal = avNorm;

}

void InspectionPatch::CaculatePatchCurvetures()
{
	auto& vertices = m_surface->vertices;
	m_curvetures.clear();
	for (auto& facet : m_surface->facets())
	{
		for (int e = 0; e < 3; e++)
		{
			int iv0 = facet[e];
			int iv1 = facet[(e + 1) % 3];

			auto v0_in = m_isVertexIn[iv0];
			auto v1_in = m_isVertexIn[iv1];
			if (v0_in || v1_in)
			{
				auto v0 = vertices[iv0];
				auto v1 = vertices[iv1];

				XMVECTOR p0 = get_position(v0);
				XMVECTOR n0 = get_normal(v0);
				XMVECTOR p1 = get_position(v1);
				XMVECTOR n1 = get_normal(v1);

				// project p0,p1,n0,n1 into intrinsic coordinate, ignores the bi-normal direction
				XMMATRIX rot;
				XMVECTOR axisX = _DXMEXT XMVector3Normalize(p1 - p0);
				XMVECTOR axisZ = XMVector3Cross(n0, n1); axisZ = _DXMEXT XMVector3Normalize(axisZ);
				XMVECTOR axisY = XMVector3Cross(axisZ, axisX);
				axisZ = XMVector3Cross(axisX, axisY); // idely, the normal should be perpendicular to the edge, but it may not be true in the real world case
				rot.r[0] = axisX; rot.r[1] = axisY; rot.r[2] = axisZ; rot.r[3] = g_XMIdentityR3;
				p0 = XMVector3TransformNormal(p0, rot);
				p1 = XMVector3TransformNormal(p1, rot);
				n0 = XMVector3TransformNormal(n0, rot);
				n1 = XMVector3TransformNormal(n1, rot);

				// No do the intersection in 2D
				XMVECTOR ip = XMVector2IntersectLine(p0, p0 + n0, p1, p1 + n1);
				if (XMVector4IsNaN(ip))
					ip = XMVectorZero();
				else
				{
					XMVECTOR c0 = ip + ip - p0 - p1;
					XMVECTOR c = XMVector3Length(c0);
					if (XMVector4Less(XMVector3Dot(c0, n0), XMVectorZero()))
						c = -c;

					ip = XMVectorReciprocal(ip);
				}

				XMVECTOR uv0, uv1;
				//uv0 = get_uv(v0);
				//uv1 = get_uv(v1);
				//uv1 = uv1 - uv0;

				uv1 = p1 - p0;
				//uv1 = XMVector3Rotate(uv1, XMQuaternionConjugate(patchOrientation));

				//uv0 = XMVectorSplatY(uv1);
				//uv1 = XMVectorSplatX(uv1);
				//XMVECTOR phi = XMVectorATanEst(uv0 / uv1);
				ip = _DXMEXT XMVectorSelect<0, 0, 0, 1>(uv1, ip);
				m_curvetures.emplace_back(ip); // X = phi, Y = curveture

				if (v0_in ^ v1_in) // crossing edge
				{
					// m_uvCurve.intersect2D(uv0, uv1 - uv0);
					// add a new persudo vertex in
				}

			}
		}
	}
}

void XM_CALLCONV InspectionPatch::CaculatePrinciplePatchOrientation()
{
	XMVECTOR orientation = XMLoad(BoundingBox.Orientation);
	XMVECTOR projQuat = XMQuaternionConjugate(orientation);

	auto proj = [projQuat](FXMVECTOR v) -> float
	{
		XMVECTOR V = XMVector3Rotate(v, projQuat);
		return atanf(XMVectorGetX(V) / XMVectorGetY(V));
	};

	std::vector<Vector2> cuv_phi(m_curvetures.size());
	for (int i = 0; i < m_curvetures.size(); i++)
	{
		cuv_phi[i].x = proj(m_curvetures[i]);
		cuv_phi[i].y = m_curvetures[i].w;
	}

	// sort the directional curvetures by its UV rotation angle PHI
	std::sort(cuv_phi.begin(), cuv_phi.end(), [](const Vector2& c0, const Vector2& c1) -> bool {
		return c0.x < c1.x;
	});

	Geometrics::laplacianSmooth<Vector2>(cuv_phi, 0.8f, 4, true);
	auto minidx = std::min_element(cuv_phi.begin(), cuv_phi.end(), [](const Vector2& c0, const Vector2& c1) -> bool {
		return fabs(c0.y) < fabs(c1.y);
	}) - cuv_phi.begin();

	m_principleUvRotation = proj(m_curvetures[minidx]);

	// (Quaternion&)BoundingBox.Orientation = XMQuaternionMultiply(XMQuaternionRotationRoll(m_principleUvRotation), orientation);
}

void XM_CALLCONV InspectionPatch::SetUVRect(FXMVECTOR uvc, FXMVECTOR uvext)
{
	m_uvCurve.clear();
	m_uvCenter = uvc;
	m_uvExtent = uvext;
	m_uvRotation = .0f;

	XMVECTOR onx = g_XMOne.v;
	XMVECTOR flx = g_XMNegateY.v;

	m_uvCurve.push_back(-onx); // (-1,-1)
	m_uvCurve.push_back(+flx); // ( 1,-1)
	m_uvCurve.push_back(+onx); // ( 1, 1)
	m_uvCurve.push_back(-flx); // (-1, 1)

	UpdateDecalTransformMatrix();
	m_dirty = 3;
}

void InspectionPatch::UpdateGeometry(I2DFactory* pFactory, bool smooth)
{
	auto& points = this->m_uvCurve.anchors();
	auto smoothed = m_uvCurve.sample(100);

	if (points.size() <= 1)
		return;

	pFactory->CreatePathGeometry(&m_deaclGeometry);
	cptr<ID2D1GeometrySink> pSink;
	ThrowIfFailed(m_deaclGeometry->Open(&pSink));
	pSink->SetFillMode(D2D1_FILL_MODE_WINDING);

	int n = smooth ? smoothed.size() : points.size();
	vector<D2D1_POINT_2F> dpoints(n);
	if (smooth)
	{
		for (int i = 0; i < n; i++)
		{
			Vector2 p(smoothed[i].x, smoothed[i].y);
			dpoints[i] = GetD2DPoint(p);
		}
	}
	else
	{
		for (int i = 0; i < n; i++)
		{
			Vector2 p(points[i].x, points[i].y);
			dpoints[i] = GetD2DPoint(p);
		}
	}

	pSink->BeginFigure(
		dpoints[0],
		D2D1_FIGURE_BEGIN_FILLED
	);

	pSink->AddLines(&dpoints[1], n - 1);
	pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
	pSink->Close();
	pSink.Reset();

	m_dirty = 1;
}

void InspectionPatch::DrawDecal(I2DContext * pContext, ID2D1SolidColorBrush * pBrush)
{
	Color color = Colors::Red.v;
	if (Valiad)
		color = Colors::LimeGreen.v;
	color.w = m_opticity;

	UpdateDecalTransformMatrix();

	// S
	auto trans = m_decalTransform * D2D1::Matrix3x2F::Scale(DecalSize.x, DecalSize.y);

	pContext->SetTransform(trans);

	pBrush->SetColor(GetD2DColor(color));
	pContext->FillGeometry(m_deaclGeometry.Get(), pBrush);

	color = Colors::Black.v;
	pBrush->SetColor(GetD2DColor(color));
	pContext->DrawGeometry(m_deaclGeometry.Get(), pBrush, 2.0f / DecalSize.Length());
}

void InspectionPatch::UpdateDecalTransformMatrix()
{
	// S
	m_decalTransform = D2D1::Matrix3x2F::Scale(m_uvExtent.x, m_uvExtent.y);
	// R
	m_decalTransform = m_decalTransform * D2D1::Matrix3x2F::Rotation(m_uvRotation);
	// T
	m_decalTransform = m_decalTransform * D2D1::Matrix3x2F::Translation(m_uvCenter.x, m_uvCenter.y);
}
