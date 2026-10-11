//
//  batch_rendering.cpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 28/03/26.
//

#include "batch_render.hpp"
#include "../Shaders/shader_types.hpp"
#include "../utility.hpp"
#include "../NES/palette.hpp"

namespace core {



BatchRender::BatchRender(mtl::Device& _device, std::uint32_t _max_quads, std::string_view filename) : device(_device), max_quads(_max_quads) {

    init_batching();
}


void BatchRender::init_batching(){
    
    init_buffers();
    init_textures();
    init_nes_system();
    
}
void BatchRender::init_buffers(){
    // Vertex/Index Buffers
    max_vertices = max_quads * 4;
    max_indices = max_quads * 6;
    
    vertex_size = max_vertices * sizeof(Vertex);
    
    index_size = max_indices * sizeof(std::uint32_t);
    
    for (auto& vertex_buffer : dynamic_vertex_buffers) {
        vertex_buffer = std::make_unique<mtl::Buffer>(device, vertex_size, MTL::StorageModeShared);
        vertex_buffer->set_label("Batching Vertex Buffer");
    }
    
    std::uint32_t offset = 0;
    
    std::vector<std::uint32_t> indices(max_indices);
    
    for (std::uint32_t i = 0; i < max_indices; i += 6) {
        
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;
        
        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;
        
        offset += 4;
        
    }
    
    index_buffer = std::make_unique<mtl::Buffer>(device, index_size, MTL::StorageModeShared);
    index_buffer->write_to_buffer(indices.data());
}

void BatchRender::init_nes_system(){
    
    std::vector<NES::Palette> palettes = {
        {NES::COLOR_01, NES::COLOR_30, NES::COLOR_27, NES::COLOR_0D}, // 0
        {NES::COLOR_30, NES::COLOR_06, NES::COLOR_01, NES::COLOR_0D}, // 1
        {NES::COLOR_0D, NES::COLOR_29, NES::COLOR_19, NES::COLOR_1D}, // 2
        {NES::COLOR_0D, NES::COLOR_30, NES::COLOR_06, NES::COLOR_1D}, // 3
        {NES::COLOR_0D, NES::COLOR_30, NES::COLOR_1D, NES::COLOR_01}, // 4
        {NES::COLOR_0D, NES::COLOR_30, NES::COLOR_1D, NES::COLOR_01}, // 5
        {NES::COLOR_19, NES::COLOR_29, NES::COLOR_19, NES::COLOR_09}, // 6
        {NES::COLOR_20, NES::COLOR_26, NES::COLOR_07, NES::COLOR_0D}, // 7
        {NES::COLOR_10, NES::COLOR_19, NES::COLOR_11, NES::COLOR_0D}, // 8
        {NES::COLOR_30, NES::COLOR_27, NES::COLOR_30, NES::COLOR_1D}, // 9
        {NES::COLOR_0D, NES::COLOR_1D, NES::COLOR_17, NES::COLOR_1D}, // 10
        {NES::COLOR_0D, NES::COLOR_1D, NES::COLOR_1D, NES::COLOR_1D}, // 11
        {NES::COLOR_1D, NES::COLOR_1D, NES::COLOR_1D, NES::COLOR_0D}, // 12
        {NES::COLOR_0D, NES::COLOR_30, NES::COLOR_25, NES::COLOR_1D}, // 13
        {NES::COLOR_0D, NES::COLOR_30, NES::COLOR_00, NES::COLOR_1D}, // 14
        {NES::COLOR_0D, NES::COLOR_27, NES::COLOR_17, NES::COLOR_1D}, // 15
    };
    
    std::size_t buffer_size = sizeof(NES::Palette) * palettes.size();
    
    nes_palette = std::make_unique<mtl::Buffer>(device, buffer_size, MTL::StorageModeShared);
   
    nes_palette->write_to_buffer(palettes.data());
    
    nes_palette->set_label("NES Palettes");
   
    buffer_size = sizeof(simd::float4) * (int)NES::COLOR_COUNT;
    
    nes_colors = std::make_unique<mtl::Buffer>(device, buffer_size,  MTL::StorageModeShared );
    
    nes_colors->write_to_buffer(NES::colors);
    
    nes_colors->set_label("NES Colors");
    
}

void BatchRender::init_textures(){
    //Textures
    
    game_textures = std::make_unique<mtl::ArrayTexture2D>();
    
    std::vector<std::string_view> paths = {
        "SMB2 Toad.png",
        "SMB2 Tiles.png",
        "SMB2 Items.png",
        "SMB2 Enemy.png",
        "SMB2 Animated Tiles.png"
    };
    
    game_textures->load_texture_from_files(paths, 256, 256, device, MTL::PixelFormatR8Uint, MTL::TextureUsageShaderRead);
    
    texture_size = game_textures->get_texture_size();
    
}

void BatchRender::push_vertex(simd::float3 position ,simd::float2 uv, uint palette_index, uint texture_index){
    assert(vertex_count < max_vertices);
    data.buffer->position = position;
    data.buffer->uv =  uv;
    data.buffer->palette_index = palette_index;
    data.buffer->texture_index = texture_index;
    data.buffer++;
}

void BatchRender::begin_batch(const FrameData& frame_data){
    
    assert(states == BatchStates::IDLE && "You can´t use begin bacth during the recording");
    
    states = BatchStates::RECORDING;
    
    this->frame_data = &frame_data;
    frame_index = this->frame_data->frame_index;
    
    data.current_buffer = dynamic_vertex_buffers[frame_index].get();
    
    data.buffer_start = data.current_buffer->map<Vertex>();
    
    data.buffer = data.buffer_start;
    counter = 0;
}



void BatchRender::reset(){
    index_count = 0;
    vertex_count = 0;
    data.buffer = data.buffer_start;
    counter = 0;
}

void BatchRender::flush(){
    
    if(!frame_data || !frame_data->encoder /*|| !frame_data->argument_table*/) return;
    if (index_count == 0) return;
    
    frame_data->ubo->bind(frame_data->encoder, 1, MTL::RenderStageVertex);
    frame_data->ubo->bind(frame_data->encoder, 1, MTL::RenderStageFragment);
    data.current_buffer->bind(frame_data->encoder, 0, MTL::RenderStageVertex);
    frame_data->encoder->setFragmentTexture(game_textures->get_texture(), 0);
    nes_palette->bind(frame_data->encoder, 2, MTL::RenderStageFragment);
    nes_colors->bind(frame_data->encoder, 3, MTL::RenderStageFragment);
    
    frame_data->encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, index_count, MTL::IndexTypeUInt32, index_buffer->get_buffer(), 0);
    
}

void BatchRender::present(){
    
    assert(states == BatchStates::RECORDING && "Error, you can´t present, please use Begin Batch before");
    
    
    flush();
    reset();
    
    states = BatchStates::IDLE;
    frame_data = nullptr;
    data.current_buffer = nullptr;
}

void BatchRender::draw_sprite(float x, float y , float z ,float w, float h, simd::float2 sprite_pos, simd::float2 sprite_size, uint palette_index, uint texture_index){
    
    assert(states == BatchStates::RECORDING && "Error, you can´t draw, pls use Begin Batch before");
    
    if (index_count + 6 > max_indices || vertex_count + 4 > max_vertices) {
        std::cout<<"Se hizo un flush\n";
        flush();
        reset();
    }
    
    simd::float3 position = simd_make_float3(x, y, z);
    simd::float2 size = simd::make_float2(w, h);
    
    simd::float2 uv_min = sprite_pos;
    simd::float2 uv_max = (sprite_pos + sprite_size);
    
    push_vertex(position, uv_min, palette_index, texture_index);
    
    push_vertex(simd_make_float3(position.x + size.x, position.y, position.z), simd_make_float2(uv_max.x, uv_min.y), palette_index, texture_index);
    
    push_vertex(simd_make_float3(position.x + size.x, position.y + size.y, position.z), uv_max, palette_index, texture_index);
    
    push_vertex(simd_make_float3(position.x, position.y + size.y, position.z), simd_make_float2(uv_min.x, uv_max.y), palette_index, texture_index);
    
    vertex_count += 4;
    index_count += 6;
    counter += 1;
}

void BatchRender::make_resident(MTL::ResidencySet& residency) const {
    index_buffer->make_resident(residency);
    
    for (auto& vertex_buffer : dynamic_vertex_buffers) {
        vertex_buffer->make_resident(residency);
    }
    game_textures->make_resident(residency);
    
    nes_colors->make_resident(residency);
    nes_palette->make_resident(residency);
}

}

