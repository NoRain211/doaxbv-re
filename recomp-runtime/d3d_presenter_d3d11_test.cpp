#include "d3d_presenter_d3d11.cpp"

static int finish(RecompD3dPresenter *presenter, int status, uint32_t detail)
{
    const uint32_t cached = presenter->texture_count;
    if (presenter->texture_count > kTextureSlots) {
        presenter->texture_count = kTextureSlots;
    }
    releaseGraphics(presenter);
    bool released = presenter->texture_count == 0u &&
        presenter->next_texture_slot == 0u && presenter->device == nullptr &&
        presenter->context == nullptr && presenter->render_target_count == 0u &&
        presenter->depth_target_count == 0u &&
        presenter->render_target_view == nullptr && presenter->depth_view == nullptr &&
        presenter->depth_texture == nullptr;
    for (uint32_t i = 0u; i < kTextureSlots; ++i) {
        released = released && !presenter->textures[i].used &&
            presenter->textures[i].view == nullptr;
    }
    for (uint32_t i = 0u; i < kRenderTargetSlots; ++i) {
        released = released && presenter->render_targets[i].render_view == nullptr &&
            presenter->render_targets[i].sample_view == nullptr;
    }
    for (uint32_t i = 0u; i < kDepthTargetSlots; ++i) {
        released = released && presenter->depth_targets[i].view == nullptr;
    }
    if (!released) {
        std::fprintf(stderr, "FAIL release status=%d cached=%u\n", status, cached);
        return 90;
    }
    if (status != 0) {
        std::fprintf(stderr, "FAIL status=%d detail=%u cached=%u\n",
            status, detail, cached);
    } else {
        std::printf("PASS uploads=%u cached=%u reloaded=0x%08x\n",
            kTextureSlots + 1u, cached, detail);
    }
    return status;
}

static bool containsData(const RecompD3dPresenter &presenter, uint32_t data)
{
    for (uint32_t i = 0u; i < presenter.texture_count; ++i) {
        if (presenter.textures[i].used && presenter.textures[i].data == data) {
            return true;
        }
    }
    return false;
}

static bool createTestTargets(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D **color,
    ID3D11Texture2D **readback)
{
    presenter->config = {
        4u, 4u, RECOMP_D3D_PRESENTER_COLOR_FORMAT_BGRA8_UNORM,
        RECOMP_D3D_PRESENTER_DEPTH_FORMAT_D24S8};
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 4u;
    desc.Height = 4u;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(presenter->device->CreateTexture2D(&desc, nullptr, color)) ||
        FAILED(presenter->device->CreateRenderTargetView(
            *color, nullptr, &presenter->render_target_view))) {
        return false;
    }
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0u;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (FAILED(presenter->device->CreateTexture2D(&desc, nullptr, readback))) {
        return false;
    }
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0u;
    if (FAILED(presenter->device->CreateTexture2D(
            &desc, nullptr, &presenter->depth_texture)) ||
        FAILED(presenter->device->CreateDepthStencilView(
            presenter->depth_texture, nullptr, &presenter->depth_view))) {
        return false;
    }
    const D3D11_VIEWPORT viewport = {0.0f, 0.0f, 4.0f, 4.0f, 0.0f, 1.0f};
    presenter->context->RSSetViewports(1u, &viewport);
    return true;
}

static bool checkPixels(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback,
    const char *label,
    const uint32_t expected[4])
{
    presenter->context->CopyResource(readback, color);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(presenter->context->Map(
            readback, 0u, D3D11_MAP_READ, 0u, &mapped))) {
        std::fprintf(stderr, "FAIL %s readback\n", label);
        return false;
    }
    bool passed = true;
    for (uint32_t y = 0u; y < 4u; ++y) {
        const auto *row = reinterpret_cast<const uint32_t *>(
            static_cast<const uint8_t *>(mapped.pData) + y * mapped.RowPitch);
        for (uint32_t x = 0u; x < 4u; ++x) {
            if (row[x] != expected[x]) {
                std::fprintf(stderr,
                    "FAIL %s pixel=(%u,%u) got=%08x expected=%08x\n",
                    label, x, y, row[x], expected[x]);
                passed = false;
                break;
            }
        }
        if (!passed) {
            break;
        }
    }
    presenter->context->Unmap(readback, 0u);
    return passed;
}

static bool testOffscreenDepth(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback,
    RecompD3dPresenterDrawCommand draw,
    RecompD3dPresenterTarget target)
{
    const uint32_t blue[4] = {
        0xff0000ffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
    const uint32_t green[4] = {
        0xff00ff00u, 0xff00ff00u, 0xff00ff00u, 0xff00ff00u};
    const uint32_t red = 0xffff0000u;
    draw.texture = {};
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8;
    draw.texture.bits_per_pixel = 32u;
    draw.texture.width = draw.texture.height = 1u;
    draw.texture.data = 0x00400000u;
    draw.texture_bytes = &red;
    draw.texture_byte_count = sizeof red;
    draw.depth.depth_test_enable = draw.depth.depth_write_enable = true;
    draw.depth.depth_func = RECOMP_D3D_COMPARE_LESS;
    target.no_depth = false;
    target.custom_depth = true;
    target.depth = target.color;
    target.depth.data += 0x00020000u;
    target.depth.format_byte = target.color.width == 4u ? 0x2au : 0x2eu;
    target.depth.depth = true;
    target.depth.linear = target.depth.format_byte == 0x2eu;

    const auto checkOffscreen = [&](const char *label, const uint32_t expected[4]) {
        RecompD3dPresenterDrawCommand sample = draw;
        sample.texture = draw.target.color;
        sample.texture_bytes = nullptr;
        sample.texture_byte_count = 0u;
        sample.has_texture = true;
        sample.target = {};
        sample.depth.depth_test_enable = false;
        sample.depth.depth_write_enable = false;
        return submitDraw(presenter, sample) == RECOMP_D3D_PRESENTER_OK &&
            checkPixels(presenter, color, readback, label, expected);
    };

    RecompD3dPresenterClearCommand clear = {
        true, true, false, blue[0], 0.5f, 0u};
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK) {
        return false;
    }
    clear.target = target;
    clear.z = 1.0f;
    draw.target = target;
    draw.transform[14] = 0.25f;
    draw.has_texture = false;
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK) {
        return false;
    }
    draw.transform[14] = 0.75f;
    draw.has_texture = true;
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkOffscreen("offscreen depth occludes farther red", green)) {
        return false;
    }

    /* An offscreen clear must not raise main depth to 1; an offscreen write
       must not lower it to .25. Probe both sides without changing depth. */
    clear.target = {};
    clear.clear_depth = false;
    draw.target = {};
    draw.depth.depth_write_enable = false;
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback,
            "offscreen clear preserves main depth", blue)) {
        return false;
    }
    draw.transform[14] = 0.4f;
    draw.has_texture = false;
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback,
            "offscreen write preserves main depth", green)) {
        return false;
    }

    /* A new color target shares the written depth. EQUAL distinguishes the
       retained .25 from a newly allocated or reset depth surface. */
    target.color.data += 0x00010000u;
    clear.target = target;
    draw.target = target;
    draw.depth.depth_write_enable = true;
    draw.transform[14] = 0.75f;
    draw.has_texture = true;
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkOffscreen("shared depth rejects farther red", blue)) {
        return false;
    }
    draw.transform[14] = 0.25f;
    draw.depth.depth_func = RECOMP_D3D_COMPARE_EQUAL;
    draw.has_texture = false;
    return submitDraw(presenter, draw) == RECOMP_D3D_PRESENTER_OK &&
        checkOffscreen("shared depth retains exact written value", green);
}

static bool testOffscreenRendering(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback,
    bool seed)
{
    float vertices[4][8] = {
        {-1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        { 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f},
        {-1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        { 1.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
    };
    if (seed) {
        vertices[1][0] = vertices[3][0] = 0.0f;
    }
    const uint16_t indices[] = {0u, 1u, 2u, 3u};
    RecompD3dPresenterDrawCommand draw{};
    draw.blend.color_write_mask = 15u;
    draw.primitive_type = RECOMP_D3D_PT_TRIANGLESTRIP;
    draw.index_count = draw.vertex_count = 4u;
    draw.triangle_count = 2u;
    draw.vertex_stride = sizeof vertices[0];
    draw.fvf = 0x112u;
    draw.vertex_bytes = vertices;
    draw.index_bytes = indices;
    draw.has_transform = true;
    draw.transform[0] = draw.transform[5] =
        draw.transform[10] = draw.transform[15] = 1.0f;
    RecompD3dPresenterClearCommand clear = {
        true, true, false, 0xff0000ffu, 1.0f, 0u};
    const uint32_t blue[4] = {
        0xff0000ffu, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
    if (seed && submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK) {
        return false;
    }
    for (uint32_t size = 4u; size >= 2u; size -= 2u) {
        RecompD3dPresenterTarget target{};
        target.offscreen = target.no_depth = true;
        target.color.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8;
        target.color.bits_per_pixel = 32u;
        target.color.render_target = true;
        target.color.width = target.color.height = size;
        target.color.pitch = size * 4u;
        target.color.data = 0x00300000u + size * 0x100u;
        clear.target = target;
        clear.clear_depth = clear.clear_stencil = seed;
        if (seed) {
            /* Green geometry covers half of a red-cleared target. Neither
               operation may touch the blue back buffer. Depth/stencil clear
               flags must be ignored when no attachment is bound. */
            clear.color = 0xffff0000u;
            draw.target = target;
            if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
                submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
                !checkPixels(presenter, color, readback,
                    "offscreen clear/draw isolation", blue)) {
                return false;
            }
            clear.clear_color = false;
            if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
                !checkPixels(presenter, color, readback,
                    "no-depth clear is a no-op", blue)) {
                return false;
            }
            clear.clear_color = true;
        } else {
            /* No CPU bytes exist for these textures. Sampling must use the
               rendered contents retained through the immutable FIFO churn. */
            draw.has_texture = true;
            draw.texture = target.color;
            const uint32_t expected[4] = {
                size == 4u ? 0xff00ff00u : 0xff40bf00u,
                size == 4u ? 0xff00ff00u : 0xff40bf00u,
                size == 4u ? 0xffff0000u : 0xffbf4000u,
                size == 4u ? 0xffff0000u : 0xffbf4000u,
            };
            if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
                !checkPixels(presenter, color, readback,
                    "retained offscreen pixels and restored viewport", expected)) {
                return false;
            }
            /* Retarget while this resource is still bound for sampling. */
            clear.color = blue[0];
            if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
                submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
                !checkPixels(presenter, color, readback,
                    "sampled target cleared and resampled", blue)) {
                return false;
            }
            if (!testOffscreenDepth(presenter, color, readback, draw, target)) {
                return false;
            }
        }
    }
    return true;
}

static bool testFourTapFilter(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback)
{
    struct Vertex { float x, y, z, rhw; uint32_t diffuse; float u, v; };
    struct FilterVertex { float x, y, z, rhw; float uv[4][2]; };
    static_assert(sizeof(FilterVertex) == 48u, "XYZRHW|TEX4 stride");
    Vertex vertices[4]{};
    FilterVertex taps[4]{};
    const uint16_t indices[] = {0u, 1u, 2u, 3u};
    for (uint32_t i = 0u; i < 4u; ++i) {
        vertices[i].x = taps[i].x = i & 1u ? 3.5f : -0.5f;
        vertices[i].y = taps[i].y = i & 2u ? 3.5f : -0.5f;
        vertices[i].rhw = taps[i].rhw = 1.0f;
        vertices[i].diffuse = 0xffffffffu;
    }
    RecompD3dPresenterDrawCommand seed{};
    seed.blend.color_write_mask = 15u;
    seed.primitive_type = RECOMP_D3D_PT_TRIANGLESTRIP;
    seed.index_count = seed.vertex_count = 4u;
    seed.triangle_count = 2u;
    seed.vertex_stride = sizeof(Vertex);
    seed.fvf = 0x144u;
    seed.vertex_bytes = vertices;
    seed.index_bytes = indices;
    seed.has_texture = true;
    seed.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8;
    seed.texture.bits_per_pixel = 32u;
    seed.texture.width = 4u;
    seed.texture.height = 1u;
    seed.texture_byte_count = 4u * sizeof(uint32_t);
    RecompD3dPresenterTarget source{};
    source.offscreen = source.no_depth = true;
    source.color = seed.texture;
    source.color.format_byte = 0x12u;
    source.color.render_target = source.color.linear = true;
    source.color.height = 4u;
    source.color.pitch = 16u;
    source.color.data = 0x00610000u;
    RecompD3dPresenterTarget filtered = source;
    filtered.color.data = 0x00620000u;
    RecompD3dPresenterDrawCommand draw = seed;
    draw.vertex_stride = sizeof(FilterVertex);
    draw.fvf = 0x404u;
    draw.vertex_bytes = taps;
    draw.four_tap_filter = true;
    struct Case {
        const char *label;
        uint32_t texels[4];
        uint32_t result;
        bool outside_edges;
    };
    const Case cases[] = {
        {"four distinct RGBA taps", {0x10ff0000u, 0x4000ff00u, 0x800000ffu, 0xc0408040u}, 0x64506050u, false},
        {"four-tap 128/255 coefficient", {0x80808080u, 0x80808080u, 0x80808080u, 0x80808080u}, 0x81818181u, false},
        /* Clamping each pair first gives alpha 191 and red 128. Clamping
           only the final sum gives 192 and 129; a plain average also fails
           the preceding coefficient case. */
        {"four-tap pair saturation and partial alpha", {0xffffff00u, 0xffffff00u, 0xfe027c00u, 0u}, 0xbf809f00u, false},
        {"four-tap edge clamp", {0x10ff0000u, 0x4000ff00u, 0x800000ffu, 0xc0408040u}, 0x64506050u, true},
    };
    for (uint32_t c = 0u; c < sizeof cases / sizeof cases[0]; ++c) {
        const Case &test = cases[c];
        const uint32_t expected[4] = {test.result, test.result, test.result, test.result};
        for (uint32_t i = 0u; i < 4u; ++i) {
            vertices[i].u = i & 1u ? 1.0f : 0.0f;
            vertices[i].v = i & 2u ? 1.0f : 0.0f;
        }
        seed.texture.data = 0x00600000u + c * 0x100u;
        seed.texture_bytes = test.texels;
        seed.target = source;
        if (submitDraw(presenter, seed) != RECOMP_D3D_PRESENTER_OK) return false;
        for (uint32_t linear = 0u; linear < 2u; ++linear) {
            draw.target = {};
            draw.texture_is_backbuffer = false;
            draw.texture = linear ? source.color : seed.texture;
            draw.texture_bytes = linear ? nullptr : test.texels;
            draw.texture_byte_count = linear ? 0u : sizeof test.texels;
            for (FilterVertex &vertex : taps) {
                for (uint32_t t = 0u; t < 4u; ++t) {
                    float u = (static_cast<float>(t) + 0.5f) / 4.0f;
                    if (test.outside_edges && t == 0u) u = -1.0f;
                    if (test.outside_edges && t == 3u) u = 2.0f;
                    vertex.uv[t][0] = u * (linear ? 4.0f : 1.0f);
                    vertex.uv[t][1] = linear ? 2.0f : 0.5f;
                }
            }
            char label[112];
            std::snprintf(label, sizeof label, "%s (%s UV)", test.label,
                linear ? "linear pixel" : "swizzled normalized");
            if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
                !checkPixels(presenter, color, readback, label, expected)) return false;
        }
        /* Refresh the current back buffer for each case. The filtered output
           must consume that snapshot without overwriting its source. */
        seed.target = {};
        draw.target = filtered;
        draw.texture_is_backbuffer = true;
        draw.texture.data = 0x00630000u;
        if (submitDraw(presenter, seed) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback,
                "backbuffer filter preserves source", test.texels)) return false;
        RecompD3dPresenterDrawCommand consume = seed;
        consume.texture = filtered.color;
        consume.texture_bytes = nullptr;
        consume.texture_byte_count = 0u;
        for (Vertex &vertex : vertices) {
            vertex.u *= 4.0f;
            vertex.v *= 4.0f;
        }
        if (submitDraw(presenter, consume) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback,
                "ordinary glyph consumes filtered offscreen RGBA", expected)) return false;
    }
    return true;
}

static bool testPretransformedGlyphs(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback)
{
    struct Vertex { float x, y, z, rhw; uint32_t diffuse; float u, v; };
    Vertex vertices[] = {
        {-0.5f, -0.5f, 0.25f, 1.0f, 0x80ff8040u, 0.0f, 0.5f},
        { 1.5f, -0.5f, 0.25f, 1.0f, 0x80ff8040u, 1.0f, 0.5f},
        {-0.5f,  3.5f, 0.25f, 1.0f, 0x80ff8040u, 0.0f, 0.5f},
        { 1.5f,  3.5f, 0.25f, 1.0f, 0x80ff8040u, 1.0f, 0.5f},
    };
    const uint16_t indices[] = {0u, 1u, 2u, 3u};
    const uint8_t atlas[] = {0u, 255u};
    RecompD3dPresenterDrawCommand draw{};
    draw.blend.color_write_mask = 15u;
    draw.primitive_type = RECOMP_D3D_PT_TRIANGLESTRIP;
    draw.index_count = draw.vertex_count = 4u;
    draw.triangle_count = 2u;
    draw.vertex_stride = sizeof(Vertex);
    draw.fvf = 0x144u;
    draw.vertex_bytes = vertices;
    draw.index_bytes = indices;
    draw.has_texture = true;
    draw.texture.data = 0x00200700u;
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8;
    draw.texture.bits_per_pixel = 8u;
    draw.texture.width = 2u;
    draw.texture.height = 1u;
    draw.texture_bytes = atlas;
    draw.texture_byte_count = sizeof atlas;
    const RecompD3dPresenterClearCommand clear = {
        true, true, false, 0xff0000ffu, 1.0f, 0u};
    const uint32_t placed[] = {0x00ff8040u, 0x80ff8040u, 0xff0000ffu, 0xff0000ffu};
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "XYZRHW glyph placement, atlas alpha, diffuse", placed)) {
        return false;
    }
    /* One pixel spans the quad: RHW 1:3 moves its sample from U=.5 to
       U=.75, the opaque texel center. Ignoring RHW gives half the alpha. */
    vertices[1].x = vertices[3].x = 0.5f;
    vertices[1].rhw = vertices[3].rhw = 3.0f;
    const uint32_t perspective[] = {0x80ff8040u, 0xff0000ffu, 0xff0000ffu, 0xff0000ffu};
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "XYZRHW perspective atlas interpolation", perspective)) {
        return false;
    }

    /* Two identical 4-texel rows, in rectangular Morton order. Treating
       these bytes as linear indices repeats the first two colors. */
    const uint8_t palettized[] = {0u, 1u, 0u, 1u, 2u, 255u, 2u, 255u};
    uint32_t palette[256]{};
    palette[0] = 0x00ff0000u;
    palette[1] = 0x8000ff00u;
    palette[2] = 0xffffff00u;
    palette[255] = 0xff00ffffu;
    vertices[1].x = vertices[3].x = 3.5f;
    for (Vertex &vertex : vertices) {
        vertex.rhw = 1.0f;
        vertex.diffuse = 0xffffffffu;
        vertex.v = 0.25f;
    }
    draw.texture.data = 0x00200800u;
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_P8;
    draw.texture.width = 4u;
    draw.texture.height = 2u;
    draw.texture_bytes = palettized;
    draw.texture_byte_count = sizeof palettized;
    draw.palette_bytes = palette;
    draw.palette_byte_count = sizeof palette;
    ID3D11ShaderResourceView *original = lookupTexture(presenter, draw);
    const uint32_t colors[] = {palette[0], palette[1], palette[2], palette[255]};
    if (original == nullptr ||
        submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "P8 rectangular swizzle and ARGB palette", colors)) {
        return false;
    }
    draw.blend.blend_enable = true;
    draw.blend.src_factor = RECOMP_D3D_BLEND_SRC_ALPHA;
    draw.blend.dst_factor = RECOMP_D3D_BLEND_INV_SRC_ALPHA;
    draw.blend.op = RECOMP_D3D_BLEND_OP_ADD;
    const uint32_t blended[] = {0xff0000ffu, 0xbf00807fu, 0xffffff00u, 0xff00ffffu};
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "P8 transparent and partial alpha blending", blended)) {
        return false;
    }
    draw.blend.blend_enable = false;
    palette[255] = 0xffff00ffu;
    ID3D11ShaderResourceView *changed = lookupTexture(presenter, draw);
    const uint32_t changed_colors[] = {colors[0], colors[1], colors[2], palette[255]};
    if (changed == nullptr || changed == original ||
        submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "P8 same-pointer palette mutation", changed_colors)) {
        return false;
    }
    palette[255] = colors[3];
    if (lookupTexture(presenter, draw) != original ||
        submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "P8 restored palette cache hit", colors)) {
        return false;
    }
    draw.palette_bytes = nullptr;
    if (lookupTexture(presenter, draw) != nullptr) return false;
    draw.palette_bytes = palette;
    draw.palette_byte_count = sizeof palette - 1u;
    return lookupTexture(presenter, draw) == nullptr;
}

static bool testShadowRendering(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback,
    RecompD3dPresenterDrawCommand draw)
{
    const uint32_t white[4] = {0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu};
    const uint32_t shaded[4] = {0xbf7f7f7fu, 0xbf7f7f7fu, 0xbf7f7f7fu, 0xbf7f7f7fu};
    const uint32_t factor[4] = {0x80402010u, 0x80402010u, 0x80402010u, 0x80402010u};
    const RecompD3dPresenterClearCommand clear = {
        true, true, true, white[0], 1.0f, 0xa2u};
    draw.depth = {};
    draw.depth.depth_test_enable = true;
    draw.depth.depth_func = RECOMP_D3D_COMPARE_LESS_EQUAL;
    draw.depth.stencil_enable = true;
    draw.depth.stencil_func = RECOMP_D3D_COMPARE_LESS_EQUAL;
    draw.depth.stencil_ref = 1u;
    draw.depth.stencil_read_mask = draw.depth.stencil_write_mask = 3u;
    draw.depth.stencil_fail = draw.depth.stencil_zfail = RECOMP_D3D_STENCIL_KEEP;
    draw.depth.stencil_pass = RECOMP_D3D_STENCIL_INVERT;
    draw.blend.color_write_mask = 0u;
    draw.use_texture_factor = true;
    draw.texture_factor = 0x80000000u;
    draw.material_alpha_mode = RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE;
    draw.material_alpha = 0.65f;

    RecompD3dDepthState cache_depth = draw.depth;
    for (uint32_t i = 0u; i < 18u; ++i) {
        cache_depth.stencil_read_mask = i < 17u ? i : 0u;
        ID3D11DepthStencilState *state = lookupDepthState(presenter, cache_depth);
        if (state == nullptr) {
            std::fprintf(stderr, "FAIL stencil cache tuple=%u unavailable\n", i);
            return false;
        }
        D3D11_DEPTH_STENCIL_DESC desc{};
        state->GetDesc(&desc);
        if (!desc.StencilEnable || desc.StencilReadMask != cache_depth.stencil_read_mask ||
            desc.StencilWriteMask != 3u ||
            desc.FrontFace.StencilFunc != D3D11_COMPARISON_LESS_EQUAL ||
            desc.FrontFace.StencilPassOp != D3D11_STENCIL_OP_INVERT) {
            std::fprintf(stderr, "FAIL stencil cache tuple=%u descriptor\n", i);
            return false;
        }
    }

    /* LEQUAL compares reference 1 against masked stencil 2; INVERT changes
       only the two writable bits, so A2 becomes A1 without coloring pixels. */
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "shadow stencil mark", white)) {
        return false;
    }
    draw.blend.color_write_mask = 15u;
    draw.depth.stencil_func = RECOMP_D3D_COMPARE_EQUAL;
    draw.depth.stencil_pass = RECOMP_D3D_STENCIL_ZERO;
    draw.depth.alpha_test_enable = true;
    draw.depth.alpha_func = RECOMP_D3D_COMPARE_GREATER;
    draw.depth.alpha_ref = 128u;
    /* Constant alpha 128 must reject every column, including texture alpha
       255, and discard must leave A1 available for the following pass. */
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "factor alpha rejects texture", white)) {
        return false;
    }
    draw.depth.alpha_func = RECOMP_D3D_COMPARE_EQUAL;
    draw.has_texture = false;
    draw.blend.blend_enable = true;
    draw.blend.src_factor = RECOMP_D3D_BLEND_SRC_ALPHA;
    draw.blend.dst_factor = RECOMP_D3D_BLEND_INV_SRC_ALPHA;
    draw.blend.op = RECOMP_D3D_BLEND_OP_ADD;
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "black factor overrides normals and blends", shaded)) {
        return false;
    }
    /* ZERO clears the writable low bits while preserving A0. Compare all
       eight bits with a new reference, then expose the exact ARGB factor. */
    draw.depth.stencil_read_mask = 255u;
    draw.depth.stencil_ref = 0xa0u;
    draw.depth.stencil_pass = RECOMP_D3D_STENCIL_KEEP;
    draw.has_texture = true;
    draw.blend.blend_enable = false;
    draw.texture_factor = factor[0];
    draw.material_alpha_mode = RECOMP_D3D_MATERIAL_ALPHA_SELECT_DIFFUSE;
    draw.modulate_texture_factor = true;
    return submitDraw(presenter, draw) == RECOMP_D3D_PRESENTER_OK &&
        checkPixels(presenter, color, readback, "stencil masks, reference, texture factor", factor);
}

static bool testMaterialAlphaRendering(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback,
    RecompD3dPresenterDrawCommand draw)
{
    const uint32_t red = 0xffff0000u;
    const RecompD3dPresenterClearCommand clear = {
        true, false, false, 0xff0000ffu, 1.0f, 0u};
    draw.depth = {};
    draw.blend.color_write_mask = 15u;
    draw.blend.src_factor = RECOMP_D3D_BLEND_SRC_ALPHA;
    draw.blend.dst_factor = RECOMP_D3D_BLEND_INV_SRC_ALPHA;
    draw.blend.op = RECOMP_D3D_BLEND_OP_ADD;
    draw.use_texture_factor = false;
    draw.material_alpha = 0.65f;
    draw.has_texture = true;
    draw.texture = {};
    draw.texture.data = 0x00200300u;
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8;
    draw.texture.bits_per_pixel = 32u;
    draw.texture.width = draw.texture.height = 1u;
    draw.texture_bytes = &red;
    draw.texture_byte_count = sizeof red;
    const struct {
        const char *label;
        bool material_alpha;
        bool blend;
        bool alpha_test;
        RecompD3dCompareFunc alpha_func;
        uint32_t pixel;
    } cases[] = {
        {"material scales texture alpha", true, false, false, RECOMP_D3D_COMPARE_ALWAYS, 0xa6ff0000u},
        {"material alpha blends over blue", true, true, false, RECOMP_D3D_COMPARE_ALWAYS, 0xc5a60059u},
        {"material alpha equals 166", true, false, true, RECOMP_D3D_COMPARE_EQUAL, 0xa6ff0000u},
        {"material alpha rejects greater 166", true, false, true, RECOMP_D3D_COMPARE_GREATER, 0xff0000ffu},
        {"disabled material restores texture alpha", false, false, false, RECOMP_D3D_COMPARE_ALWAYS, 0xffff0000u},
    };
    for (const auto &test : cases) {
        draw.material_alpha_mode = test.material_alpha
            ? RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE : RECOMP_D3D_MATERIAL_ALPHA_NONE;
        draw.blend.blend_enable = test.blend;
        draw.depth.alpha_test_enable = test.alpha_test;
        draw.depth.alpha_func = test.alpha_func;
        draw.depth.alpha_ref = 166u;
        const uint32_t expected[4] = {test.pixel, test.pixel, test.pixel, test.pixel};
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, test.label, expected)) {
            return false;
        }
    }
    const uint32_t textures[] = {0x00ff0000u, 0xffff0000u};
    for (uint32_t i = 0u; i < 4u; ++i) {
        draw.material_alpha_mode = i < 2u ? RECOMP_D3D_MATERIAL_ALPHA_SELECT_DIFFUSE
            : RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE;
        draw.material_alpha = i < 2u ? 0.25f : 0.65f;
        draw.texture.data = 0x00200400u + (i & 1u) * 0x100u;
        draw.texture_bytes = &textures[i & 1u];
        const uint32_t pixel = i < 2u ? 0x40ff0000u
            : (i & 1u) ? 0xa6ff0000u : 0x00ff0000u;
        const uint32_t expected[4] = {pixel, pixel, pixel, pixel};
        char label[60];
        std::snprintf(label, sizeof label, "material %s texture alpha=%u",
            i < 2u ? "SELECTDIFFUSE" : "MODULATE", (i & 1u) * 255u);
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, label, expected)) {
            return false;
        }
    }
    const uint32_t texel = 0xff804020u;
    draw.texture.data = 0x00200600u;
    draw.texture_bytes = &texel;
    const struct {
        const char *label;
        bool modulate;
        RecompD3dMaterialAlphaMode material;
        uint32_t factor;
        uint32_t pixel;
    } factor_cases[] = {
        {"stage1 combines material and factor alpha", true,
            RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE, 0xa5ffffffu, 0x6b804020u},
        {"disabled stage1 retains material alpha", false,
            RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE, 0xa5ffffffu, 0xa6804020u},
        {"stage1 multiplies every RGBA channel", true,
            RECOMP_D3D_MATERIAL_ALPHA_NONE, 0x800080ffu, 0x80002020u},
        {"disabled stage1 restores texture RGBA", false,
            RECOMP_D3D_MATERIAL_ALPHA_NONE, 0x800080ffu, 0xff804020u},
    };
    for (const auto &test : factor_cases) {
        draw.modulate_texture_factor = test.modulate;
        draw.material_alpha_mode = test.material;
        draw.texture_factor = test.factor;
        const uint32_t expected[4] = {test.pixel, test.pixel, test.pixel, test.pixel};
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, test.label, expected)) {
            return false;
        }
    }
    const struct {
        const char *label;
        bool zero_rgb;
        bool replace_factor;
        bool modulate_factor;
        RecompD3dMaterialAlphaMode material;
        uint32_t texel;
        uint32_t factor;
        uint32_t pixel;
    } zero_diffuse_cases[] = {
        {"zero diffuse preserves texture alpha", true, false, false,
            RECOMP_D3D_MATERIAL_ALPHA_NONE, 0x80804020u, 0xa5ffffffu, 0x80000000u},
        {"zero diffuse retains material and factor alpha", true, false, true,
            RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE, 0xff804020u, 0xa5ffffffu, 0x6b000000u},
        {"disabled zero diffuse restores RGB", false, false, true,
            RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE, 0xff804020u, 0xa5ffffffu, 0x6b804020u},
        {"texture factor replacement overrides zero diffuse", true, true, true,
            RECOMP_D3D_MATERIAL_ALPHA_MODULATE_TEXTURE, 0x80804020u, 0xa5804020u, 0xa5804020u},
    };
    draw.material_alpha = 0.65f;
    for (uint32_t i = 0u; i < sizeof zero_diffuse_cases / sizeof zero_diffuse_cases[0]; ++i) {
        const auto &test = zero_diffuse_cases[i];
        draw.zero_diffuse_rgb = test.zero_rgb;
        draw.use_texture_factor = test.replace_factor;
        draw.modulate_texture_factor = test.modulate_factor;
        draw.material_alpha_mode = test.material;
        draw.texture.data = 0x00200700u + i * 0x100u;
        draw.texture_bytes = &test.texel;
        draw.texture_factor = test.factor;
        const uint32_t expected[4] = {test.pixel, test.pixel, test.pixel, test.pixel};
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, test.label, expected)) {
            return false;
        }
    }
    return true;
}

static bool testAlphaRendering(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *color,
    ID3D11Texture2D *readback)
{
    /* The oversized triangle samples texel centers across the 4x4 target.
       DXT3 stores alpha 0, 85, 170, 255 in each row; every texel is red. */
    float vertices[3][8] = {
        {-1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        { 3.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2.0f, 1.0f},
        {-1.0f,  3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,-1.0f},
    };
    const uint16_t indices[] = {0u, 1u, 2u};
    const uint8_t red[16] = {
        0x50u, 0xfau, 0x50u, 0xfau, 0x50u, 0xfau, 0x50u, 0xfau,
        0x00u, 0xf8u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    };
    const uint32_t blue = 0xff0000ffu;
    const uint32_t green = 0xff00ff00u;
    RecompD3dPresenterDrawCommand draw{};
    draw.blend.color_write_mask = 15u;
    draw.primitive_type = RECOMP_D3D_PT_TRIANGLELIST;
    draw.index_count = 3u;
    draw.triangle_count = 1u;
    draw.vertex_count = 3u;
    draw.vertex_stride = sizeof vertices[0];
    draw.fvf = 0x112u; /* XYZ|NORMAL|TEX1, without diffuse vertex color. */
    draw.vertex_bytes = vertices;
    draw.index_bytes = indices;
    draw.has_transform = true;
    draw.transform[0] = draw.transform[5] =
        draw.transform[10] = draw.transform[15] = 1.0f;
    draw.transform[14] = 0.25f;
    draw.depth.depth_test_enable = true;
    draw.depth.depth_write_enable = true;
    draw.depth.depth_func = RECOMP_D3D_COMPARE_LESS;
    draw.has_texture = true;
    draw.texture.data = 0x00200000u;
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_DXT3;
    draw.texture.bits_per_pixel = 8u;
    draw.texture.width = draw.texture.height = 4u;
    draw.texture_bytes = red;
    draw.texture_byte_count = sizeof red;
    const RecompD3dPresenterClearCommand clear = {
        true, true, false, 0xff000000u, 1.0f, 0u};
    const uint32_t preserved_alpha[] = {
        0x00ff0000u, 0x55ff0000u, 0xaaff0000u, 0xffff0000u};
    if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK) {
        return false;
    }
    bool passed = checkPixels(
        presenter, color, readback, "texture alpha preserved", preserved_alpha);

    RecompD3dPresenterClearCommand mask_clear = clear;
    mask_clear.color = 0x12345678u;
    const struct { uint8_t mask; uint32_t bytes; } masks[] = {
        {1u, 0x00ff0000u}, {2u, 0x0000ff00u},
        {4u, 0x000000ffu}, {8u, 0xff000000u},
        {15u, 0xffffffffu}, {0u, 0u},
        {1u, 0x00ff0000u}, {15u, 0xffffffffu},
    };
    for (const auto &test : masks) {
        draw.blend.color_write_mask = test.mask;
        uint32_t expected[4];
        for (uint32_t x = 0u; x < 4u; ++x) {
            expected[x] = (preserved_alpha[x] & test.bytes) |
                (mask_clear.color & ~test.bytes);
        }
        char label[40];
        std::snprintf(label, sizeof label, "color write mask=%u", unsigned(test.mask));
        if (submitClear(presenter, mask_clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, label, expected)) {
            return false;
        }
    }

    /* Seventeen distinct tuples exceed the sixteen-state cache; the final
       request revisits the first tuple after eviction. */
    RecompD3dBlendState cache_blend = draw.blend;
    cache_blend.src_factor = RECOMP_D3D_BLEND_ONE;
    cache_blend.dst_factor = RECOMP_D3D_BLEND_ZERO;
    cache_blend.op = RECOMP_D3D_BLEND_OP_ADD;
    for (uint32_t i = 0u; i < 18u; ++i) {
        cache_blend.blend_enable = i == 16u;
        cache_blend.color_write_mask = static_cast<uint8_t>(i < 16u ? i : 0u);
        ID3D11BlendState *state = lookupBlendState(presenter, cache_blend);
        if (state == nullptr) {
            std::fprintf(stderr, "FAIL blend cache tuple=%u unavailable\n", i);
            return false;
        }
        D3D11_BLEND_DESC desc{};
        state->GetDesc(&desc);
        if (desc.RenderTarget[0].RenderTargetWriteMask != cache_blend.color_write_mask ||
            (desc.RenderTarget[0].BlendEnable != FALSE) != cache_blend.blend_enable) {
            std::fprintf(stderr, "FAIL blend cache tuple=%u descriptor\n", i);
            return false;
        }
    }

    /* Suppressing every color channel must still write depth at .25. */
    const uint32_t untouched[4] = {
        mask_clear.color, mask_clear.color, mask_clear.color, mask_clear.color};
    draw.blend.color_write_mask = 0u;
    if (submitClear(presenter, mask_clear) != RECOMP_D3D_PRESENTER_OK ||
        submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "zero mask preserves color", untouched)) {
        return false;
    }
    draw.blend.color_write_mask = 15u;
    draw.transform[14] = 0.5f;
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "zero mask writes depth", untouched)) {
        return false;
    }
    draw.transform[14] = 0.125f;
    if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
        !checkPixels(presenter, color, readback, "all color writes restored", preserved_alpha)) {
        return false;
    }
    draw.transform[14] = 0.25f;

    const struct {
        RecompD3dCompareFunc func;
        uint32_t ref;
        uint32_t passing_columns;
        float u_offset;
    } cases[] = {
        {RECOMP_D3D_COMPARE_NEVER,          85u, 0x0u},
        {RECOMP_D3D_COMPARE_LESS,           85u, 0x1u},
        {RECOMP_D3D_COMPARE_EQUAL,          85u, 0x2u},
        {RECOMP_D3D_COMPARE_LESS_EQUAL,     85u, 0x3u},
        {RECOMP_D3D_COMPARE_GREATER,        85u, 0xcu},
        {RECOMP_D3D_COMPARE_NOT_EQUAL,      85u, 0xdu},
        {RECOMP_D3D_COMPARE_GREATER_EQUAL,  85u, 0xeu},
        {RECOMP_D3D_COMPARE_ALWAYS,         85u, 0xfu},
        {RECOMP_D3D_COMPARE_GREATER,         0u, 0xeu},
        {RECOMP_D3D_COMPARE_EQUAL,         255u, 0x8u},
        {RECOMP_D3D_COMPARE_GREATER,       255u, 0x0u},
        /* Linear filtering yields alpha 85.33 in column 1: byte rounding
           must preserve EQ 85, while a raw float comparison rejects it. */
        {RECOMP_D3D_COMPARE_EQUAL,          85u, 0x2u, 1.0f / 1024.0f},
    };
    for (const auto &test : cases) {
        vertices[0][6] = vertices[2][6] = test.u_offset;
        vertices[1][6] = 2.0f + test.u_offset;
        draw.depth.alpha_test_enable = true;
        draw.depth.alpha_func = test.func;
        draw.depth.alpha_ref = test.ref;
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK) {
            return false;
        }
        /* Rejected red pixels must leave depth untouched so blue can render.
           Farther green must then fail depth behind both blue and red. */
        RecompD3dPresenterDrawCommand background = draw;
        background.depth.alpha_test_enable = false;
        background.transform[14] = 0.5f;
        background.texture.data = 0x00200100u;
        background.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_A8R8G8B8;
        background.texture.bits_per_pixel = 32u;
        background.texture.width = background.texture.height = 1u;
        background.texture_bytes = &blue;
        background.texture_byte_count = sizeof blue;
        if (submitDraw(presenter, background) != RECOMP_D3D_PRESENTER_OK) {
            return false;
        }
        background.transform[14] = 0.75f;
        background.texture.data = 0x00200200u;
        background.texture_bytes = &green;
        if (submitDraw(presenter, background) != RECOMP_D3D_PRESENTER_OK) {
            return false;
        }
        uint32_t expected[4];
        for (uint32_t x = 0u; x < 4u; ++x) {
            expected[x] = (test.passing_columns & (1u << x)) != 0u
                ? preserved_alpha[x] : blue;
        }
        char label[80];
        std::snprintf(label, sizeof label, "alpha func=%u ref=%u u=%g depth",
            static_cast<unsigned>(test.func), test.ref, test.u_offset);
        passed &= checkPixels(presenter, color, readback, label, expected);
    }
    return passed && testMaterialAlphaRendering(presenter, color, readback, draw) &&
        testShadowRendering(presenter, color, readback, draw);
}

static bool testLinearTextureUpdates(
    RecompD3dPresenter *presenter,
    ID3D11Texture2D *readback)
{
    uint32_t pixels[4][5]{};
    const uint32_t frames[2][4] = {
        {0xffff0000u, 0xff00ff00u, 0xff0000ffu, 0xffffffffu},
        {0xff112233u, 0x80445566u, 0x00778899u, 0xffaabbccu},
    };
    RecompD3dPresenterDrawCommand draw{};
    draw.has_texture = true;
    draw.texture = {0x12u, 32u, false, false, true, 4u, 4u,
        sizeof pixels[0], 0x00600000u};
    draw.texture_bytes = pixels;
    /* The final row needs its pixels, but not trailing padding. */
    draw.texture_byte_count = sizeof pixels - sizeof pixels[0][4];
    ID3D11ShaderResourceView *original = nullptr;
    for (const auto &frame : frames) {
        for (auto &row : pixels) {
            std::memcpy(row, frame, sizeof frame);
            row[4] = 0xff000000u;
        }
        ID3D11ShaderResourceView *view = lookupTexture(presenter, draw);
        if (view == nullptr || (original != nullptr && view != original)) {
            std::fprintf(stderr, "FAIL linear texture upload/cache reuse\n");
            return false;
        }
        original = view;
        ID3D11Resource *resource = nullptr;
        view->GetResource(&resource);
        const bool passed = checkPixels(presenter,
            static_cast<ID3D11Texture2D *>(resource), readback,
            "linear BGRA same-buffer update with row padding", frame);
        releaseCom(resource);
        if (!passed) return false;
    }
    --draw.texture_byte_count;
    if (lookupTexture(presenter, draw) != nullptr) return false;
    ++draw.texture_byte_count;
    draw.texture.pitch = 15u;
    if (lookupTexture(presenter, draw) != nullptr) return false;
    draw.texture.pitch = sizeof pixels[0];
    draw.texture.bits_per_pixel = 16u;
    if (lookupTexture(presenter, draw) != nullptr) return false;
    draw.texture.bits_per_pixel = 32u;
    draw.texture.linear = false;
    return lookupTexture(presenter, draw) == nullptr;
}

static int testTextureCache(RecompD3dPresenter *presenter, uint32_t &detail)
{
    constexpr uint32_t first_data = 0x00100000u;
    constexpr uint32_t data_stride = 0x00000100u;
    const uint8_t dxt1_block[8] = {
        0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u};
    RecompD3dPresenterDrawCommand draw{};
    draw.blend.color_write_mask = 15u;
    draw.has_texture = true;
    draw.texture.format_byte = RECOMP_D3D_TEXTURE_FORMAT_DXT1;
    draw.texture.bits_per_pixel = 4u;
    draw.texture.width = 4u;
    draw.texture.height = 4u;
    draw.texture_bytes = dxt1_block;
    draw.texture_byte_count = sizeof dxt1_block;
    for (uint32_t i = 0u; i <= kTextureSlots; ++i) {
        draw.texture.data = first_data + i * data_stride;
        detail = i + 1u;
        if (lookupTexture(presenter, draw) == nullptr) {
            return 20;
        }
        if (presenter->texture_count > kTextureSlots) {
            return 30;
        }
    }

    uint32_t evicted_data = 0u;
    uint32_t missing = 0u;
    for (uint32_t i = 0u; i < kTextureSlots; ++i) {
        const uint32_t data = first_data + i * data_stride;
        if (!containsData(*presenter, data)) {
            evicted_data = data;
            ++missing;
        }
    }
    if (missing != 1u) {
        detail = missing;
        return 40;
    }
    draw.texture.data = evicted_data;
    detail = evicted_data;
    if (lookupTexture(presenter, draw) == nullptr ||
        presenter->texture_count != kTextureSlots) {
        return 50;
    }
    return 0;
}

static bool testVertexBlending(
    RecompD3dPresenter *presenter, ID3D11Texture2D *color, ID3D11Texture2D *readback)
{
    const uint16_t indices[] = {0u, 1u, 2u, 3u};
    const uint32_t expected[] = {0xff0000ffu, 0xff0000ffu, 0xff00ff00u, 0xff00ff00u};
    for (uint32_t count = 1u; count <= 3u; ++count) {
        float vertices[44]{};
        const uint32_t stride = 8u + count;
        for (uint32_t v = 0u; v < 4u; ++v) {
            float *p = vertices + v * stride;
            p[0] = v & 1u ? 0.0f : -1.0f;
            p[1] = v & 2u ? 1.0f : -1.0f;
            for (uint32_t i = 0u; i < count; ++i) p[3u + i] = 0.25f;
            p[4u + count] = 1.0f; // Normal gives green diagnostic shading.
        }
        RecompD3dPresenterDrawCommand draw{};
        draw.blend.color_write_mask = 15u;
        draw.primitive_type = RECOMP_D3D_PT_TRIANGLESTRIP;
        draw.index_count = draw.vertex_count = 4u;
        draw.triangle_count = 2u;
        draw.vertex_stride = stride * sizeof(float);
        draw.fvf = 0x114u + 2u * count;
        draw.vertex_bytes = vertices;
        draw.index_bytes = indices;
        draw.has_transform = true;
        draw.blend_weight_count = count;
        for (uint32_t i = 0u; i <= count; ++i) {
            float *m = i ? draw.blend_transforms[i - 1u] : draw.transform;
            m[0] = m[5] = m[10] = m[15] = 1.0f;
            m[12] = i == count ? static_cast<float>(count + 1u) : 2.0f * i - 2.0f;
        }
        /* Different matrices and the implied final weight move the left
           half-screen quad exactly one clip-space unit to the right. */
        RecompD3dPresenterClearCommand clear = {true, true, false, 0xff0000ffu, 1.0f, 0u};
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, "weighted vertex positions", expected)) return false;
        draw.blend_weight_count = count == 1u ? 2u : 1u;
        if (submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_UNSUPPORTED_COMMAND) return false;
        draw.blend_weight_count = 0u;
        draw.transform[12] = 0.0f;
        for (uint32_t v = 0u; v < 4u; ++v) vertices[v * stride + 3u] = NAN;
        const uint32_t unblended[] = {0xff00ff00u, 0xff00ff00u, 0xff0000ffu, 0xff0000ffu};
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, "disabled blending ignores weights", unblended)) return false;
        draw.blend_weight_count = count;
        for (uint32_t v = 0u; v < 4u; ++v) {
            for (uint32_t i = 0u; i < count; ++i) vertices[v * stride + 3u + i] = i ? 0.0f : 1.0f;
        }
        std::memset(draw.blend_transforms, 0, sizeof draw.blend_transforms);
        if (submitClear(presenter, clear) != RECOMP_D3D_PRESENTER_OK ||
            submitDraw(presenter, draw) != RECOMP_D3D_PRESENTER_OK ||
            !checkPixels(presenter, color, readback, "unused zero blend matrices", unblended)) return false;
    }
    return true;
}

static LRESULT CALLBACK closeOnShowWindowProc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SHOWWINDOW && wparam) {
        PostMessageW(window, WM_CLOSE, 0u, 0u);
    }
    return presenterWindowProc(window, message, wparam, lparam);
}

static bool testWindowClose(RecompD3dPresenter *warp)
{
    IDXGIDevice *dxgi_device = nullptr;
    IDXGIAdapter *adapter = nullptr;
    IDXGIFactory *factory = nullptr;
    HRESULT result = warp->device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
    if (SUCCEEDED(result)) result = dxgi_device->GetAdapter(&adapter);
    if (SUCCEEDED(result)) result = adapter->GetParent(IID_PPV_ARGS(&factory));
    releaseCom(adapter);
    releaseCom(dxgi_device);
    if (FAILED(result)) return false;

    bool passed = true;
    for (unsigned scenario = 0u; scenario < 5u && passed; ++scenario) {
        RecompD3dPresenter presenter{};
        presenter.config = warp->config;
        presenter.config.width = 320u;
        presenter.config.height = 240u;
        presenter.owner_thread = GetCurrentThreadId();
        presenter.device = warp->device;
        presenter.device->AddRef();
        presenter.context = warp->context;
        presenter.context->AddRef();
        passed = createWindow(&presenter);
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = presenter.config.width;
        desc.BufferDesc.Height = presenter.config.height;
        desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1u;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 1u;
        desc.OutputWindow = presenter.window;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        if (passed) passed = SUCCEEDED(factory->CreateSwapChain(
            presenter.device, &desc, &presenter.swap_chain));
        if (!passed) std::fprintf(stderr, "FAIL window setup scenario=%u\n", scenario);
        if (passed) {
            active_presenter = &presenter;
            RecompD3dPresenterCommand command{};
            command.type = RECOMP_D3D_PRESENTER_COMMAND_PRESENT;
            command.data.present.effective_flags = 5u;
            command.data.present.swap_counter = 1u;
            if (scenario == 0u || scenario == 4u) {
                passed = PostMessageW(presenter.window, WM_CLOSE, 0u, 0u) != 0;
            } else if (scenario == 1u) {
                SetWindowLongPtrW(presenter.window, GWLP_WNDPROC,
                    reinterpret_cast<LONG_PTR>(closeOnShowWindowProc));
            } else if (scenario == 2u) {
                passed = DestroyWindow(presenter.window) != 0;
            }
            if (scenario == 3u || scenario == 4u) PostQuitMessage(0);
            const RecompD3dPresenterError expected = scenario < 2u
                ? RECOMP_D3D_PRESENTER_CLOSED : RECOMP_D3D_PRESENTER_HOST_FAILURE;
            const RecompD3dPresenterError actual =
                recomp_d3d_presenter_submit(&presenter, &command);
            passed = passed && actual == expected &&
                presenter.close_requested == (scenario < 2u || scenario == 4u) &&
                presenter.present_count == (scenario == 1u ? 1u : 0u);
            if (!passed) std::fprintf(stderr,
                "FAIL window close scenario=%u result=%u expected=%u\n",
                scenario, static_cast<unsigned>(actual), static_cast<unsigned>(expected));
            active_presenter = nullptr;
        }
        releasePresenter(&presenter);
    }
    releaseCom(factory);
    if (passed) std::printf("PASS window close before/after present, unexpected loss/quit\n");
    return passed;
}

int main()
{
    ULONGLONG next_dump = 0u;
    if (!frameDumpDue(100u, 10000u, next_dump) || next_dump != 10100u ||
        frameDumpDue(10099u, 10000u, next_dump) ||
        !frameDumpDue(10100u, 10000u, next_dump) || next_dump != 20100u ||
        !frameDumpDue(100000u, 10000u, next_dump) || next_dump != 110000u ||
        frameDumpDue(100001u, 10000u, next_dump) ||
        !frameDumpDue(100001u, 0u, next_dump)) {
        std::fprintf(stderr, "FAIL capture timing/burst suppression\n");
        return 1;
    }
    RecompD3dPresenter presenter{};
    D3D_FEATURE_LEVEL feature_level{};
    HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0u, D3D11_SDK_VERSION,
        &presenter.device, &feature_level, &presenter.context);
    if (FAILED(result)) {
        return finish(&presenter, 10, static_cast<uint32_t>(result));
    }
    ID3D11Texture2D *color = nullptr;
    ID3D11Texture2D *readback = nullptr;
    uint32_t detail = 0u;
    int status = 0;
    if (!createTestTargets(&presenter, &color, &readback) ||
        !testOffscreenRendering(&presenter, color, readback, true)) {
        status = 70;
    } else {
        status = testTextureCache(&presenter, detail);
    }
    if (status == 0 &&
        !testOffscreenRendering(&presenter, color, readback, false)) {
        status = 70;
    }
    if (status == 0 && !testFourTapFilter(&presenter, color, readback)) status = 87;
    if (status == 0 && !testPretransformedGlyphs(&presenter, color, readback)) {
        status = 85;
    }
    if (status == 0 && !testAlphaRendering(&presenter, color, readback)) {
        status = 60;
    }
    if (status == 0 && !testVertexBlending(&presenter, color, readback)) status = 80;
    if (status == 0 && !testLinearTextureUpdates(&presenter, readback)) status = 88;
    if (status == 0 && !testWindowClose(&presenter)) status = 86;
    releaseCom(readback);
    releaseCom(color);
    if (status == 0) {
        std::printf("PASS offscreen isolation, retained pixels, viewport restore\n");
        std::printf("PASS offscreen depth occlusion, isolation, shared storage\n");
        std::printf("PASS texture alpha, all alpha comparisons, discarded depth\n");
        std::printf("PASS four-tap RGBA, linear UVs, backbuffer snapshot and filtered output\n");
        std::printf("PASS mutable linear BGRA pixels, row padding, cached source bounds\n");
    }
    return finish(&presenter, status, detail);
}
