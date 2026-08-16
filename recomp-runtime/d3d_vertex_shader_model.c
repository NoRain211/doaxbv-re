#include "d3d_vertex_shader_model.h"

#include <stddef.h>

enum {
    D3D_FVF_POSITION_MASK = 0x0000000eu,
    D3D_FVF_POSITION_XYZRHW = 0x00000004u,
    D3D_FVF_NORMAL = 0x00000010u,
    D3D_FVF_DIFFUSE = 0x00000040u,
    D3D_FVF_SPECULAR = 0x00000080u,
    D3D_FVF_TEXTURE_COUNT_MASK = 0x00000f00u,
    D3D_FVF_TEXTURE_COUNT_SHIFT = 8u,
    D3D_FVF_TEXTURE_FORMAT_SHIFT = 16u,
    D3D_MAX_TEXTURE_COORDINATES = 8u,
};

static void set_word(uint32_t *declaration, uint32_t offset, uint32_t value)
{
    declaration[offset / 4u] = value;
}

void recomp_d3d_vertex_shader_reset(RecompD3dVertexShaderModel *model)
{
    if (model != NULL) {
        *model = (RecompD3dVertexShaderModel){0};
    }
}

bool recomp_d3d_build_fixed_function_declaration(
    uint32_t fvf,
    uint32_t declaration[RECOMP_D3D_VERTEX_DECLARATION_WORDS])
{
    static const uint32_t texture_coordinate_sizes[4] = {2u, 3u, 4u, 1u};
    uint32_t position = fvf & D3D_FVF_POSITION_MASK;
    uint32_t texture_count =
        (fvf & D3D_FVF_TEXTURE_COUNT_MASK) >>
        D3D_FVF_TEXTURE_COUNT_SHIFT;
    uint32_t texture_formats = fvf >> D3D_FVF_TEXTURE_FORMAT_SHIFT;
    uint32_t offset = 0u;

    if (declaration == NULL ||
        texture_count > D3D_MAX_TEXTURE_COORDINATES) {
        return false;
    }

    set_word(declaration, 0x04u, 0u);
    set_word(declaration, 0x10u, 0u);
    for (uint32_t i = 0u; i < 16u; ++i) {
        set_word(declaration, 0x1cu + i * 0x10u, 2u);
    }

    if (position == D3D_FVF_POSITION_XYZRHW) {
        set_word(declaration, 0x04u, declaration[0x04u / 4u] | 2u);
        set_word(declaration, 0x1cu, 0x42u);
        offset = 0x10u;
    } else if (position != 0u) {
        set_word(declaration, 0x1cu, 0x32u);
        offset = 0x0cu;
        if (position >= 6u) {
            uint32_t blend_weight_count = ((position - 6u) >> 1u) + 1u;

            set_word(declaration, 0x28u, 0x0cu);
            set_word(
                declaration, 0x2cu, (blend_weight_count << 4u) | 2u);
            offset += blend_weight_count * 4u;
        }
    }

    if ((fvf & D3D_FVF_NORMAL) != 0u) {
        set_word(declaration, 0x38u, offset);
        set_word(declaration, 0x3cu, 0x32u);
        offset += 0x0cu;
    }
    if ((fvf & D3D_FVF_DIFFUSE) != 0u) {
        set_word(declaration, 0x48u, offset);
        set_word(declaration, 0x4cu, 0x40u);
        set_word(declaration, 0x04u, declaration[0x04u / 4u] | 0x400u);
        offset += 4u;
    }
    if ((fvf & D3D_FVF_SPECULAR) != 0u) {
        set_word(declaration, 0x58u, offset);
        set_word(declaration, 0x5cu, 0x40u);
        set_word(declaration, 0x04u, declaration[0x04u / 4u] | 0x800u);
        offset += 4u;
    }

    for (uint32_t i = 0u; i < texture_count; ++i) {
        uint32_t size = texture_coordinate_sizes[texture_formats & 3u];

        set_word(declaration, 0xa8u + i * 0x10u, offset);
        set_word(declaration, 0xacu + i * 0x10u, (size << 4u) | 2u);
        set_word(
            declaration,
            0x10u,
            declaration[0x10u / 4u] | (size << (i * 8u)));
        offset += size * 4u;
        texture_formats >>= 2u;
    }
    return true;
}

bool recomp_d3d_bind_vertex_shader(
    RecompD3dVertexShaderModel *model,
    uint32_t handle,
    uint32_t declaration_address)
{
    uint32_t expected = (handle & 1u) != 0u
        ? handle - 1u
        : RECOMP_D3D_DEFAULT_VERTEX_DECLARATION;

    if (model == NULL || declaration_address != expected) {
        return false;
    }
    model->handle = handle;
    model->declaration_address = declaration_address;
    ++model->update_count;
    return true;
}
