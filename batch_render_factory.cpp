//
//  batch_render_factory.cpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 29/07/26.
//

#include "batch_render_factory.hpp"

#include "mtl3_batch_render.hpp"
#include "mtl4_batch_render.hpp"

namespace core {
/*
std::unique_ptr<BatchRender> BatchRenderFactory::create(mtl::Device &device, std::uint32_t max_quads, mtl::Backend backend) {
    if (backend == mtl::Backend::Metal4) {
        return std::make_unique<MTL4BatchRender> (device, max_quads);
    } else {
        return std::make_unique<MTLBatchRender> (device, max_quads);
    }
}
*/
}
