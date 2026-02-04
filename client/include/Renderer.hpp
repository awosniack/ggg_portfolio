#pragma once

namespace inventory {

class Renderer {
public:
    Renderer();
    ~Renderer();
    
    bool initialize(int width, int height, const char* title);
    void shutdown();
    
    void beginFrame();
    void endFrame();
    
    bool shouldClose() const;
    
private:
    bool initialized_;
};

} // namespace inventory
