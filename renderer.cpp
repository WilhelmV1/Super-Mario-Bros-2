//
//  renderer.cpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 03/03/26.
//

#include "renderer.hpp"

#include <memory>

#include "../Metal_Imp/mtl_utilities.hpp"


namespace mtl {

Renderer::Renderer(platform::Window& _window, mtl::Device& _device) : window(_window), device(_device){
    
    command_queue = device.get_device()->newCommandQueue();
    
    init_synchronization();
    init_command_buffers();
    
    swap_chain = std::make_unique<core::SwapChain>();
    
}
Renderer::~Renderer(){
   /* for (auto& allocator : allocators) {
        allocator->release();
    }*/
    for (auto& cmd_buffer : command_buffers) {
        cmd_buffer->release();
    }
}


void Renderer::init_residency_set(std::size_t initial_capacity, std::string_view label){
    
    MTL::ResidencySetDescriptor* descriptor = MTL::ResidencySetDescriptor::alloc()->init();
    NS::Error* error = nullptr;
    
    descriptor->setInitialCapacity(initial_capacity);
    descriptor->setLabel(to_ns_string(label));
    
    residency_set = device.get_device()->newResidencySet(descriptor, &error);
    
    if (!residency_set){
        throw std::runtime_error("Failed to create Residency Set " + std::string(error->localizedFailureReason()->utf8String()));
    }
    
    if (descriptor) descriptor->release();
    if (error) error->release();
}
void Renderer::commit_residency(){
    residency_set->commit();
}
std::uint32_t Renderer::get_frame_index() const {
    return frame_index;
}
/**
 *
 * @param residency  This residency must be the CA::MetalLayer ´s residency set or a custom residency
 *
 */

void Renderer::set_residency(MTL::ResidencySet *residency){
    assert(residency && "Residency Set is a nullptr");
    command_queue->addResidencySet(residency);
}

void Renderer::init_command_buffers(){
    
//    MTL4::CommandAllocatorDescriptor* descriptor = MTL4::CommandAllocatorDescriptor::alloc()->init();
  //  descriptor->setLabel(to_ns_string("Command Allocator"));
    //NS::Error* err;
    
    /*for (auto& allocator : allocators) {
        allocator = device.get_device()->newCommandAllocator(descriptor, &err);
        if (!allocator) {
            descriptor->release();
            err->release();
            throw std::runtime_error("Failed to create command allocator " + std::string(err->localizedFailureReason()->utf8String()));
        }
    }
    for (auto& command_buffer : command_buffers) {
        command_buffer = device.get_device()->newCommandBuffer();
        command_buffer->setLabel(to_ns_string("Command Buffer"));
    }*/
   // descriptor->release();
}
/**
 *  Esta funcion inicializa los eventos compartidos en Metal que se usaran en el renderer
 *
 */

void Renderer::init_synchronization(){
    
    shared_event = device.get_device()->newSharedEvent();
    shared_event->setSignaledValue(frame_number);
}
/**
 * Agrega un uno al frameNumber para poder trabajar correctamente con los shared events
 */
void Renderer::next_event_frame(){
    frame_number++;
    
    frame_index = frame_number % MAX_FRAMES_IN_FLIGHT;
    
    
    if (frame_number > MAX_FRAMES_IN_FLIGHT) {
        const std::uint64_t milisecons = 100;
        
        bool beforeTimeout = shared_event->waitUntilSignaledValue(frame_number - MAX_FRAMES_IN_FLIGHT, milisecons);
        
        if(!beforeTimeout){
            SDL_Log("No signal from frame %llu to shared event after %llums", frame_number -  MAX_FRAMES_IN_FLIGHT, milisecons);
        }
    }
}

/**
 * La funcion begin renderer creae un frameIndex el cual va intercalando desde 0 a 2 y con este accedemos al commanf buffer y el allocator del command Buffer
 *
 */

MTL::CommandBuffer* Renderer::begin_renderer(){
    
    if (residency_dirt) {
        residency_set->commit();
        residency_dirt = false;
    }
    
    //MTL::CommandBuffer* command_buffer = command_buffers[frame_index];
    //MTL::CommandAllocator* allocator = allocators[frame_index];
    //allocator->reset();
   // command_buffer->beginCommandBuffer(allocator);
    
    return command_queue->commandBuffer();
}

void Renderer::render(const RenderContext& context, core::Game& game) {
    
    
}

void Renderer::end_renderer(MTL::CommandBuffer* command_buffer, MTL::Drawable* drawable, MTL::ResidencySet* residency){
    
    command_buffer->useResidencySet(residency_set);
    command_buffer->encodeSignalEvent(shared_event, frame_number);
    command_buffer->presentDrawable(drawable);
    command_buffer->commit();
    
    //command_buffer->endCommandBuffer();
    
    //command_queue->wait(drawable);
    //command_queue->commit(&command_buffer, 1);
   
    //command_queue->signalDrawable(drawable);
    //drawable->present();
    //command_queue->signalEvent(shared_event, frame_number);
}

}
