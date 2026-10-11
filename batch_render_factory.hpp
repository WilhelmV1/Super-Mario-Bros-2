//
//  batch_render_factory.hpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 29/07/26.
//
#pragma once

#include <memory>
#include "batch_render.hpp"


namespace core {

class BatchRenderFactory {
public:
    
    static std::unique_ptr<BatchRender> create(mtl::Device& device, std::uint32_t max_quads, mtl::Backend backend);
    
};

}
