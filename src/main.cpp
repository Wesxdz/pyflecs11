#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <flecs.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <typeindex> // For std::type_index

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

// Each (non-tag) component or relationship id on an entity is mapped to a py::object
// This allows arbitrary Python classes/variables (such as neural networks) as components
static std::map<flecs::id_t, std::map<flecs::id_t, py::object>> flecs_component_pyobject;
static std::vector<py::object> observer_callbacks;

// Simple wrapper for Flecs entity
class PyEntity {
public:
    flecs::entity entity;
    
    PyEntity(flecs::entity e) : entity(e) {}
    
    // Get entity ID
    uint64_t id() const { 
        return entity.id(); 
    }
    
    // Get entity name
    std::string name() const { 
        const char* name = entity.name();
        return name ? std::string(name) : "";
    }
    
    // Set entity name
    void set_name(const std::string& name) {
        entity.set_name(name.c_str());
    }
    
    // Check if entity is alive
    bool is_alive() const { 
        return entity.is_alive(); 
    }
    
    // Delete entity
    void destroy() { 
        entity.destruct(); 
    }
    
    // Add a tag (create tag entity if it doesn't exist, then add it)
    PyEntity* add_tag(const std::string& tag_name) {
        flecs::entity tag = entity.world().entity(tag_name.c_str());
        entity.add(tag);
        return this;
    }
    
    // Check if entity has a tag
    bool has_tag(const std::string& tag_name) {
        flecs::entity tag = entity.world().lookup(tag_name.c_str());
        return tag.is_valid() && entity.has(tag);
    }
    
    // Remove a tag
    void remove_tag(const std::string& tag_name) {
        flecs::entity tag = entity.world().lookup(tag_name.c_str());
        if (tag.is_valid()) {
            entity.remove(tag);
        }
    }

    PyEntity* set_component_instance(py::object py_component_instance) {
        py::object py_type = py::type::of(py_component_instance);
        std::string type_name = py::str(py_type.attr("__name__"));
        
        // Get or create the component entity
        flecs::entity flecs_comp_id = entity.world().entity(type_name.c_str());
        
        // Store the Python object
        flecs_component_pyobject[entity.id()][flecs_comp_id.id()] = py_component_instance;
        
        // Add the component using C API - use ecs_id() to get the component ID
        ecs_add_id(entity.world(), entity.id(), flecs_comp_id.id());
        
        return this;
    }

    // Get a Python component from the entity
    py::object get_component(py::object py_component_type) {
        std::string type_name = py::str(py_component_type.attr("__name__"));
        
        // Log using py::print
        py::print("DEBUG (get_component): Attempting to get component:", type_name);

        flecs::entity flecs_comp_id = entity.world().lookup(type_name.c_str());

        if (!flecs_comp_id.is_valid()) {
            py::print("DEBUG (get_component): Flecs ID for", type_name, "is NOT valid.");
            return py::none();
        }
        py::print("DEBUG (get_component): Flecs ID for", type_name, "is valid. ID:", flecs_comp_id.id()); 

        if (flecs_component_pyobject[entity].count(flecs_comp_id)) {
            py::print("DEBUG (get_component): Entity HAS component", type_name);
            return flecs_component_pyobject[entity][flecs_comp_id];
        }
        py::print("DEBUG (get_component): Entity DOES NOT HAVE component", type_name);
        return py::none();
    }
};

class PyQueryIterator {
private:
    flecs::world world;
    ecs_query_t* query;
    ecs_iter_t it;
    std::vector<ecs_entity_t> component_ids;
    bool next_archetype = true;
    size_t i = 0;
    size_t current = 0;
public:
    PyQueryIterator(flecs::world& w, py::args args) : world(w) {
        // Convert py::args to std::vector<py::object>

        for (auto arg : args) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            ecs_entity_t component_id = world.entity(component_name.c_str());
            component_ids.push_back(component_id);
        }
        ecs_query_desc_t desc = {};

        for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
            desc.terms[i] = {
                .id = component_ids[i],
                .inout = EcsInOut
            };
        }
        // desc.term_count = terms.size();
        
        query = ecs_query_init(world, &desc);
        it = ecs_query_iter(world, query);
    }

    PyQueryIterator& iter() {
        return *this;
    }
        
    py::list next() {
        bool result = true;
        if (next_archetype)
        {
            result = ecs_query_next(&it);
            next_archetype = false;
            i = 0;
            current = it.count;
        }
        if (result)
        {
            ecs_entity_t source = it.entities[i];
            py::list value;
            
            // Create a PyEntity from the flecs entity and append it
            PyEntity py_entity(flecs::entity(world, source));
            value.append(py_entity);
            
            // Append the component data
            for (size_t c = 0; c < component_ids.size(); c++)
            {
                value.append(flecs_component_pyobject[source][component_ids[c]]);
            }
            
            i++;
            if (i == current)
            {
                next_archetype = true;
            }
            return value;
        } else {
            throw pybind11::stop_iteration();
        }
    }
    
    void reset() {
        it = ecs_query_iter(world, query);
        i = 0;
        current = 0;
    }
};

void PythonObserverCallback(ecs_iter_t *it) {
    ecs_world_t *ecs = it->world;
    ecs_entity_t event = it->event;
    ecs_entity_t event_id = it->event_id;
    
    // Find the Python callback associated with this observer
    // For simplicity, we'll store the callback index in the observer's context
    size_t callback_index = reinterpret_cast<size_t>(it->ctx);
    
    if (callback_index < observer_callbacks.size()) {
        py::object callback = observer_callbacks[callback_index];
        
        for (int i = 0; i < it->count; i++) {
            ecs_entity_t entity_id = it->entities[i];
            
            // Create PyEntity wrapper
            flecs::entity flecs_entity(ecs, entity_id);
            PyEntity py_entity(flecs_entity);
            
            py::list args;
            args.append(py_entity);
            
            // Add component data for each term in the observer query
            for (int term_idx = 0; term_idx < it->field_count; term_idx++) {
                ecs_entity_t comp_id = ecs_field_id(it, term_idx);
                
                if (flecs_component_pyobject[entity_id].count(comp_id)) {
                    args.append(flecs_component_pyobject[entity_id][comp_id]);
                }
            }
            
            try {
                // Call Python callback with entity and component data
                callback(*args);
            } catch (const std::exception& e) {
                py::print("Error in observer callback:", e.what());
            }
        }
    }
}



// Simple wrapper for Flecs world
class PyWorld {
public:
    flecs::world world;
    
    PyWorld() {
    }
    
    // Create entity
    PyEntity entity() {
        return PyEntity(world.entity());
    }

    ~PyWorld() {
        observer_callbacks.clear();
    }

    void create_observer(py::function callback, py::args component_types, ecs_entity_t event = EcsOnAdd) {
        // Store the callback
        size_t callback_index = observer_callbacks.size();
        observer_callbacks.push_back(callback);
        
        // Parse component types (reuse query parsing logic)
        std::vector<ecs_entity_t> component_ids;
        for (auto arg : component_types) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            ecs_entity_t component_id = world.entity(component_name.c_str());
            component_ids.push_back(component_id);
        }
        
        // Build observer description
        ecs_observer_desc_t desc = {};
        desc.callback = PythonObserverCallback;
        desc.ctx = reinterpret_cast<void*>(callback_index);
        desc.events[0] = event;
        
        // Set up query terms (same as query logic)
        for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
            desc.query.terms[i] = {
                .id = component_ids[i],
                .inout = EcsInOut
            };
        }
        
        // Create the observer
        ecs_observer_init(world, &desc);
    }
    
    // Convenience method for decorator support
    py::function observer_decorator(py::args component_types, ecs_entity_t event = EcsOnAdd) {
        return py::cpp_function([this, component_types, event](py::function callback) {
            this->create_observer(callback, component_types, event);
            return callback;
        });
    }


    PyEntity entity(const std::string& name, const py::list& components_and_tags) {
        PyEntity entity = PyEntity(world.entity(name.c_str()));
        
        // Process each item in the list
        for (auto item : components_and_tags) {
            try {
                // Cast handle to object
                py::object item_obj = item.cast<py::object>();
                
                // Check if it's a string (tag)
                if (py::isinstance<py::str>(item_obj)) {
                    std::string tag_name = py::str(item_obj);
                    entity.add_tag(tag_name);
                }
                // Otherwise, treat it as a component instance
                else {
                    entity.set_component_instance(item_obj);
                }
            } catch (const std::exception& e) {
                // Handle any errors during processing
                py::print("Error processing component/tag:", e.what());
            }
        }
        
        return entity;
    }
    
    // Create named entity
    PyEntity entity(const std::string& name) {
        return PyEntity(world.entity(name.c_str()));
    }
    
    // Lookup entity by name
    PyEntity lookup(const std::string& name) {
        flecs::entity e = world.lookup(name.c_str());
        return PyEntity(e);
    }
    
    // Progress world (run systems)
    bool progress(float delta_time = 0.0f) {
        return world.progress(delta_time);
    }
    
    // Get info about the world
    std::string info() const {
        return "Flecs World";
    }
    
    // Find entities with a tag
    std::vector<PyEntity> find_with_tag(const std::string& tag_name) {
        std::vector<PyEntity> entities;
        flecs::entity tag = world.lookup(tag_name.c_str());
        if (tag.is_valid()) {
            world.query_builder().with(tag).build().each([&entities](flecs::entity e) {
                entities.push_back(PyEntity(e));
            });
        }
        return entities;
    }

    std::vector<PyEntity> find_with_tags(const std::vector<std::string>& tag_names) {
        std::vector<PyEntity> entities;
        flecs::query_builder<> qb = world.query_builder();

        for (const std::string& tag_name : tag_names) {
            flecs::entity tag = world.lookup(tag_name.c_str());
            if (!tag.is_valid()) {
                // If any tag is not valid, no entities can match all tags
                return {}; 
            }
            qb.with(tag); 
        }

        if (!tag_names.empty()) {
            qb.build().each([&entities](flecs::entity e) {
                entities.push_back(PyEntity(e));
            });
        }

        return entities;
    }

    // Create a query for a specific component type
    PyQueryIterator query(py::args args) {
        return PyQueryIterator(world, args);
    }

};


PYBIND11_MODULE(_core, m) {
    m.doc() = R"pbdoc(
        Flecs Python Bindings
        --------------------

        A simple Python interface to the Flecs Entity Component System.

        .. currentmodule:: flecs

        .. autosummary::
           :toctree: _generate

           PyWorld
           PyEntity
    )pbdoc";

    m.attr("OnAdd") = EcsOnAdd;
    m.attr("OnRemove") = EcsOnRemove;
    m.attr("OnSet") = EcsOnSet;
    
    // Bind PyEntity class
    py::class_<PyEntity>(m, "Entity")
        .def("id", &PyEntity::id)
        .def("name", &PyEntity::name)
        .def("set_name", &PyEntity::set_name)
        .def("is_alive", &PyEntity::is_alive)
        .def("destroy", &PyEntity::destroy)
        .def("has_tag", &PyEntity::has_tag)
        .def("remove_tag", &PyEntity::remove_tag)
        .def("add", &PyEntity::add_tag)
        .def("set", &PyEntity::set_component_instance)
        .def("get", &PyEntity::get_component) // New get method for components
        .def("__repr__", [](const PyEntity& e) {
            return "Entity(id=" + std::to_string(e.id()) + ", name=\"" + e.name() + "\")";
        });

        // Bind PyQuery with iterator support
    py::class_<PyQueryIterator>(m, "Query")
        .def("__iter__", &PyQueryIterator::iter, 
             py::return_value_policy::reference_internal)
        .def("__next__", &PyQueryIterator::next)
        .def("reset", &PyQueryIterator::reset);
    
    // Bind PyWorld class
    py::class_<PyWorld>(m, "World")
        .def(py::init<>())
        .def("entity", py::overload_cast<>(&PyWorld::entity))
        .def("entity", py::overload_cast<const std::string&>(&PyWorld::entity))
        .def("entity", py::overload_cast<const std::string&, const py::list&>(&PyWorld::entity))
        .def("lookup", &PyWorld::lookup)
        .def("progress", &PyWorld::progress, py::arg("delta_time") = 0.0f)
        .def("info", &PyWorld::info)
        .def("find_with_tag", &PyWorld::find_with_tag)
        .def("find_with_tags", &PyWorld::find_with_tags)
        .def("query", &PyWorld::query)
        .def("observer", &PyWorld::observer_decorator, py::arg("event") = EcsOnAdd)
        .def("__repr__", [](const PyWorld& w) {
            return w.info();
        });

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}