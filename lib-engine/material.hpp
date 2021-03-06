#pragma once

#ifndef polymer_material_asset_hpp
#define polymer_material_asset_hpp

#include "gl-api.hpp"
#include "math-core.hpp"
#include "asset-handle-utils.hpp"
#include "shader-library.hpp"
#include "ecs/typeid.hpp"
#include "json.hpp"
#include "serialization.hpp"

namespace polymer
{
    using json = nlohmann::json;

    typedef std::shared_ptr<polymer::shader_variant> cached_variant;

    ////////////////////////////
    //   material_interface   //
    ////////////////////////////

    struct material_interface
    {
        mutable cached_variant compiled_shader{ nullptr };  // cached on first access (because needs to happen on GL thread)
        shader_handle shader;                               // typically set during object inflation / deserialization
        virtual void update_uniforms() {}                   // generic interface for overriding specific uniform sets
        virtual void use() {}                               // generic interface for binding the program
        virtual void resolve_variants() = 0;                // all overridden functions need to call this to cache the shader
        virtual uint32_t id() = 0;                          // returns the gl handle, used for sorting materials by type to minimize state changes in the renderer
    };

    //////////////////////////////////
    //   polymer_default_material   //
    //////////////////////////////////

    struct polymer_default_material final : public material_interface
    {
        polymer_default_material();
        virtual void use() override final;
        virtual void resolve_variants() override final;
        virtual uint32_t id() override final;
    };
    POLYMER_SETUP_TYPEID(polymer_default_material);

    template<class F> void visit_fields(polymer_default_material & o, F f) {}
    inline void to_json(json & j, const polymer_default_material & p) {}
    inline void from_json(const json & archive, polymer_default_material & m) {}

    /////////////////////////////
    //   polymer_fx_material   //
    /////////////////////////////

    struct polymer_fx_material final : public material_interface
    {
        polymer_fx_material();
        virtual void use() override final;
        virtual void resolve_variants() override final;
        virtual uint32_t id() override final;
    };
    POLYMER_SETUP_TYPEID(polymer_fx_material);

    template<class F> void visit_fields(polymer_fx_material & o, F f) {}
    inline void to_json(json & j, const polymer_fx_material & p) {}
    inline void from_json(const json & archive, polymer_fx_material & m) {}

    ////////////////////////////////////
    //   polymer_wireframe_material   //
    ////////////////////////////////////

    struct polymer_wireframe_material final : public material_interface
    {
        float4 color{ 1, 1, 1, 0.5f};
        polymer_wireframe_material();
        virtual void use() override final;
        virtual void resolve_variants() override final;
        virtual uint32_t id() override final;
    };
    POLYMER_SETUP_TYPEID(polymer_wireframe_material);

    template<class F> void visit_fields(polymer_wireframe_material & o, F f) {}
    inline void to_json(json & j, const polymer_wireframe_material & p) {}
    inline void from_json(const json & archive, polymer_wireframe_material & m) {}

    //////////////////////////////////////
    //   polymer_blinn_phong_standard   //
    //////////////////////////////////////

    class polymer_blinn_phong_standard final : public material_interface
    {
        int bindpoint = 0;

    public:

        polymer_blinn_phong_standard();
        virtual void use() override final;
        virtual void resolve_variants() override final;
        virtual uint32_t id() override final;
        virtual void update_uniforms() override final;

        float2 texcoordScale{ 1.f, 1.f };

        texture_handle diffuse;
        texture_handle normal;

        float3 diffuseColor;
        float3 specularColor;
        float specularShininess;
        float specularStrength;
    };
    POLYMER_SETUP_TYPEID(polymer_blinn_phong_standard);

    template<class F> void visit_fields(polymer_blinn_phong_standard & o, F f)
    {
        f("diffuse_color", o.diffuseColor);
        f("specular_color", o.specularColor);
        f("specular_shininess", o.specularShininess);
        f("specular_strength", o.specularStrength);
        f("texcoord_scale", o.texcoordScale, range_metadata<float>{ -16.f, 16.f });
        f("diffuse_handle", o.diffuse);
        f("normal_handle", o.normal);
        f("program_handle", o.shader, editor_hidden{}); // hidden because shaders are tied to materials
        o.resolve_variants(); // trigger recompile if a property has been changed
    }

    inline void to_json(json & j, const polymer_blinn_phong_standard & p) {
        visit_fields(const_cast<polymer_blinn_phong_standard&>(p), [&j](const char * name, auto & field, auto... metadata) {
            j[name] = field;
        });
    }

    inline void from_json(const json & archive, polymer_blinn_phong_standard & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    }

    //////////////////////////////
    //   polymer_pbr_standard   //
    //////////////////////////////

    class polymer_pbr_standard final : public material_interface
    {
        int bindpoint = 0;

    public:

        polymer_pbr_standard();

        virtual void update_uniforms() override final;
        virtual void use() override final;
        virtual void resolve_variants() override final;
        virtual uint32_t id() override final;

        void update_uniforms_shadow(GLuint handle);
        void update_uniforms_ibl(GLuint irradiance, GLuint radiance);

        float3 baseAlbedo{1.f, 1.f, 1.f};

        float roughnessFactor{ 0.04f };
        float metallicFactor{ 1.f };

        float3 baseEmissive{ 0.f, 0.f, 0.f };
        float emissiveStrength{ 1.f };

        float specularLevel{ 0.01f };
        float occlusionStrength{ 1.f };
        float ambientStrength{ 1.f };

        float opacity{ 1.f };
        float shadowOpacity{ 1.f };
        float2 texcoordScale{ 1.f, 1.f };

        texture_handle albedo;
        texture_handle normal;
        texture_handle metallic;
        texture_handle roughness;
        texture_handle emissive;
        texture_handle height;
        texture_handle occlusion;
    };

    POLYMER_SETUP_TYPEID(polymer_pbr_standard);

    template<class F> void visit_fields(polymer_pbr_standard & o, F f)
    {
        f("base_albedo", o.baseAlbedo);
        f("opacity", o.opacity, range_metadata<float>{ 0.f, 1.f });
        f("roughness_factor", o.roughnessFactor, range_metadata<float>{ 0.04f, 1.f });
        f("metallic_factor", o.metallicFactor, range_metadata<float>{ 0.f, 1.f });
        f("base_emissive", o.baseEmissive);
        f("emissive_strength", o.emissiveStrength, range_metadata<float>{ 0.f, 1.f });
        f("specularLevel", o.specularLevel, range_metadata<float>{ 0.f, 1.f });
        f("occulusion_strength", o.occlusionStrength, range_metadata<float>{ 0.f, 1.f });
        f("ambient_strength", o.ambientStrength, range_metadata<float>{ 0.f, 1.f });
        f("shadow_opacity", o.shadowOpacity, range_metadata<float>{ 0.f, 1.f });
        f("texcoord_scale", o.texcoordScale, range_metadata<float>{ -16.f, 16.f });
        f("albedo_handle", o.albedo);
        f("normal_handle", o.normal);
        f("metallic_handle", o.metallic);
        f("roughness_handle", o.roughness);
        f("emissive_handle", o.emissive);
        f("height_handle", o.height);
        f("occlusion_handle", o.occlusion);
        f("program_handle", o.shader, editor_hidden{}); // hidden because shaders are tied to materials
        o.resolve_variants(); // trigger recompile if a property has been changed
    }

    inline void to_json(json & j, const polymer_pbr_standard & p) {
        visit_fields(const_cast<polymer_pbr_standard&>(p), [&j](const char * name, auto & field, auto... metadata) { 
            j[name] = field;
        });
    }

    inline void from_json(const json & archive, polymer_pbr_standard & m) {
        visit_fields(m, [&archive](const char * name, auto & field, auto... metadata) {
            field = archive.at(name).get<std::remove_reference_t<decltype(field)>>();
        });
    }

    template<class F> void visit_subclasses(material_interface * p, F f)
    {
        f("polymer_default_material", dynamic_cast<polymer_default_material *>(p));
        f("polymer_pbr_standard", dynamic_cast<polymer_pbr_standard *>(p));
        f("polymer_blinn_phong_standard", dynamic_cast<polymer_blinn_phong_standard *>(p));
        f("polymer_wireframe_material", dynamic_cast<polymer_wireframe_material *>(p));
        f("polymer_fx_material", dynamic_cast<polymer_fx_material *>(p));
    }

}

#endif // end polymer_material_asset_hpp
