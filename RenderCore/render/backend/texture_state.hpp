#pragma once

/**
 * One of the states a render target may be in
 */
enum class TextureState {
    /**
     * The color target will be rendered to in this pass
     */
    ColorWrite,

    /**
     * The depth target will be read from and written to in this pass
     */
    DepthReadWrite,

    /**
     * The color or depth target will be read from in this pass. The render graph will hint to the hardware that it may
     * keep the data for the render target in on-chip memory. You may only read from the current pixel
     */
    InputAttachment,

    /**
     * The color target may be read from at any arbitrary location
     */
    ColorRead,

    /**
     * The depth target may be read from at any arbitrary location
     */
    DepthRead,

    VertexShaderRead,
    FragmentShaderRead,
    ShaderWrite,
    TransferSource,
    TransferDestination,
};
