#include "render_doc_wrapper.hpp"

#include "core/system_interface.hpp"

RenderDocWrapper::RenderDocWrapper(RENDERDOC_API_1_1_2* api_in) : api{api_in} {
}

void RenderDocWrapper::begin_capture() const {
    if(api) {
        api->StartFrameCapture(nullptr, nullptr);
    }
}

void RenderDocWrapper::end_capture() const {
    if(api) {
        api->EndFrameCapture(nullptr, nullptr);
    }
}
