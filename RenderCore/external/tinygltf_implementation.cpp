#if defined(__ANDROID__)
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#endif
#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf.h>

#if defined(__ANDROID__)
void set_tinygltf_asset_manager(AAssetManager* asset_manager) {
    tinygltf::asset_manager = asset_manager;
}
#endif
