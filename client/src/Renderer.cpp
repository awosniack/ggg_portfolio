#include "Renderer.hpp"

namespace inventory {

Renderer::Renderer() : initialized_(false) {
}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::initialize(int width, int height, const char* title) {
    initialized_ = true;
    return true;
}

void Renderer::shutdown() {
    initialized_ = false;
}

void Renderer::beginFrame() {
}

void Renderer::endFrame() {
}

bool Renderer::shouldClose() const {
    return false;
}

} // namespace inventory
