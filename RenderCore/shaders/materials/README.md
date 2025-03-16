# Materials

This folder contains all of SAH's materials

A Material is essentially a unique shader. Each material may have many instances, e.g. different parameters. It may also be used in many different contexts

## Contexts

We have a few contexts, controlled with defines:

### Depth prepass

```cpp
#define SAH_DEPTH_ONLY 1
#define SAH_MAIN_VIEW 1
```

The depth prepass rasterizes a depth buffer from a viewpoint. It only renders one view

### Shadowmaps

```cpp
#define SAH_DEPTH_ONLY 1
#define SAH_CSM 1
#define SAH_MULTIVIEW 1
```

The shadowmaps render a depth buffer from the directional light's point of view. We use multiview rendering to rasterize every shadow cascade at once

### Reflective Shadow Maps

```cpp
#define SAH_THIN_GBUFFER 1
#define SAH_RSM 1
#define SAH_MULTIVIEW 1
```

Reflective shadow maps rasterize a thin gbuffer from the light's point of view. They need base color and normals, but ignore data and emission. We do not use multiview rendering (but we should tbh)

### Geometry Buffer

```cpp
#define SAH_GBUFFER 1
#define SAH_MAIN_VIEW 1
```

Geometry buffers store BRDF inputs - base color, surface normal, roughness, metalness, and emission. We render one for the main view

### Ray Traced Occlusion

```cpp
#define SAH_RT 1
#define SAH_RT_OCCLUSION 1
```

Ray traced occlusion uses rays to determine if anything lies in a given direction. We use it for ray traced shadows in RTAO and RTGI

### Ray Traced Global Illumination

```cpp
#define SAH_RT 1
#define SAH_RT_GI 1
```
Ray traced global illumination is the GOAT

## Variants

We also have masked vs not-masked variants for most of these contexts

```cpp
#define SAH_MASKED 1
```

Masked materials perform an alpha-test and may discard a fragment, non-masked may not
