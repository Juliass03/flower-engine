#include "render_prepare.h"
#include "mesh.h"
#include "material.h"
#include "renderer.h"
#include "frame_data.h"
#include <execution>

namespace engine{

static AutoCVarInt32 cVarCulling(
	"r.Culling.Enable",
	"Enable culling. 0 is off, other is on.",
	"Culling",
	1,
	CVarFlags::ReadAndWrite
);

static AutoCVarInt32 cVarParallelCulling(
	"r.Culling.Parallel",
	"Enable parallel culling. 0 is off, 1 is multi-thread culling, 2 is multi-thread simd culling.",
	"Culling",
	0,
	CVarFlags::ReadAndWrite
);

}

using namespace engine;

// Reference: https://bruop.github.io/improved_frustum_culling/
// See: https://gist.github.com/BruOp/60e862049ac6409d2fd4ec6fa5806b30

struct AABB
{
    glm::vec3 min;
    glm::vec3 max;
};

struct OBB
{
    glm::vec3 center = {};
    glm::vec3 extents = {};
    // Orthonormal basis
    union
    {
        glm::vec3 axes[3] = {};
    };
};

struct CullingFrustum
{
	float near_right;
	float near_top;
	float near_plane;
	float far_plane;
};

bool test_using_separating_axis_theorem(const CullingFrustum& frustum, const glm::mat4& vs_transform, const AABB& aabb)
{
    using namespace glm;

    // Near, far
    float z_near = frustum.near_plane;
    float z_far = frustum.far_plane;

    // half width, half height
    float x_near = frustum.near_right;
    float y_near = frustum.near_top;

    // So first thing we need to do is obtain the normal directions of our OBB by transforming 4 of our AABB vertices
    vec3 corners[] = {
        {aabb.min.x, aabb.min.y, aabb.min.z},
        {aabb.max.x, aabb.min.y, aabb.min.z},
        {aabb.min.x, aabb.max.y, aabb.min.z},
        {aabb.min.x, aabb.min.y, aabb.max.z},
    };

    // Transform corners
    // This only translates to our OBB if our transform is affine
    for (size_t corner_idx = 0; corner_idx < std::size(corners); corner_idx++) {
        corners[corner_idx] = (vs_transform * glm::vec4(corners[corner_idx],1.0f));
    }

    OBB obb;
    obb.axes[0] = corners[1] - corners[0];
    obb.axes[1] = corners[2] - corners[0];
    obb.axes[2] = corners[3] - corners[0];

    obb.center = corners[0] + 0.5f * (obb.axes[0] + obb.axes[1] + obb.axes[2]);
    obb.extents = vec3{ length(obb.axes[0]), length(obb.axes[1]), length(obb.axes[2]) };
    obb.axes[0] = obb.axes[0] / obb.extents.x;
    obb.axes[1] = obb.axes[1] / obb.extents.y;
    obb.axes[2] = obb.axes[2] / obb.extents.z;
    obb.extents *= 0.5f;

    {
        vec3 M = { 0, 0, 1 };
        float MoX = 0.0f;
        float MoY = 0.0f;
        float MoZ = 1.0f;

        // Projected center of our OBB
        float MoC = obb.center.z;
        // Projected size of OBB
        float radius = 0.0f;
        for (uint32 i = 0; i < 3; i++) {
            // dot(M, axes[i]) == axes[i].z;
            radius += fabsf(obb.axes[i].z) * obb.extents[i];
        }
        float obb_min = MoC - radius;
        float obb_max = MoC + radius;

        float tau_0 = z_far; // Since z is negative, far is smaller than near
        float tau_1 = z_near;

        if (obb_min > tau_1 || obb_max < tau_0) {
            return false;
        }
    }

    {
        const vec3 M[] = {
            { z_near, 0.0f, x_near }, // Left Plane
            { -z_near, 0.0f, x_near }, // Right plane
            { 0.0, -z_near, y_near }, // Top plane
            { 0.0, z_near, y_near }, // Bottom plane
        };
        for (uint32 m = 0; m < std::size(M); m++) {
            float MoX = fabsf(M[m].x);
            float MoY = fabsf(M[m].y);
            float MoZ = M[m].z;
            float MoC = dot(M[m], obb.center);

            float obb_radius = 0.0f;
            for (uint32 i = 0; i < 3; i++) {
                obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
            }
            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            float p = x_near * MoX + y_near * MoY;

            float tau_0 = z_near * MoZ - p;
            float tau_1 = z_near * MoZ + p;

            if (tau_0 < 0.0f) {
                tau_0 *= z_far / z_near;
            }
            if (tau_1 > 0.0f) {
                tau_1 *= z_far / z_near;
            }

            if (obb_min > tau_1 || obb_max < tau_0) {
                return false;
            }
        }
    }

    // OBB Axes
    {
        for (uint32 m = 0; m < std::size(obb.axes); m++) {
            const vec3& M = obb.axes[m];
            float MoX = fabsf(M.x);
            float MoY = fabsf(M.y);
            float MoZ = M.z;
            float MoC = dot(M, obb.center);

            float obb_radius = obb.extents[m];

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = x_near * MoX + y_near * MoY;
            float tau_0 = z_near * MoZ - p;
            float tau_1 = z_near * MoZ + p;
            if (tau_0 < 0.0f) {
                tau_0 *= z_far / z_near;
            }
            if (tau_1 > 0.0f) {
                tau_1 *= z_far / z_near;
            }

            if (obb_min > tau_1 || obb_max < tau_0) {
                return false;
            }
        }
    }

    // Now let's perform each of the cross products between the edges
    // First R x A_i
    {
        for (uint32 m = 0; m < std::size(obb.axes); m++) {
            const vec3 M = { 0.0f, -obb.axes[m].z, obb.axes[m].y };
            float MoX = 0.0f;
            float MoY = fabsf(M.y);
            float MoZ = M.z;
            float MoC = M.y * obb.center.y + M.z * obb.center.z;

            float obb_radius = 0.0f;
            for (uint32 i = 0; i < 3; i++) {
                obb_radius += fabsf(dot(M, obb.axes[i])) * obb.extents[i];
            }

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = x_near * MoX + y_near * MoY;
            float tau_0 = z_near * MoZ - p;
            float tau_1 = z_near * MoZ + p;
            if (tau_0 < 0.0f) {
                tau_0 *= z_far / z_near;
            }
            if (tau_1 > 0.0f) {
                tau_1 *= z_far / z_near;
            }

            if (obb_min > tau_1 || obb_max < tau_0) {
                return false;
            }
        }
    }

    // U x A_i
    {
        for (uint32 m = 0; m < std::size(obb.axes); m++) {
            const vec3 M = { obb.axes[m].z, 0.0f, -obb.axes[m].x };
            float MoX = fabsf(M.x);
            float MoY = 0.0f;
            float MoZ = M.z;
            float MoC = M.x * obb.center.x + M.z * obb.center.z;

            float obb_radius = 0.0f;
            for (uint32 i = 0; i < 3; i++) {
                obb_radius += fabsf(dot(M, obb.axes[i])) * obb.extents[i];
            }

            float obb_min = MoC - obb_radius;
            float obb_max = MoC + obb_radius;

            // Frustum projection
            float p = x_near * MoX + y_near * MoY;
            float tau_0 = z_near * MoZ - p;
            float tau_1 = z_near * MoZ + p;
            if (tau_0 < 0.0f) {
                tau_0 *= z_far / z_near;
            }
            if (tau_1 > 0.0f) {
                tau_1 *= z_far / z_near;
            }

            if (obb_min > tau_1 || obb_max < tau_0) {
                return false;
            }
        }
    }

    // Frustum Edges X Ai
    {
        for (uint32 obb_edge_idx = 0; obb_edge_idx < std::size(obb.axes); obb_edge_idx++) {
            const vec3 M[] = {
                cross({-x_near, 0.0f, z_near}, obb.axes[obb_edge_idx]), // Left Plane
                cross({ x_near, 0.0f, z_near }, obb.axes[obb_edge_idx]), // Right plane
                cross({ 0.0f, y_near, z_near }, obb.axes[obb_edge_idx]), // Top plane
                cross({ 0.0, -y_near, z_near }, obb.axes[obb_edge_idx]) // Bottom plane
            };

            for (uint32 m = 0; m < std::size(M); m++) {
                float MoX = fabsf(M[m].x);
                float MoY = fabsf(M[m].y);
                float MoZ = M[m].z;

                constexpr float epsilon = 1e-4f;
                if (MoX < epsilon && MoY < epsilon && fabsf(MoZ) < epsilon) continue;

                float MoC = dot(M[m], obb.center);

                float obb_radius = 0.0f;
                for (uint32 i = 0; i < 3; i++) {
                    obb_radius += fabsf(dot(M[m], obb.axes[i])) * obb.extents[i];
                }

                float obb_min = MoC - obb_radius;
                float obb_max = MoC + obb_radius;

                // Frustum projection
                float p = x_near * MoX + y_near * MoY;
                float tau_0 = z_near * MoZ - p;
                float tau_1 = z_near * MoZ + p;
                if (tau_0 < 0.0f) {
                    tau_0 *= z_far / z_near;
                }
                if (tau_1 > 0.0f) {
                    tau_1 *= z_far / z_near;
                }

                if (obb_min > tau_1 || obb_max < tau_0) {
                    return false;
                }
            }
        }
    }

    // No intersections detected
    return true;
};


static void singleThreadCulling(std::vector<RenderMesh>& inMeshes,const GPUFrameData& inView)
{
    float tan_fov = tan(0.5f * inView.cameraInfo.x);
    CullingFrustum frustum;
    frustum.near_right = inView.cameraInfo.y * inView.cameraInfo.z * tan_fov;
    frustum.near_top = inView.cameraInfo.z * tan_fov;
    frustum.near_plane = -inView.cameraInfo.z;
    frustum.far_plane = -inView.cameraInfo.w;

	for(auto& mesh : inMeshes)
	{
		bool bMeshVisible = false;
		const glm::mat4& MV = inView.camView * mesh.modelMatrix;

		for(auto& subMesh : mesh.submesh)
		{
            AABB aabb;
            aabb.max = subMesh.renderBounds.origin + subMesh.renderBounds.extents;
            aabb.min = subMesh.renderBounds.origin - subMesh.renderBounds.extents;

			bool bSubMeshVisible = test_using_separating_axis_theorem(frustum,MV,aabb);
            subMesh.bCullingResult = bSubMeshVisible;
			bMeshVisible |= bSubMeshVisible;
		}

		mesh.bMeshVisible = bMeshVisible;
	}

}

static void mutliThreadCulling(std::vector<RenderMesh>& inMeshes,const GPUFrameData& inView)
{
    float tan_fov = tan(0.5f * inView.cameraInfo.x);
    CullingFrustum frustum;
    frustum.near_right = inView.cameraInfo.y * inView.cameraInfo.z * tan_fov;
    frustum.near_top = inView.cameraInfo.z * tan_fov;
    frustum.near_plane = -inView.cameraInfo.z;
    frustum.far_plane = -inView.cameraInfo.w;

    std::for_each(std::execution::par_unseq,
        inMeshes.begin(),
        inMeshes.end(),
        [inView,frustum](auto&& mesh)
    {
        mesh.bMeshVisible = false;
        const glm::mat4& MV = inView.camView * mesh.modelMatrix;

        std::for_each(std::execution::par_unseq,
            mesh.submesh.begin(),
            mesh.submesh.end(),
            [inView,frustum,MV,&mesh](auto&& subMesh)
        {
            AABB aabb;
            aabb.max = subMesh.renderBounds.origin + subMesh.renderBounds.extents;
            aabb.min = subMesh.renderBounds.origin - subMesh.renderBounds.extents;

            bool bSubMeshVisible = test_using_separating_axis_theorem(frustum,MV,aabb);
            subMesh.bCullingResult = bSubMeshVisible;
            mesh.bMeshVisible |= bSubMeshVisible;
        });
    });
}

void engine::frustumCulling(std::vector<RenderMesh>& inMeshes,const GPUFrameData& inView)
{
	if(cVarCulling.get() == 0) return;

	if(cVarParallelCulling.get() == 0) return singleThreadCulling(inMeshes,inView);
    else return mutliThreadCulling(inMeshes,inView);
}