#include "dummy_camera.hpp"

namespace auroraos {
namespace camera {

DummyCamera::DummyCamera() 
    : width_(0), height_(0), format_(PixelFormat::RGB565), 
      capturing_(false), frame_count_(0), frame_surface_(nullptr), 
      callback_(nullptr), user_data_(nullptr) {}

DummyCamera::~DummyCamera() {
    if (frame_surface_) {
        delete frame_surface_;
    }
}

bool DummyCamera::init(uint16_t width, uint16_t height, PixelFormat fmt) {
    if (capturing_) return false;
    
    width_ = width;
    height_ = height;
    format_ = fmt;
    
    if (frame_surface_) {
        delete frame_surface_;
    }
    
    // In a real system, we'd ensure continuous memory for DMA.
    frame_surface_ = new gpu::Surface(width, height);
    return true;
}

void DummyCamera::set_frame_callback(FrameCallback callback, void* user_data) {
    callback_ = callback;
    user_data_ = user_data;
}

void DummyCamera::start_capture() {
    if (!frame_surface_) return;
    capturing_ = true;
    frame_count_ = 0;
}

void DummyCamera::stop_capture() {
    capturing_ = false;
}

void DummyCamera::simulate_frame_arrival() {
    if (!capturing_ || !frame_surface_ || !callback_) return;
    
    // Generate a simple moving pattern to simulate video stream
    uint16_t* buf = static_cast<uint16_t*>(frame_surface_->get_buffer());
    uint32_t total = width_ * height_;
    
    // Base color shifts with frame_count_
    uint16_t color = (frame_count_ << 11) | ((frame_count_ & 0x3F) << 5) | (frame_count_ & 0x1F);
    
    for (uint32_t i = 0; i < total; ++i) {
        buf[i] = color; // Fill solid changing color
    }
    
    // Draw a moving bar
    uint32_t bar_y = (frame_count_ * 5) % height_;
    for (uint32_t r = bar_y; r < bar_y + 10 && r < height_; ++r) {
        for (uint32_t c = 0; c < width_; ++c) {
            buf[r * width_ + c] = 0xFFFF; // White bar
        }
    }
    
    frame_count_++;
    
    // Trigger the callback (simulating hardware DMA transfer complete interrupt)
    callback_(frame_surface_, user_data_);
}

} // namespace camera
} // namespace auroraos
