//
//  physic_system.hpp
//  Super Mario Bros 2
//
//  Created by William Bustamante on 01/04/26.
//
#pragma once
#include <entt.hpp>
#include <simd/simd.h>

namespace core {

struct ColliderData;
class Level;
class TileMap;
class ResourceManager;
struct Tile;
struct GameData;
struct Zone;
}

struct CollisionComponent;
struct TransformComponent;
struct PhysicsComponent;
struct EnemyComponent;
struct BossComponent;
struct SolidComponent;
struct GrabberComponent;
struct ControlComponent;
struct ProjectileComponent;

enum Axis : std::uint8_t;

namespace ecs {

struct TileCollisionResult {
    std::uint8_t collision_side, tile_type;
    simd::int4 tile_rect;
};

class PhysicsSystem{
public:
    
    void update(entt::registry& registry, float delta_time, core::Level& level, bool& phy_pause);
    
private:
    
    float gravity = 9.8f;
    const float fall_speed = 9.8;
    bool* phy_pause = nullptr;
    
    simd::int4 make_collision_rect(const TransformComponent& trans, const CollisionComponent& coll, const PhysicsComponent& phy);
    
    simd::int4 make_collision_rect(const TransformComponent& trans, const CollisionComponent& coll, const PhysicsComponent& phy,  bool is_tiny);
    
    simd::int4 make_solid_rect(const TransformComponent& trans, const CollisionComponent& coll, const PhysicsComponent& phy);
    
    void solid_collision(entt::registry& registry, float delta_time, core::Level &level);
    void world_collision(entt::registry& registry, float delta_time, core::Level& level);
    void entity_collision(entt::registry& registry, float delta_time, core::Level& level);
    TileCollisionResult find_colliding_tile(const core::TileMap& tile_map, simd::float2 position, simd::int4 rect ,bool x_axis, bool check_one_way);
    bool is_entity_valid(entt::registry& registry, entt::entity entity, const core::Level& level);
    void apply_gravity(entt::registry& registry, entt::entity entity, float delta_time);
    void reset_entity(entt::registry& registry, entt::entity entity, bool reset_block_tile);
    
    // Solid Logic
    void move_x(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, bool is_a_solid);
    void carry_x_actor(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, PhysicsComponent& phy, simd::int4& rect, int move);
    bool check_solid_x_tile_collision(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, TransformComponent& transform, CollisionComponent& coll, simd::int4& rect, PhysicsComponent& phy, int move);
    void move_y(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, bool is_a_solid);
    void carry_y_actor(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, PhysicsComponent& phy, CollisionComponent& coll, int move);
    bool check_solid_y_tile_collision(entt::registry& registry, entt::entity entity, const core::TileMap& tile_map, TransformComponent& transform, CollisionComponent& coll, PhysicsComponent& phy, int move);
    void manage_boss_projectiles(entt::registry& registry,  core::Level& level);
    void projectile_solid_collision(entt::registry& registry, core::ResourceManager& manager);
    void enemy_interaction(entt::registry& registry, entt::entity entity, TransformComponent& transform, CollisionComponent& coll, PhysicsComponent& phy, EnemyComponent& enemy, core::ResourceManager& manager);
    void item_projectile_interaction(entt::registry& registry, entt::entity entity, TransformComponent& transform, CollisionComponent& coll, PhysicsComponent& phy, EnemyComponent& enemy, core::ResourceManager& manager);
    void boss_interaction(entt::registry& registry, entt::entity entity, TransformComponent& transform, CollisionComponent& coll, PhysicsComponent& phy, BossComponent& enemy, core::ResourceManager& manager);
    bool interaction(TransformComponent& eTrans, CollisionComponent& eColl, PhysicsComponent& ePhy, TransformComponent& pTrans, CollisionComponent& pColl, PhysicsComponent& pPhy);
    void solid_move(entt::registry& registry, core::Level& level, float delta_time);
    
    // Player logic
    bool is_player_valid(entt::registry& registry, entt::entity player, const core::GameData& game_data);
    void players_vs_entity(entt::registry& registry, entt::entity player, const core::GameData& game_data);
    void move_player_x(entt::registry& registry, entt::entity player, core::Level& level, bool is_tiny);
    bool player_vs_solid(entt::registry& registry, entt::entity player, simd::int4& rect, bool x_axis);
    bool handle_player_vs_solid_collision(PhysicsComponent& phy, SolidComponent& solid, bool x_axis, bool is_enemy, bool enemy_can_jump);
    void player_vs_enemy(PhysicsComponent& phy, entt::entity enemy, bool x_axis);
    //void player_vs_platform(entt::registry& registry, entt::entity player, bool x_axis);
    bool check_player_x_tile_collision(entt::registry& registry, entt::entity player, const core::TileMap& tile_map, TransformComponent& transform, CollisionComponent& coll, simd::int4& rect, PhysicsComponent& phy, int move);
    
    void move_player_y(entt::registry& registry, entt::entity player, core::Level& level, bool is_tiny);
    bool check_player_y_tile_collision(entt::registry& registry, entt::entity player, const core::TileMap& tile_map, TransformComponent& transform, CollisionComponent& coll, simd::int4& rect, PhysicsComponent& phy, int move, bool is_tiny);
    
    void adjust_special_rect_tile(PhysicsComponent& phy, bool is_tiny, core::Tile& tile ,simd::int4& rect);
    bool handle_player_vs_y_tile_collision(PhysicsComponent& phy, CollisionComponent& coll, core::Tile& tile, simd::int4& rect, int move);
    void check_player_vs_special_tile(entt::registry& registry, entt::entity player, const core::TileMap& tile_map, bool is_tiny);
    bool handle_player_vs_special_tile_collision(PhysicsComponent& phy, CollisionComponent& coll, core::Tile& tile, simd::int4& rect);
    void zone_transition(entt::registry& registry, entt::entity player, const core::Level& level);
    void manage_horizontal_zone(TransformComponent& trans, const core::Zone& zone);
    void manage_vertical_zone(TransformComponent& trans, const core::Zone& zone);
    void player_collision(entt::registry& registry, float delta_time, core::Level& level);
    void player_grab_entities(entt::registry& registry, entt::entity player, core::Level& level, core::ResourceManager& manager);
    bool can_grab_entity(entt::registry& registry, entt::entity player, entt::entity entity);
    void handle_grab(entt::registry& registry, entt::entity player, entt::entity entity);
    void player_dectect_door(entt::registry& registry, entt::entity player);
    void player_vs_hawkmouth(entt::registry& registry, entt::entity player);
    // Item Logic
    
    void item_collision(entt::registry& registry, float delta_time, core::Level &level);
    void move_item_x(entt::registry& registry, entt::entity item, core::Level& level);
    
    // Projectile Logic
    
    void enemy_projectile_move(entt::registry& registry, float delta_time, core::Level &level);

    void bounce_projectile(entt::registry& registry);
    void modify_projectile_direction(PhysicsComponent& phy, CollisionComponent& coll);
    void projectile_ground_bounce(entt::registry& registry, entt::entity entity, PhysicsComponent& phy, CollisionComponent& coll, ProjectileComponent& proj, bool is_enemy);
    void expire_projectile(entt::registry& registry, entt::entity entity, PhysicsComponent &phy, CollisionComponent &coll, ProjectileComponent &proj, bool is_enemy);
    
    // Generic Functions
    void move(entt::registry &registry, entt::entity entity, core::Level &level, float delta_time);
};

}
