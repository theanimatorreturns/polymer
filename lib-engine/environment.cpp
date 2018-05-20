#include "environment.hpp"
#include "system-renderer-pbr.hpp"
#include "system-collision.hpp"
#include "system-transform.hpp"
#include "system-identifier.hpp"
#include "file_io.hpp"
#include "serialization.inl"

using namespace polymer;

entity environment::track_entity(entity e) 
{ 
    log::get()->assetLog->info("[environment] created tracked entity {}", e);
    active_entities.push_back(e); return e;
}

const std::vector<entity> & environment::entity_list() 
{ 
    return active_entities; 
}

void environment::copy(entity src, entity dest)
{
    visit_systems(this, [src](const char * name, auto * system_pointer)
    {
        if (system_pointer)
        {
            visit_components(src, system_pointer, [&](const char * component_name, auto & component_ref, auto... component_metadata)
            {
                // system_pointer->create(...)
            });
        }
    });

}
void environment::destroy(entity e)
{
    if (e == kInvalidEntity) return;

    // Destroy everything
    if (e == kAllEntities)
    {
        for (auto active : active_entities)
        {
            visit_systems(this, [active](const char * name, auto * system_pointer)
            {
                if (system_pointer) system_pointer->destroy(active);
            });
        }
        active_entities.clear();
    }
    else
    {
        active_entities.erase(std::find(active_entities.begin(), active_entities.end(), e));

        // Destroy a single entity
        visit_systems(this, [e](const char * name, auto * system_pointer)
        {
            if (system_pointer) system_pointer->destroy(e);
        });
        
    }
}

void environment::import_environment(const std::string & import_path, entity_orchestrator & o)
{
    manual_timer t;
    t.start();

    destroy(kAllEntities);

    const json env_doc = json::parse(read_file_text(import_path));
    std::unordered_map<entity, entity> remap_table;

    for (auto entityIterator = env_doc.begin(); entityIterator != env_doc.end(); ++entityIterator)
    {
        const entity entity_value = std::atoi(entityIterator.key().c_str());
        const entity new_entity = track_entity(o.create_entity());
        remap_table[entity_value] = new_entity; // remap old entity to new

        const json & comp = entityIterator.value();

        for (auto componentIterator = comp.begin(); componentIterator != comp.end(); ++componentIterator)
        {
            if (starts_with(componentIterator.key(), "@"))
            {
                const std::string type_key = componentIterator.key();
                const std::string type_name = type_key.substr(1);
                const poly_typeid id = get_typeid(type_name.c_str());

                visit_systems(this, [&](const char * system_name, auto * system_pointer)
                {
                    if (system_pointer)
                    {
                        if (type_name == get_typename<identifier_component>())
                        {
                            identifier_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<mesh_component>())
                        {
                            mesh_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<material_component>())
                        {
                            material_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<geometry_component>())
                        {
                            geometry_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<point_light_component>())
                        {
                            point_light_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<directional_light_component>())
                        {
                            directional_light_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else if (type_name == get_typename<scene_graph_component>())
                        {
                            scene_graph_component c(new_entity);
                            c = componentIterator.value();
                            //if (system_pointer->create(new_entity, id, &c)) std::cout << "Created " << type_name << " on " << system_name << std::endl;
                        }
                        else
                        {
                            throw std::runtime_error("component type name mismatch!");
                        }
                    }
                });
            }
            else throw std::runtime_error("type key mismatch!");
        }
    }

    // Finalize the transform system by refreshing the scene graph
    xform_system->refresh();

    t.stop();
    log::get()->assetLog->info("importing {} took {}ms", import_path, t.get());
}

void environment::export_environment(const std::string & export_path) 
{
    manual_timer t;
    t.start();

    json environment;

    // foreach entity
    for (const auto & e : entity_list())
    {
        json entity; // list of components

        // foreach system
        visit_systems(this, [&](const char * system_name, auto * system_pointer)
        {
            if (system_pointer)
            {
                // foreach component
                visit_components(e, system_pointer, [&](const char * component_name, auto & component_ref, auto... component_metadata)
                {
                    const std::string type_key = get_typename<std::decay<decltype(component_ref)>::type>();
                    const std::string type_name = "@" + type_key;

                    json component; 

                    // foreach field
                    visit_fields(component_ref, [&](const char * name, auto & field, auto... metadata)
                    { 
                        component[name] = field;
                    });

                    entity[type_name] = component;
                });
            }
        });

        environment[std::to_string(e)] = entity;
    }

    write_file_text(export_path, environment.dump(4));

    t.stop();
    log::get()->assetLog->info("exporting {} took {}ms", export_path, t.get());
}