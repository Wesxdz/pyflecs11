#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <flecs.h>
#include <string>
#include <vector>
#include <map>
#include <typeindex> // For std::type_index

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

// Define a C++ component to hold a Python object
struct PyComponentWrapper {
    py::object obj;
};

// Global map to store Python type info (conceptually, a mapping from Flecs component ID to Python type)
// This is a simplified approach. In a more robust system, you might register Python types with Flecs.
static std::map<flecs::entity_t, py::object> flecs_id_to_py_type_map;


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
        flecs::entity flecs_comp_id = entity.world().entity(type_name.c_str());
        py::print("DEBUG (add_component): Adding component:", type_name, "with Flecs ID:", flecs_comp_id.id()); // New line
        flecs_id_to_py_type_map[flecs_comp_id.id()] = py_type;
        entity.set<PyComponentWrapper>(flecs_comp_id, {py_component_instance});
        return this;
    }

    // Get a Python component from the entity
    py::object get_component(py::object py_component_type) {
        std::string type_name = py::str(py_component_type.attr("__name__"));
        
        // Log using py::print
        py::print("DEBUG (get_component): Attempting to get component:", type_name); // New line

        flecs::entity flecs_comp_id = entity.world().lookup(type_name.c_str());

        if (!flecs_comp_id.is_valid()) {
            py::print("DEBUG (get_component): Flecs ID for", type_name, "is NOT valid."); // New line
            return py::none();
        }
        py::print("DEBUG (get_component): Flecs ID for", type_name, "is valid. ID:", flecs_comp_id.id()); // New line

        if (entity.has<PyComponentWrapper>(flecs_comp_id)) {
            py::print("DEBUG (get_component): Entity HAS component", type_name); // New line
            const PyComponentWrapper& wrapper = entity.get<PyComponentWrapper>(flecs_comp_id);
            if (wrapper.obj.is_none()) {
                py::print("DEBUG (get_component): Retrieved Python object is None."); // New line
            } else {
                py::print("DEBUG (get_component): Successfully retrieved Python object."); // New line
            }
            return wrapper.obj;
        }
        py::print("DEBUG (get_component): Entity DOES NOT HAVE component", type_name); // New line
        return py::none();
    }
};

// Simple wrapper for Flecs world
class PyWorld {
public:
    flecs::world world;
    
    PyWorld() {
        // Register the PyComponentWrapper with Flecs
        world.component<PyComponentWrapper>();
    }
    
    // Create entity
    PyEntity entity() {
        return PyEntity(world.entity());
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
    
    // Bind PyWorld class
    py::class_<PyWorld>(m, "World")
        .def(py::init<>())
        .def("entity", py::overload_cast<>(&PyWorld::entity))
        .def("entity", py::overload_cast<const std::string&>(&PyWorld::entity))
        .def("lookup", &PyWorld::lookup)
        .def("progress", &PyWorld::progress, py::arg("delta_time") = 0.0f)
        .def("info", &PyWorld::info)
        .def("find_with_tag", &PyWorld::find_with_tag)
        .def("find_with_tags", &PyWorld::find_with_tags)
        .def("__repr__", [](const PyWorld& w) {
            return w.info();
        });

    // Keep the original functions for backward compatibility
    m.def("add", [](int i, int j) {
        PyWorld world;
        return i + j;
    }, R"pbdoc(
        Add two numbers (with Flecs world creation)
    )pbdoc");

    m.def("subtract", [](int i, int j) { 
        return i - j; 
    }, R"pbdoc(
        Subtract two numbers
    )pbdoc");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}