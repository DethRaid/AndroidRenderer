# Renderer overview

This project implements a hybrid deferred/forward renderer - deferred for opaque geometry, forward 
for transparent

All mesh data is stored in one large buffer

The renderer stores a couple lists of renderable objects, grouped by transparency mode. Each 
renderable has a handle to the mesh, material, and render primitive data. At the beginning of the 
frame lists of material parameters and primitive data are uploaded to buffers, when rendering we set 
push constants to select the appropriate primitive data

We use CSM for shadows and LPVs for GI

The renderer is very CPU-driven, that's not likely to change

We use descriptors to bind material buffers and textures. This (hopefully) lets the GPU perform some
optimizations. Eventually I'll test it

We pass the object's model matrix in the push constants. Material data is a descriptor set with a
UBO descriptor + four texture descriptors. Per-view data is a descriptor set with one UBO descriptor

## Shadow pass

The shadow pass renders various objects into a four-cascade shadowmap with RSM targets (flux + normal)

## RSM injection

Inject the RSM targets into the LPV as virtual point lights

We probably want to use a subpass somehow to keep the RSM target in tiled memory. Will solve later

We may want to inject some real lights into the LPV as well. Might be helpful to have a switch for
high-quality lights that get computed analytically, vs low-quality that get injected into the LPV

## Light/Cluster classification

Classify lights into clusters. Not entirely sure how - probably just a compute thread per cluster, 
then that one thread can add overlapping lights to a list

## gbuffer

Render the scene into gbuffers

Base color (RGB)
Normals (RGB)
Data (Roughness = R, Metalness = G)

## Lighting

The lighting pass computes contribution from the sun, including PCSS shadows. It also adds in 
lighting from the LPV (GI and emission) and from point lights. We'll use a clustered deferred
architecture

The gbuffer pass and ligthing pass must be subpasses in the same renderpass, to avoid flushing the 
gbuffers to main memory

## Post-processing

I'll first just do tonemapping and color grading, to keep the data in tiled memory. Later we might 
add in bloom

## UI

We accomplish UI with Dear ImGUI cause yolo

UI is rendered at full res, the 3D scene might not be

The UI pass takes the output of the postprocessing and writes it to the backbuffer attachment
