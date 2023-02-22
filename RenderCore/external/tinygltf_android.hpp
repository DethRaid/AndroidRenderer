#pragma once

#if defined(__ANDROID__)
struct AAssetManager;

void set_tinygltf_asset_manager(AAssetManager* asset_manager);
#endif
