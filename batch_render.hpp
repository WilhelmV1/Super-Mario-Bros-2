//
//  batch_rendering.hpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 28/03/26.
//
#pragma once
#include <entt.hpp>
#include "mtl_buffer.hpp"
#include "mtl_texture.hpp"
#include "../Shaders/shader_types.hpp"


namespace core {

enum class BatchStates{
    IDLE,
    RECORDING
};

struct BatchData{
    Vertex* buffer = nullptr;
    Vertex* buffer_start = nullptr;
    mtl::Buffer* current_buffer = nullptr;
    
};

struct FrameData;

class BatchRender{
public:
    
    BatchRender(mtl::Device& device, std::uint32_t max_quads, std::string_view filename = "");
    
    void make_resident(MTL::ResidencySet& residency) const ;
    
    // Methos for rendering
    
    
    
    void begin_batch(const FrameData& frame_data);
    
    void present();
    
    void reset();
    
    void flush();
    
    //void end_batch(); in the future, we will use this 

    
    void draw_rect(float x, float y, float w, float h);
    
    void draw_sprite(float x, float y, float z, float w, float h, simd::float2 sprite_pos, simd::float2 sprite_size, uint palette_index, uint texture_index);

    
    
    
    
    
private:
    
    
    
    // Methods
    
    void init_batching();
    void init_buffers();
    void init_textures();
    
    void init_nes_system();
    
    void push_vertex(simd::float3 position ,simd::float2 uv, uint palette_index, uint texture_index);
    
    
    //Attributes
    
    static constexpr std::uint8_t MAX_FRAMES_IN_FLIGHT = 3;
    std::array<std::unique_ptr<mtl::Buffer>, MAX_FRAMES_IN_FLIGHT> dynamic_vertex_buffers;
    std::unique_ptr<mtl::Buffer> nes_palette{nullptr}, nes_colors{nullptr};
    std::unique_ptr<mtl::Buffer> index_buffer = nullptr;
    std::unique_ptr<mtl::ArrayTexture2D> game_textures = nullptr;
    mtl::Device& device;
    std::uint32_t max_quads = 0, max_vertices = 0, max_indices = 0;
    std::size_t vertex_size = 0, index_size = 0;
    std::size_t index_count = 0, vertex_count = 0;
    std::uint32_t frame_index{0};
    simd::float2 texture_size = {0,0};
    std::string_view path;
    int counter = 0;
    
    BatchData data;
    BatchStates states = BatchStates::IDLE;
    
    const FrameData* frame_data;
};


}
