# Direct-Vulkan renderer seams

Status: resolved research for “Research direct-Vulkan renderer seams.” This
note uses only the public source tree and public primary references.

## Decision

Keep rendering at the API-level D3D8 replacement boundary:

`D3D8 manual lookup -> D3D8 model -> semantic presenter command -> D3D11 or Vulkan backend`

The Vulkan backend must not expose or implement NV2A registers, PFIFO/RAMHT
state, push-buffer decoding, or an Xbox GPU device model. The current presenter
interface is sufficient for the first clear/present slice, but it does not yet
express resources, uploads, draws, shaders, viewport/scissor, blend/depth
state, resizing, or device loss.

Use a backend-private object/vtable behind `RecompD3dPresenter`. Keep semantic
commands and the C models backend-independent. Retain D3D11 as a reference
backend while Vulkan gains capabilities one validated D3D8 behavior at a time.
Do not define a broad render IR before real draw and resource requirements are
known; that risks recreating GPU emulation under a new name.

## Why the seam fits Vulkan

Vulkan applications explicitly own the surface, swapchain, image acquisition,
render submission, and presentation. The backend's create path should own the
instance, physical/logical device, queue, Win32 surface, swapchain, image views,
and per-frame resources. Present must enforce acquire-render-present ordering
and handle `VK_ERROR_OUT_OF_DATE_KHR` and `VK_SUBOPTIMAL_KHR` as swapchain
lifecycle events. See the [Khronos swapchain guide](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/01_Swap_chain.html)
and [synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html#_swapchain_image_acquire_and_present).

Dynamic rendering is appropriate for incremental growth because it avoids
predeclared render-pass and framebuffer objects, but it leaves attachment
transitions and synchronization explicit. See the
[dynamic-rendering specification](https://docs.vulkan.org/spec/latest/chapters/renderpass.html)
and [Khronos synchronization guidance](https://github.khronos.org/Vulkan-Site/tutorial/latest/Synchronization/Dynamic_Rendering_Sync/01_introduction.html).

The current logical color format maps naturally to
`VK_FORMAT_B8G8R8A8_UNORM`. Depth/stencil selection must query device support
for packed D24S8 and apply a documented fallback rather than silently assuming
availability. See [Vulkan format features](https://docs.vulkan.org/spec/latest/chapters/formats.html).

## Delivery slices

1. Add a Vulkan backend behind the existing presenter lifecycle and command
   contract. Implement window/surface/device/swapchain creation, clear,
   present, out-of-date recreation, and destroy. Keep D3D8 models unchanged.
2. Add semantic presenter operations only when required by validated D3D8 API
   behavior: resource creation/upload, immutable draw data, viewport/scissor,
   shader translation or a bounded fixed-function path, render state, resize,
   and device-loss reporting.

The first backend should use one owning render thread, a graphics/present queue
where available, per-frame command resources, acquire and render-finished
semaphores, and a fence or equivalent timeline synchronization. A small number
of in-flight frames matches the current synchronous contract and avoids early
concurrency complexity.

## Dependency and platform decisions

- Start with Win32 WSI. Do not leak Win32, SDL, or GLFW types into D3D8 models.
- Prefer runtime backend selection so D3D11 and Vulkan can be compared, but
  define explicit capability and fallback behavior first.
- Decide whether Vulkan 1.3 and dynamic rendering are minimum requirements or
  whether older devices require a legacy-render-pass path.
- Keep validation layers and deterministic command traces as optional
  development diagnostics, not product dependencies.
- Review the Vulkan loader, headers, window library, and every transitive
  license before adding a package.

## Next gate

Add a backend-private Vulkan presenter that creates a Win32
window/surface/device/swapchain, accepts the existing clear command, presents
one frame, recreates an out-of-date swapchain, and passes a focused presenter
test or reports the first unsupported host capability. No D3D8 model,
generated source, or NV2A emulation belongs in that gate.
