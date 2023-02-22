#pragma once

/**
 * States that a buffer might be in
 */
enum class BufferState {
    Read,
    Write,
    TransferSource,
    TransferDestination,
};
