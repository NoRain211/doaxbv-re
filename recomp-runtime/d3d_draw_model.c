#include "d3d_draw_model.h"

#include <string.h>

enum {
    /* Fixed-function FVF component bits this seam decodes. */
    RECOMP_D3D_FVF_POSITION_MASK = 0x00fu,
    RECOMP_D3D_FVF_XYZ = 0x002u,
    RECOMP_D3D_FVF_NORMAL = 0x010u,
    RECOMP_D3D_FVF_DIFFUSE = 0x040u,
    RECOMP_D3D_FVF_SPECULAR = 0x080u,
    RECOMP_D3D_FVF_TEXCOUNT_SHIFT = 8u,
    RECOMP_D3D_FVF_TEXCOUNT_MASK = 0x00fu,
};

void recomp_d3d_draw_reset(RecompD3dDrawState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof *state);
    }
}

uint32_t recomp_d3d_fvf_stride(uint32_t fvf)
{
    uint32_t texture_count =
        (fvf >> RECOMP_D3D_FVF_TEXCOUNT_SHIFT) & RECOMP_D3D_FVF_TEXCOUNT_MASK;
    uint32_t size;

    /* Only untransformed positions are handled; XYZRHW and blend weights
       would change the vertex layout and the shader with it. */
    if ((fvf & RECOMP_D3D_FVF_POSITION_MASK) != RECOMP_D3D_FVF_XYZ) {
        return 0u;
    }
    if (texture_count > 4u) {
        return 0u;
    }

    size = 12u;
    if ((fvf & RECOMP_D3D_FVF_NORMAL) != 0u) {
        size += 12u;
    }
    if ((fvf & RECOMP_D3D_FVF_DIFFUSE) != 0u) {
        size += 4u;
    }
    if ((fvf & RECOMP_D3D_FVF_SPECULAR) != 0u) {
        size += 4u;
    }
    return size + texture_count * 8u;
}

bool recomp_d3d_fvf_layout(uint32_t fvf, RecompD3dVertexLayout *layout)
{
    RecompD3dVertexLayout decoded;
    uint32_t offset;

    if (layout == NULL || recomp_d3d_fvf_stride(fvf) == 0u) {
        return false;
    }

    decoded.texcoord_count =
        (fvf >> RECOMP_D3D_FVF_TEXCOUNT_SHIFT) & RECOMP_D3D_FVF_TEXCOUNT_MASK;
    decoded.position_offset = 0u;
    offset = 12u;
    if ((fvf & RECOMP_D3D_FVF_NORMAL) != 0u) {
        decoded.normal_offset = offset;
        offset += 12u;
    } else {
        decoded.normal_offset = RECOMP_D3D_FVF_ABSENT;
    }
    if ((fvf & RECOMP_D3D_FVF_DIFFUSE) != 0u) {
        decoded.diffuse_offset = offset;
        offset += 4u;
    } else {
        decoded.diffuse_offset = RECOMP_D3D_FVF_ABSENT;
    }
    if ((fvf & RECOMP_D3D_FVF_SPECULAR) != 0u) {
        decoded.specular_offset = offset;
        offset += 4u;
    } else {
        decoded.specular_offset = RECOMP_D3D_FVF_ABSENT;
    }
    decoded.texcoord_offset = decoded.texcoord_count != 0u
        ? offset
        : RECOMP_D3D_FVF_ABSENT;
    decoded.stride = offset + decoded.texcoord_count * 8u;

    *layout = decoded;
    return true;
}

uint32_t recomp_d3d_draw_vertex_bytes(uint32_t stride, uint32_t max_index)
{
    uint64_t bytes;

    if (stride == 0u) {
        return 0u;
    }
    bytes = (uint64_t)stride * ((uint64_t)max_index + 1u);
    return bytes > 0xffffffffu ? 0u : (uint32_t)bytes;
}

uint32_t recomp_d3d_draw_triangle_count(
    uint32_t primitive_type,
    uint32_t index_count)
{
    switch (primitive_type) {
    case RECOMP_D3D_PT_TRIANGLELIST:
        return index_count / 3u;
    case RECOMP_D3D_PT_TRIANGLESTRIP:
    case RECOMP_D3D_PT_TRIANGLEFAN:
        return index_count < 3u ? 0u : index_count - 2u;
    default:
        return 0u;
    }
}

RecompD3dDrawResult recomp_d3d_draw_indexed(
    const RecompD3dDrawState *state,
    uint32_t primitive_type,
    uint32_t index_count,
    uint32_t index_data)
{
    RecompD3dDrawResult result = {0};
    uint32_t triangles;

    if (state == NULL || index_data == 0u) {
        result.error = RECOMP_D3D_DRAW_INVALID_ARGUMENT;
        return result;
    }
    if (state->stream0.vertex_data == 0u || state->stream0.stride == 0u) {
        result.error = RECOMP_D3D_DRAW_NO_STREAM;
        return result;
    }
    if (primitive_type != RECOMP_D3D_PT_TRIANGLELIST &&
        primitive_type != RECOMP_D3D_PT_TRIANGLESTRIP &&
        primitive_type != RECOMP_D3D_PT_TRIANGLEFAN) {
        result.error = RECOMP_D3D_DRAW_UNSUPPORTED_PRIMITIVE;
        return result;
    }

    triangles = recomp_d3d_draw_triangle_count(primitive_type, index_count);
    if (triangles == 0u) {
        result.error = RECOMP_D3D_DRAW_EMPTY;
        return result;
    }

    result.error = RECOMP_D3D_DRAW_OK;
    result.plan.primitive_type = primitive_type;
    result.plan.index_count = index_count;
    result.plan.triangle_count = triangles;
    result.plan.index_data = index_data;
    /* Xbox index buffers are 16-bit. */
    result.plan.index_bytes = index_count * 2u;
    result.plan.vertex_data = state->stream0.vertex_data;
    result.plan.vertex_stride = state->stream0.stride;
    result.plan.fvf = state->fvf;
    return result;
}
