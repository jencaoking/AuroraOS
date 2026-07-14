#include "../drivers/camera/dummy_camera.hpp"
#include "../guix/window.hpp"
#include "../guix/compositor.hpp"
#include "../drivers/gpu/gpu_device.hpp"

namespace auroraos {
namespace apps {

class CameraApp {
public:
    CameraApp(guix::Compositor* compositor, gpu::GpuDevice* gpu) 
        : compositor_(compositor), gpu_(gpu) {
        
        // Create a 240x240 window for the camera viewfinder
        window_ = new guix::Window(240, 240, gpu_, compositor_);
        window_->move(50, 50);
        window_->set_z_order(10);
        
        // Initialize camera to match window resolution
        camera_.init(240, 240, camera::PixelFormat::RGB565);
        
        // Register static callback, passing 'this' as user_data
        camera_.set_frame_callback(on_frame_arrived_static, this);
        
        // Start streaming
        camera_.start_capture();
    }

    ~CameraApp() {
        camera_.stop_capture();
        delete window_;
    }

    // This simulates the main loop spinning (if the camera is dummy)
    void tick() {
        camera_.simulate_frame_arrival();
    }

private:
    static void on_frame_arrived_static(gpu::Surface* frame, void* user_data) {
        auto* app = static_cast<CameraApp*>(user_data);
        app->on_frame_arrived(frame);
    }
    
    void on_frame_arrived(gpu::Surface* frame) {
        if (!frame || !window_) return;
        
        // The camera hardware has DMA'd the new frame into 'frame' Surface.
        // We now use the GPU driver to Blit this frame onto our Window's backing store.
        gpu::GpuCommand blit;
        blit.opcode = gpu::GpuOpcode::Blit;
        blit.dst_surface = window_->get_surface();
        blit.dst_x = 0;
        blit.dst_y = 0;
        blit.width = 240;
        blit.height = 240;
        blit.args.blit.src_surface = frame;
        blit.args.blit.src_x = 0;
        blit.args.blit.src_y = 0;
        
        gpu_->submit(&blit, 1);
        
        // Notify the compositor that our window is dirty and needs to be re-composited to the screen
        window_->invalidate();
    }

    guix::Compositor* compositor_;
    gpu::GpuDevice* gpu_;
    guix::Window* window_;
    camera::DummyCamera camera_;
};

} // namespace apps
} // namespace auroraos
