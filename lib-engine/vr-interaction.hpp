#pragma once

#ifndef polymer_vr_interaction_hpp
#define polymer_vr_interaction_hpp

#include "gl-imgui.hpp"
#include "gl-gizmo.hpp"
#include "environment.hpp"
#include "ecs/core-events.hpp"
#include "material.hpp"
#include "openvr-hmd.hpp"
#include "renderer-pbr.hpp"


namespace polymer
{
    /// interface: pre-render, post-render, handle input
    /// render_source (get list of render components)
    /// render_source -> update (renderloop)

    enum class vr_event_t : uint32_t
    {
        focus_begin,
        focus_end,
        press,
        release,
        cancel
    };

    enum class vr_input_source_t : uint32_t
    {
        left_controller,
        right_controller,
        left_hand,
        right_hand,
        tracker
    };

    struct vr_input_focus
    {
        entity_hit_result result;
    };

    struct vr_input_event
    {
        vr_event_t type;
        vr_input_source_t source;
        vr_input_focus focus;
        uint64_t timestamp;
        openvr_controller controller;
    };
    POLYMER_SETUP_TYPEID(vr_input_event);

    struct vr_teleport_event { float3 world_position; uint64_t frame_count; };
    POLYMER_SETUP_TYPEID(vr_teleport_event);

    // triple buffer input_event state
    // lock/unlock focus for teleportation
    class vr_input_processor
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };

        vr_input_focus get_focus(const openvr_controller * controller)
        {

        }

    public:

        vr_input_processor(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)
            : env(env), hmd(hmd)
        {

        }

        void process(const float dt, const view_data view)
        {
            for (auto hand : { vr::TrackedControllerRole_LeftHand, vr::TrackedControllerRole_RightHand })
            {
                const openvr_controller * controller = hmd->get_controller(hand);
                vr_input_focus focus = get_focus(controller);

                //vr_input_event event;
                //event.type = vr_event_t::
                //if (controller->trigger.pressed)
                // collide
                // issue event
            }
        }
    };

    enum class controller_render_style_t : uint32_t
    {
        invisible,
        laser,
        arc
    };

    // visual appearance of openvr controller
    // render as: arc, line
    // shaders + materials
    struct vr_controller_system
    {
        environment * env{ nullptr };
        openvr_hmd * hmd{ nullptr };
        mesh_component * mesh_component{ nullptr };
        entity pointer;
        arc_pointer_data arc_pointer;
        controller_render_style_t style{ controller_render_style_t::laser };
        float3 target_location;
        std::vector<float3> arc_curve;
        bool should_draw_pointer{ false };

    public:

        vr_controller_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd)
            : env(env), hmd(hmd)
        {
            arc_pointer.xz_plane_bounds = aabb_3d({ -24.f, -0.01f, -24.f }, { +24.f, +0.01f, +24.f });

            pointer = env->track_entity(orch->create_entity());
            env->identifier_system->create(pointer, "vr-pointer");
            env->xform_system->create(pointer, transform(float3(0, 0, 0)), { 1.f, 1.f, 1.f });

            polymer::material_component pointer_mat(pointer);
            pointer_mat.material = material_handle(material_library::kDefaultMaterialId);
            env->render_system->create(pointer, std::move(pointer_mat));

            polymer::mesh_component pointer_mesh(pointer);
            pointer_mesh.mesh = gpu_mesh_handle("vr-pointer");
            mesh_component = env->render_system->create(pointer, std::move(pointer_mesh));
            assert(mesh_component != nullptr);
        }

        void set_visual_style(const controller_render_style_t new_style)
        {
            style = new_style;
        }

        std::vector<entity> get_renderables() const
        {
            if (style != controller_render_style_t::invisible && should_draw_pointer) return { pointer };
        }

        void handle_event(const vr_input_event & event)
        {
            if (event.type == vr_event_t::focus_begin)
            {
                set_visual_style(controller_render_style_t::laser);
                const float hit_distance = event.focus.result.r.distance;
                auto & m = mesh_component->mesh.get();
                m = make_mesh_from_geometry(make_plane(0.010f, hit_distance, 24, 24), GL_STREAM_DRAW);
                auto t = event.controller.t;
                if (auto * tc = env->xform_system->get_local_transform(pointer))
                {
                    t = t * transform(make_rotation_quat_axis_angle({ 1, 0, 0 }, (float)POLYMER_PI / 2.f)); // coordinate
                    t = t * transform(float4(0, 0, 0, 1), float3(0, -(hit_distance * 0.5f), 0)); // translation
                    env->xform_system->set_local_transform(pointer, t);
                }
            }
            else if (event.type == vr_event_t::focus_end)
            {
                set_visual_style(controller_render_style_t::invisible);
            }
        }

        void handle_event(const vr_teleport_event & event)
        {

        }

        void process(const float dt, const view_data view)
        {
            std::vector<input_button_state> touchpad_button_states = {
                hmd->get_controller(vr::TrackedControllerRole_LeftHand)->touchpad,
                hmd->get_controller(vr::TrackedControllerRole_RightHand)->touchpad
            };

            for (int i = 0; i < touchpad_button_states.size(); ++i)
            {
                if (touchpad_button_states[i].down)
                {
                    const transform t = hmd->get_controller(vr::ETrackedControllerRole(i + 1))->t;
                    auto & m = mesh_component->mesh.get();
                    arc_pointer.position = t.position;
                    arc_pointer.forward = -qzdir(t.orientation);
                    if (make_pointer_arc(arc_pointer, arc_curve))
                    {
                        set_visual_style(controller_render_style_t::arc);
                        should_draw_pointer = true;
                        const geometry arc_geo = make_parabolic_geometry(arc_curve, arc_pointer.forward, 0.1f, arc_pointer.lineThickness);
                        m = make_mesh_from_geometry(arc_geo, GL_STREAM_DRAW);
                    }
                }
            }
        }
    };

    //////////////////////////
    //   vr_imgui_surface   //
    //////////////////////////

    class vr_imgui_surface : public gui::imgui_surface
    {
        entity imgui_billboard;
        entity pointer;
        std::shared_ptr<polymer_fx_material> imgui_material;
        bool should_draw_pointer{ false };
    public:
        vr_imgui_surface(entity_orchestrator * orch, environment * env, const uint2 size, GLFWwindow * window);
        void update(environment * env, const transform & pointer_transform, const transform & billboard_origin, bool trigger_state);
        void update_renderloop();
        entity get_pointer() const;
        entity get_billboard() const;
    };

    ////////////////////////////
    //   vr_teleport_system   //
    ////////////////////////////

    // todo - use event

    class vr_teleport_system
    {
        geometry nav_geometry;
        float3 target_location;
        entity teleportation_arc{ kInvalidEntity };
        bool should_draw{ false };
        openvr_hmd * hmd{ nullptr };
    public:
        vr_teleport_system(entity_orchestrator * orch, environment * env, openvr_hmd * hmd);
        void update(const uint64_t current_frame);
        entity get_teleportation_arc();
    };

    //////////////////
    //   vr_gizmo   //
    //////////////////

    class vr_gizmo
    {
        entity gizmo_entity, pointer;
        std::shared_ptr<polymer_fx_material> gizmo_material;
        bool should_draw_pointer{ false };
        tinygizmo::gizmo_application_state gizmo_state;
        tinygizmo::gizmo_context gizmo_ctx;
    public:
        vr_gizmo(entity_orchestrator * orch, environment * env, openvr_hmd * hmd);
        void handle_input(const app_input_event & e);
        void update(const view_data view);
        void render();
        entity get_pointer() const;
        entity get_gizmo() const;
    };

} // end namespace polymer

#endif // end polymer_vr_interaction_hpp
