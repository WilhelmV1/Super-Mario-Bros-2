//
//  animation_system.hpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 31/03/26.
//
#pragma once

#include <entt.hpp>
#include "Components/render_components.hpp"
#include "Components/physic_components.hpp"

namespace ecs {


class SpriteAnimationSystem{
public:
    
    
    void update(entt::registry& _register,  float delta_time, bool& phy_pause);
    
    
private:
    
    static constexpr int TILE_PADDING = 2;
    
    void check_current_state(PhysicsComponent& phy, AnimationComponent& anim);
    int animation(float* time, int frameCount, bool loop, bool& finished , int direction = 1.0f, float duration = 1.0f);
    void get_sprite(SpriteComponent& sprite, AnimationComponent& anim);
    void get_sprite(SpriteComponent& sprite, const AnimationData& data, int index);
    void get_palette(SpriteComponent& sprite, PaletteComponent& palette, bool& phy_pause);
};



}
