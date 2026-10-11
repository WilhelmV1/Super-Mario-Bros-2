//
//  physic_system.cpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 01/04/26.
//

#include "physic_system.hpp"
#include "Components/physic_components.hpp"
#include "../Game/Levels/level.hpp"
#include <iostream>
#include "../Game/physic_engine.hpp"
#include "Components/controller_components.hpp"
#include "Components/render_components.hpp"
#include "Components/objects_components.hpp"
#include "Components/boss_components.hpp"
#include "../Game/resource_manager.hpp"
#include "../Game/game_info.hpp"
#include <format>

namespace ecs {


void PhysicsSystem::update(entt::registry &registry, float delta_time, core::Level& level, bool& phy_pause){
    this->phy_pause = &phy_pause;
    
    item_collision(registry, delta_time, level);
    
    solid_collision(registry, delta_time, level);
    
    enemy_projectile_move(registry, delta_time, level);
    
    bounce_projectile(registry);
    
    player_collision(registry, delta_time, level);
}

TileCollisionResult PhysicsSystem::find_colliding_tile(const core::TileMap &tile_map, simd::float2 position, simd::int4 rect , bool x_axis, bool ignore_one_way) {
    
    simd::int2 entity_grid_pos = tile_map.get_grid_pos((int)position.x, (int)position.y);
    for (int x = entity_grid_pos.x - 1; x <= entity_grid_pos.x + 1; x++) {
        for (int y = entity_grid_pos.y - 2; y <= entity_grid_pos.y + 2; y++) {
            core::Tile tile = tile_map.get_tile(x, y);
            
            std::uint8_t index = tile_map.get_tile_index(x, y);
            if (tile.type == 0 || tile.type == core::CLIMBABLE || index >= 210) continue;
            
            bool coll_side = x_axis ? (tile.collision_side & core::RIGHT || tile.collision_side & core::LEFT) : (tile.collision_side & core::UP || tile.collision_side & core::DOWN);
            bool one_way = ignore_one_way ? false : tile.type & core::ONE_WAY;
            simd::int4 tile_rect = tile_map.get_tile_rect(x, y);
            
            if (check_collision(rect, tile_rect) && coll_side && !one_way) {
                return TileCollisionResult{.tile_rect = tile_rect, .collision_side = tile.collision_side, .tile_type = tile.type};
            }
        }
    }
    return TileCollisionResult{.collision_side = core::NONE, .tile_type = core::AIR};
}

bool PhysicsSystem::is_entity_valid(entt::registry &registry, entt::entity entity, const core::Level& level) {
    auto& phy = registry.get<PhysicsComponent>(entity);
    auto& location = registry.get<LocationComponent>(entity);
    
    if (!registry.any_of<PowerUpComponent>(entity)) {
        if (*phy_pause) return false;
    }
    if (level.get_current_zone() == core::LevelZone::SUBSPACE) {
        if (location.location & SUBSPACE) {}
        else if (location.location & EntityLocation::OVERWORLD) {
            phy.velocity.y = 0;
            return false;
        }
    } else if (level.get_current_zone() == core::LevelZone::OVERWORLD){
        if (location.location & OVERWORLD) {}
        else if (location.location & EntityLocation::SUBSPACE) {
            phy.velocity.y = 0;
            return false;
        }
    }
    return true;
}

void PhysicsSystem::apply_gravity(entt::registry &registry, entt::entity entity, float delta_time) {
    auto& phy = registry.get<PhysicsComponent>(entity);
    if (phy.apply_gravity) phy.velocity.y = approach(phy.velocity.y, fall_speed, gravity * delta_time);
}

void PhysicsSystem::reset_entity(entt::registry &registry, entt::entity entity, bool reset_block_tile) {
    auto& phy = registry.get<PhysicsComponent>(entity);
    auto& coll = registry.get<CollisionComponent>(entity);
    
    phy.is_on_ground = false;
    coll.result_collision = NONE;
    if (reset_block_tile) coll.block_type = AIR;
}

void PhysicsSystem::move_x(entt::registry &registry, entt::entity entity, const core::TileMap& tile_map, bool is_a_solid) {
    auto&& [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
    
    simd::int4 rect = make_collision_rect(transform, coll, phy);
    if (is_a_solid) rect.xy -= 1;
    phy.remainder.x += phy.velocity.x;
    int move_x = round(phy.remainder.x);
    
    if (move_x != 0) {
        phy.remainder.x -= move_x;
        int move_sign = sign(move_x);
        while (move_x) {
            rect.x += move_sign;
            if (is_a_solid) carry_x_actor(registry, entity, tile_map, phy, rect, move_sign);
            bool can_move = check_solid_x_tile_collision(registry, entity, tile_map, transform, coll, rect ,phy, move_sign);
            if (can_move) {
                transform.position.x += move_sign;
                move_x -= move_sign;
            }
            else break;
        }
    }
}

void PhysicsSystem::carry_x_actor(entt::registry &registry, entt::entity entity, const core::TileMap &tile_map, PhysicsComponent& phy ,simd::int4& rect, int move) {
    
    for (auto actor : registry.view<PhysicsComponent, TransformComponent, CollisionComponent>(entt::exclude<SolidComponent,ProjectileComponent,DoorComponent>)) {
        if (actor == entity) continue;
        auto [aPhy, aTrans, aColl] = registry.get<PhysicsComponent, TransformComponent, CollisionComponent>(actor);
        
        simd::int4 actor_rect = make_collision_rect(aTrans, aColl, aPhy, registry.any_of<TinyComponent>(actor));
        aPhy.solid_velocity = {0.0f, 0.0f};
        
        if (!check_collision(rect, actor_rect)) continue;
        if (!(std::abs(actor_rect.y + actor_rect.w - rect.y) <= 2) || registry.any_of<ProjectileComponent>(entity)) continue;;
        if (!aColl.can_collide_with_world) continue;
        
        aPhy.solid_velocity.x = phy.velocity.x * move;
        actor_rect.x += move;
        if (find_colliding_tile(tile_map, aTrans.position.xy, actor_rect, true, false).tile_type == AIR) aTrans.position.x += move;
    }
}

bool PhysicsSystem::check_solid_x_tile_collision(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, TransformComponent &transform, CollisionComponent &coll, simd::int4& rect, PhysicsComponent &phy, int move) {
    
    TileCollisionResult result = find_colliding_tile(tile_map, transform.position.xy, rect, true, true);
    if (result.tile_type == AIR || !coll.can_collide_with_world) return true;
     
    if (result.collision_side & core::RIGHT) {
        phy.velocity.x = 0;
        phy.remainder.x = 0.0f;
        coll.result_collision = RIGHT;
        return false;
    }
    if (result.collision_side & core::LEFT) {
        phy.velocity.x = 0;
        phy.remainder.x = 0.0f;
        coll.result_collision = LEFT;
        return false;
    }
    return true;
}

void PhysicsSystem::move_y(entt::registry &registry, entt::entity entity, const core::TileMap &tile_map, bool is_a_solid) {
    auto&& [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
    
    coll.rect = make_collision_rect(transform, coll, phy);
    if (is_a_solid) coll.rect.xy -= 1;
    phy.remainder.y += phy.velocity.y;
    int move_y = round(phy.remainder.y);
    
    if (move_y != 0) {
        phy.remainder.y -= move_y;
        int move_sign = sign(move_y);
        while (move_y) {
            coll.rect.y += move_sign;
            if (is_a_solid) carry_y_actor(registry, entity, tile_map, phy, coll, move_sign);
            bool can_move = check_solid_y_tile_collision(registry, entity, tile_map, transform, coll, phy, move_sign);
            if (can_move) {
                transform.position.y += move_sign;
                move_y -= move_sign;
            }
            else break;
        }
    }
}

void PhysicsSystem::carry_y_actor(entt::registry &registry, entt::entity entity, const core::TileMap &tile_map, PhysicsComponent &phy, CollisionComponent &coll, int move) {
    
    for (auto actor : registry.view<PhysicsComponent, TransformComponent, CollisionComponent>(entt::exclude<SolidComponent,ProjectileComponent,DoorComponent>)) {
        if (actor == entity) continue;
        auto [aPhy, aTrans, aColl] = registry.get<PhysicsComponent, TransformComponent, CollisionComponent>(actor);
        
        aColl.rect = make_collision_rect(aTrans, aColl, aPhy, registry.any_of<TinyComponent>(actor));
        aPhy.solid_velocity = {0.0f, 0.0f};
        aPhy.apply_gravity = true;
        
        if (!check_collision(coll.rect, aColl.rect)) continue;
        if (!(std::abs(aColl.rect.y + aColl.rect.w - coll.rect.y) <= 2) || registry.any_of<ProjectileComponent>(entity)) continue;
        if (!coll.can_collide_with_world) continue;
        
        aColl.rect.y += move;
        aPhy.solid_velocity.y = phy.velocity.y * move;
        if (find_colliding_tile(tile_map, aTrans.position.xy, aColl.rect, false, false).tile_type == AIR) {
            aTrans.position.y += move;
            aPhy.velocity.y = 0;
            aPhy.apply_gravity = false;
        }
    }
}


bool PhysicsSystem::check_solid_y_tile_collision(entt::registry &registry, entt::entity entity, const core::TileMap &tile_map, TransformComponent &transform, CollisionComponent &coll, PhysicsComponent &phy, int move) {
    
    TileCollisionResult result = find_colliding_tile(tile_map, transform.position.xy, coll.rect, false, true);
    
    if (result.tile_type == AIR || !coll.can_collide_with_world) return true;
    
    if(phy.velocity.y < 0.0f && (result.collision_side & core::DOWN)){
        phy.velocity.y = 0;
        phy.is_jumping = false;
        return false;
    }
    if (phy.velocity.y > 0.0f && (result.collision_side & core::UP)){
        int previous_feet_pos = (coll.rect.y + coll.rect.w) - move;
        
        if (previous_feet_pos <= result.tile_rect.y) {
            phy.is_on_ground = true;
            phy.is_jumping = false;
            phy.velocity.y = 0;
            coll.result_collision = UP;
            return false;
        }
    }
    return true;
}

void PhysicsSystem::manage_boss_projectiles(entt::registry &registry, core::Level &level) {
    for (auto entity : registry.view<EnemyProjectile>()) {
        auto [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
        auto zone = level.get_current_level_zone();
        if (registry.any_of<EnemyProjectile>(entity)) {
            if (transform.position.x <= zone.min_x * 16.0f || transform.position.x >= zone.max_x * 16.0f || coll.result_collision != NONE) {
                coll.can_collide_with_world = false;
                coll.can_collide_with_entity = false;
                phy.apply_gravity = true;
                phy.velocity.x = 0.0f;
                phy.remainder.x = 0.0f;
            }
            if (transform.position.y >= zone.max_y * 16.0f) {
                registry.destroy(entity);
            }
        }
    }
}

void PhysicsSystem::projectile_solid_collision(entt::registry &registry, core::ResourceManager& manager) {
    for (auto entity : registry.view<EnemyComponent>(entt::exclude<BossComponent>)) {
        auto [phy, coll, trans, enemy] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, EnemyComponent>(entity);
        enemy_interaction(registry, entity, trans, coll, phy, enemy, manager);
        item_projectile_interaction(registry, entity, trans, coll, phy, enemy, manager);
    }
    for (auto entity : registry.view<BossComponent>()) {
        auto [phy, coll, trans, boss] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, BossComponent>(entity);
        boss_interaction(registry, entity, trans, coll, phy, boss, manager);
    }
}

void PhysicsSystem::enemy_interaction(entt::registry &registry, entt::entity entity, TransformComponent &transform, CollisionComponent &coll, PhysicsComponent &phy, EnemyComponent &enemy, core::ResourceManager& manager) {
    for (auto projectile : registry.view<EnemyComponent, ProjectileComponent>()) {
        if (entity == projectile) continue;
        auto [pPhy, pColl, pTrans, pEnemy] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, EnemyComponent>(projectile);
        if (interaction(transform, coll, phy, pTrans, pColl, pPhy)) {
            pEnemy.is_colliding_with_projective = true;
            enemy.is_colliding_with_projective = true;
            manager.play_audio("COLLIDE SOUND");
            pPhy.velocity.y = -1.0f;
        }
    }
}

void PhysicsSystem::item_projectile_interaction(entt::registry &registry, entt::entity entity, TransformComponent &transform, CollisionComponent &coll, PhysicsComponent &phy, EnemyComponent &enemy, core::ResourceManager &manager) {
    for (auto projectile : registry.view<ProjectileComponent>(entt::exclude<EnemyComponent>)) {
        if (entity == projectile) continue;
        auto [pPhy, pColl, pTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(projectile);
        if (interaction(transform, coll, phy, pTrans, pColl, pPhy)) {
            enemy.is_colliding_with_projective = true;
            manager.play_audio("COLLIDE SOUND");
            pPhy.velocity.y = -1.0f;
        }
    }
    
}

void PhysicsSystem::boss_interaction(entt::registry &registry, entt::entity entity, TransformComponent &transform, CollisionComponent &coll, PhysicsComponent &phy, BossComponent &boss, core::ResourceManager &manager) {
    for (auto projectile : registry.view<ProjectileComponent>()) {
        if (entity == projectile) continue;
        auto [pPhy, pColl, pTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(projectile);
        if (interaction(transform, coll, phy, pTrans, pColl, pPhy) && boss.state != BossState::HURT) {
            boss.get_damage = true;
        }
    }
}

bool PhysicsSystem::interaction(TransformComponent &eTrans, CollisionComponent &eColl, PhysicsComponent &ePhy, TransformComponent &pTrans, CollisionComponent &pColl, PhysicsComponent &pPhy) {
    
    eColl.rect = make_collision_rect(eTrans, eColl, ePhy);
    pColl.rect = make_collision_rect(pTrans, pColl, pPhy);
    if (check_collision(eColl.rect, pColl.rect) && eColl.can_collide_with_entity && pColl.can_collide_with_entity) {
        return true;
    }
    return false;
}

void PhysicsSystem::solid_move(entt::registry &registry, core::Level& level, float delta_time) {
    
    core::TileMap& tile_map = level.get_map();
    for (auto solid : registry.view<CollisionComponent, TransformComponent, PhysicsComponent, SolidComponent, LocationComponent>()) {
        
        if (!is_entity_valid(registry, solid, level)) continue;

        reset_entity(registry, solid, true);
        move_x(registry, solid, tile_map, true);
        apply_gravity(registry, solid, delta_time);
        move_y(registry, solid, tile_map, true);
    }
}

void PhysicsSystem::solid_collision(entt::registry &registry, float delta_time, core::Level &level) {
    
    auto& resource = registry.ctx().get<core::ResourceManager>();
    
    solid_move(registry, level, delta_time);
    manage_boss_projectiles(registry, level);
    projectile_solid_collision(registry, resource);
    
}

bool PhysicsSystem::is_player_valid(entt::registry &registry, entt::entity player, const core::GameData& game_data) {
    auto& phy = registry.get<PhysicsComponent>(player);
    if (phy.physic_current_state == PhysicState::GRAB_START || phy.physic_current_state == PhysicState::POWER_UP || phy.physic_current_state == PhysicState::STATIC) return false;
    if (game_data.level_state == core::LevelState::CLIMB_ANIMATION || game_data.level_state == core::LevelState::FINISH_LEVEL) return false;
    if (*phy_pause) return false;
    return true;
}

void PhysicsSystem::players_vs_entity(entt::registry &registry, entt::entity player, const core::GameData& game_data) {
    
    auto [phy, coll, trans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
        
    coll.rect = make_collision_rect(trans, coll, phy, registry.any_of<TinyComponent>(player));
        
    for (auto entity : registry.view<CollisionComponent, TransformComponent, PhysicsComponent, SolidComponent, LocationComponent>(entt::exclude<PowerUpComponent, ItemComponent, CrystalBallComponent, GrabbedComponent, ProjectileComponent>)) {
        auto [ePhy, eColl, eTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
        eColl.rect = make_collision_rect(eTrans, eColl, ePhy);
        eColl.rect.xy -= 1;
        
        if (check_collision(coll.rect, eColl.rect)) {
            if (!(std::abs(coll.rect.y + coll.rect.w - eColl.rect.y) <= 2)) {
                if (registry.get<PaletteComponent>(player).can_do_anim) continue;
                    
                if (registry.any_of<InterectionComponent>(player) && coll.can_collide_with_entity && game_data.game_state != core::GameState::PAUSE_LEVEL) {
                    registry.get<InterectionComponent>(player).get_damage = true;
                    phy.velocity.x = (ePhy.is_facing_right ? 1 : -1) * 3.0f;
                }
            }
        }
    }
}

void PhysicsSystem::move_player_x(entt::registry &registry, entt::entity player, core::Level &level, bool is_tiny) {
    
    auto [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    simd::int4 rect = make_collision_rect(transform, coll, phy, is_tiny);
    phy.remainder.x += phy.velocity.x;
    int move_x = round(phy.remainder.x);
    
    if (move_x != 0) {
        phy.remainder.x -= move_x;
        int move_sign = sign(move_x);
        while (move_x) {
            rect.x += move_sign;
            bool can_move = !player_vs_solid(registry, player, rect ,true) && check_player_x_tile_collision(registry, player, level.get_map(), transform, coll, rect, phy, move_sign);
            if (can_move) {
                transform.position.x += move_sign;
                move_x -= move_sign;
            }
            else break;
        }
    }
}

bool PhysicsSystem::player_vs_solid(entt::registry &registry, entt::entity player, simd::int4& rect ,bool x_axis) {
    auto [phy, coll, trans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    for (auto solid : registry.view<SolidComponent>()) {
        if (solid == player) continue;
        auto [sPhy, sColl, sTrans, sSolid] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, SolidComponent>(solid);
        
        simd::int4 solid_rect = make_collision_rect(sTrans, sColl, sPhy);
        int feed_pos = rect.y + rect.w;
        bool is_on_top = feed_pos <= solid_rect.y;
        bool is_enemy = registry.any_of<EnemyComponent>(solid);
        bool enemy_can_jump = is_enemy ? registry.get<EnemyComponent>(solid).can_jump : is_enemy;
        
        if (check_collision(solid_rect, rect) && sColl.can_collide_with_entity) {
            if (!x_axis && !is_on_top) continue;
            return handle_player_vs_solid_collision(phy, sSolid, x_axis, is_enemy , enemy_can_jump);
        }
    }
    return false;
}

bool PhysicsSystem::handle_player_vs_solid_collision(PhysicsComponent &phy, SolidComponent &solid, bool x_axis, bool is_enemy, bool enemy_can_jump) {
    if (solid.x_collision && x_axis) {
        phy.velocity.x = 0.0f;
        phy.remainder.x = 0.0f;
        return true;
    }
    if (phy.velocity.y > 0.0f && !x_axis) {
        phy.is_on_ground = true;
        phy.is_jumping = false;
        if (is_enemy) {
            if (!enemy_can_jump) {
                phy.velocity.y = 0.0f;
                phy.remainder.y = 0.0f;
            }
        } else {
            phy.velocity.y = 0;
        }
        return true;
    }
    return false;
}


bool PhysicsSystem::check_player_x_tile_collision(entt::registry &registry, entt::entity player, const core::TileMap &tile_map, TransformComponent &transform, CollisionComponent &coll, simd::int4& rect, PhysicsComponent &phy, int move) {
    
    return check_solid_x_tile_collision(registry, player, tile_map, transform, coll, rect, phy, move); // It´s the same logic for X Axis
}

void PhysicsSystem::move_player_y(entt::registry &registry, entt::entity player, core::Level &level, bool is_tiny) {
    
    auto [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    
    simd::int4 rect = make_collision_rect(transform, coll, phy, is_tiny);
    phy.remainder.y += phy.velocity.y;
    int move_y = round(phy.remainder.y);
    phy.apply_gravity = true;
    
    if (move_y != 0) {
        phy.remainder.y -= move_y;
        int move_sign = sign(move_y);
        while (move_y) {
            rect.y += move_sign;
            bool can_move = !player_vs_solid(registry, player, rect, false) && check_player_y_tile_collision(registry, player, level.get_map(), transform, coll, rect, phy, move_sign, is_tiny);
            if (can_move) {
                transform.position.y += move_sign;
                move_y -= move_sign;
            }
            else break;
        }
    }
}

bool PhysicsSystem::check_player_y_tile_collision(entt::registry &registry, entt::entity player, const core::TileMap &tile_map, TransformComponent &transform, CollisionComponent &coll, simd::int4& rect ,PhysicsComponent &phy, int move, bool is_tiny) {
 
    simd::int2 grid_position = tile_map.get_grid_pos((int)transform.position.x, (int)transform.position.y);
    for (int x = grid_position.x - 1; x <= grid_position.x + 1; x++) {
        for (int y = grid_position.y - 2; y <= grid_position.y + 2; y++) {
            core::Tile tile = tile_map.get_tile(x, y);
            if (tile.type == 0 || tile.type == core::CLIMBABLE || tile_map.get_tile_index(x, y) >= 210) continue;
            
            simd::int4 tile_rect = tile_map.get_tile_rect(x, y);
            adjust_special_rect_tile(phy, is_tiny, tile, tile_rect);
            if (check_collision(rect, tile_rect)) {
                if (tile.type == core::ONE_WAY) {
                    coll.block_type = ONE_WAY;
                } else if (tile.type == core::SOLID) {
                    coll.block_type = SOLID;
                }
                if (handle_player_vs_y_tile_collision(phy, coll, tile, tile_rect, move)) return false;
            }
        }
    }
    
    return true;
}

void PhysicsSystem::adjust_special_rect_tile(PhysicsComponent &phy, bool is_tiny, core::Tile& tile, simd::int4 &rect) {
    if (phy.physic_current_state == PhysicState::CLIMB) {
        if (tile.type == core::DECORATION) {
            tile.collision_side = UP | DOWN;
            rect.w -= is_tiny ? 0 : 8;
        }
    }
    
    if (tile.type == core::CLIMBABLE) {
        rect.x += 5;
        rect.z -= 10;
    }
}

bool PhysicsSystem::handle_player_vs_y_tile_collision(PhysicsComponent &phy, CollisionComponent &coll, core::Tile& tile, simd::int4& rect, int move) {

    if(phy.velocity.y < 0.0f && (tile.collision_side & core::DOWN)) {
        coll.result_collision = DOWN;
        phy.velocity.y = 0.0f;
        return true;
    }
    if (phy.velocity.y > 0.0f && (tile.collision_side & core::UP )) {
        int previous_feet_pos = (coll.rect.y + coll.rect.w) - move;
        
        if (previous_feet_pos <= rect.y) {
            phy.is_on_ground = true;
            phy.is_jumping = false;
            coll.result_collision = UP;
            phy.velocity.y = 0.0f;
            if (phy.is_down) {
                phy.velocity.x = 0.0f;
                phy.remainder.x = 0.0;
            }
            return true;
        }
    }
    return false;
}

void PhysicsSystem::check_player_vs_special_tile(entt::registry &registry, entt::entity player, const core::TileMap &tile_map,  bool is_tiny) {
    
    auto [phy, coll, transform] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    coll.rect = make_collision_rect(transform, coll, phy, is_tiny);
    
    simd::int2 player_grid_pos = tile_map.get_grid_pos((int)transform.position.x,(int)transform.position.y);
    for (int x = player_grid_pos.x - 1; x <= player_grid_pos.x + 1; x++) {
        for (int y = player_grid_pos.y - 1; y <= player_grid_pos.y + 1; y++) {
            core::Tile tile = tile_map.get_tile(x, y);
            simd::int4 tile_rect = tile_map.get_tile_rect(x, y);
            
            adjust_special_rect_tile(phy, is_tiny, tile, tile_rect);
            
            if (check_collision(tile_rect, coll.rect)) {
                if (handle_player_vs_special_tile_collision(phy, coll, tile, tile_rect)) return;
            }
        }
    }
}

bool PhysicsSystem::handle_player_vs_special_tile_collision(PhysicsComponent &phy, CollisionComponent &coll, core::Tile &tile, simd::int4 &rect) {
    if (tile.type == core::CLIMBABLE) {
        coll.special_block_type = CLIMBABLE;
        return true;
    } else {
        coll.special_block_type = AIR;
        return false;
    }
}

void PhysicsSystem::zone_transition(entt::registry& registry, entt::entity player, const core::Level &level) {
    auto& trans = registry.get<TransformComponent>(player);
    if (level.get_current_zone() == core::LevelZone::OVERWORLD) {
        auto& zone = level.get_current_level_zone();
        
        if (zone.type == core::ZoneType::HORIZONTAL_ZONE) {
            manage_horizontal_zone(trans, zone);
        }
        else if (zone.type == core::ZoneType::VERTICAL_ZONE) {
            manage_vertical_zone(trans, zone);
        }
    }
}

void PhysicsSystem::manage_horizontal_zone(TransformComponent &trans, const core::Zone& zone) {
    if (trans.position.x <= zone.min_x * 16) {
        trans.position.x = zone.min_x * 16;
    } else if (trans.position.x + trans.scale.x >= zone.max_x * 16) {
        trans.position.x = zone.max_x * 16 - trans.scale.x;
    }
}
void PhysicsSystem::manage_vertical_zone(TransformComponent &trans, const core::Zone &zone) {
    float max_x = zone.min_x * 16 + 256 ;
    if (trans.position.x + trans.scale.x <= zone.min_x * 16) {
        trans.position.x = zone.max_x * 16 - trans.scale.x;
    } else if (trans.position.x > max_x) {
        trans.position.x = zone.min_x * 16 - trans.scale.x;
    }
}

void PhysicsSystem::player_grab_entities(entt::registry &registry, entt::entity player, core::Level& level, core::ResourceManager& manager) {
    auto [phy, coll, trans, control, inter] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, ControlComponent, GrabberComponent>(player);
    
    coll.rect = make_collision_rect(trans, coll, phy);
    coll.rect.w += 2;
    coll.collide_with_entity = false;
    
    for (auto entity : registry.view<PhysicsComponent>(entt::exclude<DoorComponent, BossComponent, InputComponent, HawkmountComponent>)) {
        if (entity == player || !is_entity_valid(registry, entity, level)) continue;
        if (can_grab_entity(registry, player, entity)) {
            handle_grab(registry, player, entity);
            if (!registry.any_of<PowerUpComponent>(entity)) manager.play_audio("LIFT SOUND");
            return;
        }
    }
}

bool PhysicsSystem::can_grab_entity(entt::registry& registry, entt::entity player, entt::entity entity) {
    auto [phy, coll, trans, control, inter] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, ControlComponent, GrabberComponent>(player);
    auto [ePhy, eColl, eTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
    eColl.rect = make_collision_rect(eTrans, eColl, ePhy);
    
    bool is_on_top = registry.any_of<EnemyComponent>(entity) ? std::abs((coll.rect.y + coll.rect.w) - eColl.rect.y) <= 2 : true;
    bool grab_just_pressed = control.grabb && !control.last_grab;
    if (check_collision(coll.rect, eColl.rect) && is_on_top && !phy.is_jumping && eColl.can_collide_with_entity) {
        if(grab_just_pressed && inter.object == entt::null){
            control.last_pitch = true;
            return true;
        }
    }
    
    return false;
}

void PhysicsSystem::handle_grab(entt::registry &registry, entt::entity player, entt::entity entity) {
    auto [phy, coll, trans, inter] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent, GrabberComponent>(player);
    auto [ePhy, eColl, eTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
    registry.emplace_or_replace<GrabbedComponent>(entity, GrabbedComponent{});

    phy.solid_velocity = {0.0f, 0.0f};
    eColl.can_collide_with_entity = false;
    coll.collide_with_entity = true;
    inter.object = entity;
    
    if (registry.any_of<PotionComponent>(entity)) eColl.result_collision = NONE;
    if(!phy.is_jumping){
        phy.physic_current_state = PhysicState::GRAB_START;
        phy.is_locked = true;
    }else{
        phy.is_grabbing = true;
    }
}

void PhysicsSystem::player_dectect_door(entt::registry &registry, entt::entity player) {
    auto [phy, coll, trans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    coll.rect = make_collision_rect(trans, coll, phy);
    coll.rect.w += 2;
    
    for (auto entity : registry.view<DoorComponent>()) {
        auto [ePhy, eColl, eTrans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(entity);
        eColl.rect = make_collision_rect(eTrans, eColl, ePhy);
        if (check_collision(eColl.rect, coll.rect)) eColl.result_collision = CollisionSide::ALL;
        else  eColl.result_collision = CollisionSide::NONE;
    }
}

void PhysicsSystem::player_vs_hawkmouth(entt::registry &registry, entt::entity player) {
    auto [phy, coll, trans] = registry.get<PhysicsComponent, CollisionComponent, TransformComponent>(player);
    coll.rect = make_collision_rect(trans, coll, phy);
    
    for (auto hawkmouth : registry.view<HawkmountComponent>()) {
        auto [hPhy, hColl, hTrans, hawk, anim] =  registry.get<PhysicsComponent, CollisionComponent, TransformComponent, HawkmountComponent, AnimationComponent>(hawkmouth);
        hColl.rect = make_collision_rect(hTrans, hColl, hPhy);
        
        if (check_collision(hColl.rect, coll.rect) && phy.velocity.x != 0 && anim.state == AnimationState::ANIM_WALK && anim.is_in_animation == false) {
            if (trans.position.x >= hTrans.position.x)  hawk.finish_level = true;
        }
    }
}

void PhysicsSystem::player_collision(entt::registry &registry, float delta_time, core::Level &level) {
    
    auto& game_data = registry.ctx().get<core::GameData>();
    auto& resource = registry.ctx().get<core::ResourceManager>();
    
    for (auto player : registry.view<InputComponent, CollisionComponent, InterectionComponent, PhysicsComponent, TransformComponent, PaletteComponent>()) {
        
        if (!is_player_valid(registry, player, game_data)) continue;
        players_vs_entity(registry, player, game_data);
        reset_entity(registry, player, false);
        bool is_tiny = registry.any_of<TinyComponent>(player);
        move_player_x(registry, player, level, is_tiny);
        apply_gravity(registry, player, delta_time);
        move_player_y(registry, player, level, is_tiny);
        check_player_vs_special_tile(registry, player, level.get_map(), is_tiny);
        zone_transition(registry, player, level);
        player_grab_entities(registry, player, level, resource);
        player_dectect_door(registry, player);
        player_vs_hawkmouth(registry, player);
    }
}

void PhysicsSystem::move(entt::registry &registry, entt::entity entity, core::Level &level, float delta_time) {
    if (!is_entity_valid(registry, entity, level)) return;
    
    reset_entity(registry, entity, true);
    move_x(registry, entity, level.get_map(), false);
    apply_gravity(registry, entity, delta_time);
    move_y(registry, entity, level.get_map(), false);
}

void PhysicsSystem::item_collision(entt::registry &registry, float delta_time, core::Level &level) {
    
    for (auto item :registry.view<ItemComponent>(entt::exclude<SolidComponent, InputComponent>)) {
        move(registry, item, level, delta_time);
    }
}

void PhysicsSystem::enemy_projectile_move(entt::registry &registry, float delta_time, core::Level &level) {
    for (auto proj : registry.view<EnemyProjectile>(entt::exclude<SolidComponent>)) {
        move(registry, proj, level, delta_time);
    }
}

void PhysicsSystem::modify_projectile_direction(PhysicsComponent &phy, CollisionComponent &coll) {
    float direction = phy.is_facing_right ? -1.0f : 1.0f;
    if (coll.result_collision & LEFT || coll.result_collision & RIGHT) {
        phy.velocity.x = direction;
    }
}

void PhysicsSystem::projectile_ground_bounce(entt::registry &registry, entt::entity entity, PhysicsComponent &phy, CollisionComponent &coll, ProjectileComponent &proj, bool is_enemy) {

    if (proj.current_num_jump < proj.max_jump && coll.result_collision & UP) {
        phy.velocity.y = -2.5f;
        if (is_enemy) {
            auto& enemy = registry.get<EnemyComponent>(entity);
            enemy.flip_y = true;
        }
        proj.current_num_jump += 1;
    }
}

void PhysicsSystem::expire_projectile(entt::registry &registry, entt::entity entity, PhysicsComponent &phy, CollisionComponent &coll, ProjectileComponent &proj, bool is_enemy) {
    if (proj.current_num_jump >= proj.max_jump && coll.result_collision != NONE) {
        registry.remove<ProjectileComponent>(entity);
        if (is_enemy) {
            phy.is_facing_right = phy.velocity.x >= 0.0f;
        }
        phy.velocity.x = 0;
    }
}

void PhysicsSystem::bounce_projectile(entt::registry &registry) {
    for (auto projectile : registry.view<ProjectileComponent>()) {
        auto [phy, coll, proj] = registry.get<PhysicsComponent, CollisionComponent, ProjectileComponent>(projectile);
        bool is_enemy = registry.any_of<EnemyComponent>(projectile);
        if (!registry.any_of<BombComponent>(projectile)) modify_projectile_direction(phy, coll);
        projectile_ground_bounce(registry, projectile, phy, coll, proj, is_enemy);
        expire_projectile(registry, projectile, phy, coll, proj, is_enemy);
    }
}

simd::int4 PhysicsSystem::make_collision_rect(const TransformComponent &trans, const CollisionComponent &coll, const PhysicsComponent& phy){
    float current_height = phy.is_down ? (trans.scale.y/ coll.y_padding) : trans.scale.y;
    float y_offset = phy.is_down ? (trans.scale.y / 2.0f) : 0.0f;
    return {int(trans.position.x + coll.padding.x ), int(trans.position.y + y_offset + coll.padding.y), int(trans.scale.x - coll.padding.z) , int(current_height - coll.padding.w)};
}

simd::int4 PhysicsSystem::make_collision_rect(const TransformComponent &trans, const CollisionComponent &coll, const PhysicsComponent& phy, bool is_tiny){
    
    float current_height = phy.is_down || is_tiny ? (trans.scale.y/ coll.y_padding) : trans.scale.y;
    float y_offset = phy.is_down || is_tiny ? (trans.scale.y / 2.0f) : 0.0f;
    return {int(trans.position.x + coll.padding.x ), int(trans.position.y + y_offset + coll.padding.y), int(trans.scale.x - coll.padding.z) , int(current_height - coll.padding.w)};
}

simd::int4 PhysicsSystem::make_solid_rect(const TransformComponent &trans, const CollisionComponent &coll, const PhysicsComponent &phy){
    return {int(trans.position.x) - int(trans.scale.x)/2, int(trans.position.y) - int(trans.scale.y)/2, int(trans.scale.x), int(trans.scale.y)};
}

}
