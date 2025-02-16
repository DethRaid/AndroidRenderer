# Renderer overview

This project implements a hybrid deferred/forward renderer - deferred for opaque geometry, forward 
for transparent

All mesh data is stored in one large buffer

The renderer stores a list of renderable . Each renderable has a handle to the mesh, material, and render primitive data. At the beginning of the frame, the renderer updates these buffers with any new objects

We use CSM for shadows and LPVs for GI. Surfel GI is under development

The renderer is very GPU-driven, that's not likely to change. It uses HiZ occlusion culling on the GPU, then writes out a list of indirect draw commands, then renders those commands into a gbuffer. The visibility list can be re-used later, for example in a visualization pass

We use descriptors to bind material buffers and textures. This (hopefully) lets the GPU perform some
optimizations. Eventually I'll test it

## Shadow pass

The shadow pass renders various objects into a four-cascade shadowmap with RSM targets (flux + normal)

## RSM injection

Inject the RSM targets into the LPV as virtual point lights

We use subpasses to keep the RSM target in tiled memory. First subpass rasterizes the RSM targets, second subpass reads from them and pushes their VPLs to the appropriate LPV cells. We have to use a fragment shader in the second subpass because Vulkan doesn't support compute shaders in subpasses

We inject some real lights into the LPV as well. The renderer samples mesh lights when they're loaded and finds some emissive areas. It then generates a virtual point light for each of those samples. Then, every frame, we can inject a mesh light's VPLs into the LPV

## Surfel update

Each surfel will decide how many rays it needs. This is something of an abstract measure, so as long as every surfel calculates its ray request the same we won't have issues. Then, we scale the requested rays by (current ray budget / total requested rays). We send those rays off to sample the world

Each sample tests direct lighting, and reads from the surfels closest to the sample point. It can read from other information as needed - perhaps we want to use SDF tracing for the sky light? (Use mip levels to store the sky at different cone angles, use the SDF tracing to determine the cone angle through which the sky is visible). We'll update the surfel with the sample, updating its variance and whatnot

## HiZ culling pass

Render last frame's visible objects into a depth buffer. Cull every object against that depth buffer, keeping track of which ones are newly visible this frame. Draw the newly-visible ones into the depth buffer

## gbuffer

Render the scene into gbuffers

Base color (RGB)
Normals (RGB)
Data (Roughness = R, Metalness = G)

Depth test set to equals, depth writes off

## Lighting

The lighting pass computes contribution from the sun, including PCSS shadows. It also adds in 
lighting from the LPV (GI and emission) and from point lights. We'll use a clustered deferred
architecture

The gbuffer pass and ligthing pass must be subpasses in the same renderpass, to avoid flushing the 
gbuffers to main memory

We also apply the 

## Surfel spawning

We examine the depth buffer and the current surfel placement to decide where to place new surfels (if anywhere). We should bias towards surfels that have a higher overall variance

## Post-processing

I'll first just do tonemapping and color grading, to keep the data in tiled memory. Later we might 
add in bloom

## UI

We accomplish UI with Dear ImGUI cause yolo

UI is rendered at full res, the 3D scene might not be

The UI pass takes the output of the postprocessing and writes it to the backbuffer attachment
