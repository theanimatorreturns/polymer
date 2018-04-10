#include "material.hpp"
#include "fwd_renderer.hpp"

using namespace polymer;

//////////////////////////////////////////////////////
//   Physically-Based Metallic-Roughness Material   //
//////////////////////////////////////////////////////

void MetallicRoughnessMaterial::resolve_variants() const
{
    if (!compiled_shader)
    {
        std::cout << ">>>>>>>>>>>>>> Resolving Variants" << std::endl;
        compiled_shader = shader.get()->get_variant(required_defines);
    }
}

uint32_t MetallicRoughnessMaterial::id() const
{
    resolve_variants();
    return compiled_shader->shader.handle();
}

void MetallicRoughnessMaterial::update_uniforms()
{
    resolve_variants();
    GlShader & program = compiled_shader->shader;
    program.bind();

    program.uniform("u_roughness", roughnessFactor);
    program.uniform("u_metallic", metallicFactor);
    program.uniform("u_opacity", opacity);
    program.uniform("u_albedo", baseAlbedo);
    program.uniform("u_emissive", baseEmissive);
    program.uniform("u_specularLevel", specularLevel);
    program.uniform("u_occlusionStrength", occlusionStrength);
    program.uniform("u_ambientStrength", ambientStrength);
    program.uniform("u_emissiveStrength", emissiveStrength);
    program.uniform("u_shadowOpacity", shadowOpacity);
    program.uniform("u_texCoordScale", float2(texcoordScale));

    bindpoint = 0;

    if (compiled_shader->enabled("HAS_ALBEDO_MAP")) program.texture("s_albedo", bindpoint++, albedo.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_NORMAL_MAP")) program.texture("s_normal", bindpoint++, normal.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_ROUGHNESS_MAP")) program.texture("s_roughness", bindpoint++, roughness.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_METALNESS_MAP")) program.texture("s_metallic", bindpoint++, metallic.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_EMISSIVE_MAP")) program.texture("s_emissive", bindpoint++, emissive.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_HEIGHT_MAP")) program.texture("s_height", bindpoint++, height.get(), GL_TEXTURE_2D);
    if (compiled_shader->enabled("HAS_OCCLUSION_MAP")) program.texture("s_occlusion", bindpoint++, occlusion.get(), GL_TEXTURE_2D);

    program.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_ibl(GLuint irradiance, GLuint radiance)
{
    resolve_variants();
    GlShader & program = compiled_shader->shader;
    if (!compiled_shader->enabled("USE_IMAGE_BASED_LIGHTING")) throw std::runtime_error("should not be called unless USE_IMAGE_BASED_LIGHTING is defined.");

    program.bind();
    program.texture("sc_irradiance", bindpoint++, irradiance, GL_TEXTURE_CUBE_MAP);
    program.texture("sc_radiance", bindpoint++, radiance, GL_TEXTURE_CUBE_MAP);
    program.unbind();
}

void MetallicRoughnessMaterial::update_uniforms_shadow(GLuint handle)
{
    resolve_variants();
    GlShader & program = compiled_shader->shader;
    if (!compiled_shader->enabled("ENABLE_SHADOWS")) throw std::runtime_error("should not be called unless ENABLE_SHADOWS is defined.");

    program.bind();
    program.texture("s_csmArray", bindpoint++, handle, GL_TEXTURE_2D_ARRAY);
    program.unbind();
}

void MetallicRoughnessMaterial::use()
{
    resolve_variants();
    compiled_shader->shader.bind();
}
