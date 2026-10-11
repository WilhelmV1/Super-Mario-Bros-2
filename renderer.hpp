//
//  renderer.hpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 03/03/26.
//
#pragma once

#include <array>

#include "../Platform/sdl_window.hpp"
#include "render_context.hpp"
#include "swap_chain.hpp"
#include "mtl_render_target.hpp"
#include "mtl_render_pipeline.hpp"

namespace core {
class Game;
}

namespace mtl {


class Renderer{
public:
    
    static constexpr std::uint8_t MAX_FRAMES_IN_FLIGHT = 3;
    
    Renderer(platform::Window& _window, mtl::Device& _device);
    
    ~Renderer();
    
    void init_residency_set(std::size_t initial_capacity = 16, std::string_view label = "Renderer Residency Set");
    
    template<class T>
    void register_resource(const T& resource){
        resource.make_resident(*residency_set);
        residency_dirt = true;
    }
    
    void register_texture(MTL::Texture* texture) {
        residency_set->addAllocation(texture);
    }
    
    void render(const RenderContext& context, core::Game& game);
    
    MTL::CommandBuffer* begin_renderer();
    
    void next_event_frame();
    
    void commit_residency();
    
    void end_renderer(MTL::CommandBuffer* command_buffer, MTL::Drawable* drawable, MTL::ResidencySet* residency);
    
    void set_residency(MTL::ResidencySet* residency);
    
    std::uint32_t get_frame_index() const;
    
private:
    
    //Metodos de la clase
    
    void init_command_buffers();
    void init_synchronization();
    void init_pipelines();
    void init_render_targets();
    
    //Attributos
    platform::Window& window;
    mtl::Device& device;
    
    MTL::CommandQueue* command_queue = nullptr;
    MTL::ResidencySet* residency_set = nullptr;
    
    std::unique_ptr<core::SwapChain> swap_chain;
    
    std::unique_ptr<mtl::RenderPipeline> render_pipeline;
    std::unique_ptr<mtl::RenderPipeline> ui_pipeline;
    std::unique_ptr<mtl::RenderPipeline> swap_chain_pipeline;
    
    //std::array<MTL4::CommandAllocator*, MAX_FRAMES_IN_FLIGHT> allocators;
    std::array<MTL::CommandBuffer*, MAX_FRAMES_IN_FLIGHT> command_buffers;
    
    //Synchronization
    MTL::SharedEvent* shared_event = nullptr;
    std::uint64_t frame_number = 0;
    std::uint32_t frame_index = 0;
    
    bool residency_dirt = true;
    
    
};


}
