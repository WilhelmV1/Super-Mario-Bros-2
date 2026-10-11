//
//  animation_system.cpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 31/03/26.
//

#include "animation_system.hpp"


namespace ecs {


void SpriteAnimationSystem::update(entt::registry & _register, float delta_time, bool& phy_pause){
    auto view = _register.view<SpriteComponent, AnimationComponent>();

    for (auto entity : view) {
        auto& sprite = view.get<SpriteComponent>(entity);
        auto& anim = view.get<AnimationComponent>(entity);
      
        if (anim.set == nullptr) continue;
        
        auto& anim_data = anim.set->anim[(int)anim.state];
        
        anim.timer += delta_time;
    
        anim.index = animation(&anim.timer, (int)anim_data.frames.size(), anim_data.loop_animation, anim.is_in_animation ,anim.direction ,anim_data.frame_duration);
        
        if (_register.any_of<PaletteComponent>(entity)) {
            auto& palette = _register.get<PaletteComponent>(entity);
            
            palette.timer += delta_time;
            palette.total_timer += delta_time;
            
            if (palette.can_do_anim) {
                get_palette(sprite,palette, phy_pause);
            } else {
                //phy_pause = false;
            }
        }
        
        get_sprite(sprite, anim_data, anim.index);
        
        if (_register.any_of<TinyComponent>(entity)){
            sprite.atlas_offset.y = 0;
        }
    }
    
}


int SpriteAnimationSystem::animation(float *time, int frameCount, bool loop, bool& finished, int direction, float duration){
   
    if (loop) {
        while (*time >= duration) {
            *time -= duration;
        }
    }else{
        if (*time >= duration) {
            *time = duration;
            finished = false;
        }
    }
        
    int index;
    
    if (direction == 1) {
        index =  (int)((*time / duration) * frameCount);
    } else {
       index = frameCount - 1 - (int)((*time / duration) * frameCount);
    }
    
    
    if (index >= frameCount) {
        index = frameCount - 1;
    }
    if (index <= 0) {
        index = 0;
    }
    
    return index;
    
}

void SpriteAnimationSystem::get_sprite(SpriteComponent &sprite, const AnimationData &data, int index){
   
    const auto& frame = data.frames[index];
    
    sprite.atlas_offset.x = frame.offset.x;
    sprite.atlas_offset.y = frame.offset.y;
    
    sprite.size = data.size;
    
    if (data.flip) {
        if (index % 2 == 0) {
            sprite.atlas_offset.x += sprite.size.x;
            sprite.size.x *= -1.0f;
        }
    }
}

void SpriteAnimationSystem::get_palette(SpriteComponent &sprite, PaletteComponent &palette, bool& phy_pause){
    if (palette.total_timer > palette.total_anim_time) {
        palette.can_do_anim = false;
        sprite.index_palette = sprite.default_palette;
        return;
    }
    if (palette.timer > palette.frame_change_duration) {
        sprite.index_palette = palette.palettes[palette.index];
        palette.index = (palette.index + 1) % MAX_FRAMES_IN_ANIMATION;
        palette.timer = 0.0f;
    }
}

}
