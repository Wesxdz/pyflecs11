#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <flecs.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <typeindex> // For std::type_index

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

// Each (non-tag) component or relationship id on an entity is mapped to a py::object
// This allows arbitrary Python classes/variables (such as neural networks) as components
static std::map<flecs::id_t, std::map<flecs::id_t, py::object>> flecs_component_pyobject;
static std::vector<py::object> observer_callbacks;
static std::vector<py::object> system_callbacks;
static std::vector<py::object> observer_iter_callbacks;
static std::vector<py::object> system_iter_callbacks;

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

    std::string path() {
        return entity.path().c_str();
    }

    std::vector<PyEntity> children() {
        std::vector<PyEntity> children_vec;
        entity.children([&](flecs::entity child) {
            children_vec.push_back(PyEntity(child));
        });
        return children_vec; 
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
    
    // Add a relationship (relation, target) - both as strings
    PyEntity* add_relationship(const std::string& relation_name, const std::string& target_name) {
        flecs::entity relation = entity.world().entity(relation_name.c_str());
        flecs::entity target = entity.world().entity(target_name.c_str());
        entity.add(relation, target);
        return this;
    }
    
    // Add a relationship (relation, target) - relation as string, target as entity
    PyEntity* add_relationship(const std::string& relation_name, PyEntity& target) {
        flecs::entity relation = entity.world().entity(relation_name.c_str());
        entity.add(relation, target.entity);
        return this;
    }
    
    // Add a relationship (relation, target) - both as entities
    PyEntity* add_relationship(PyEntity& relation, PyEntity& target) {
        entity.add(relation.entity, target.entity);
        return this;
    }
    
    // Add a relationship (relation, target) - relation as entity, target as string
    PyEntity* add_relationship(PyEntity& relation, const std::string& target_name) {
        flecs::entity target = entity.world().entity(target_name.c_str());
        entity.add(relation.entity, target);
        return this;
    }
    
    // Add a relationship with Python component as target
    PyEntity* add_relationship(const std::string& relation_name, py::object py_component_instance) {
        flecs::entity relation = entity.world().entity(relation_name.c_str());
        
        // Get the component type name
        py::object py_type = py::type::of(py_component_instance);
        std::string component_type_name = py::str(py_type.attr("__name__"));
        
        // Get or create the component entity
        flecs::entity component_entity = entity.world().entity(component_type_name.c_str());
        
        // Store the Python object
        flecs_component_pyobject[entity.id()][component_entity.id()] = py_component_instance;
        
        // Add the relationship
        entity.add(relation, component_entity);
        
        return this;
    }
    
    // Add a relationship with Python component as relation
    PyEntity* add_relationship(py::object py_component_instance, const std::string& target_name) {
        py::object py_type = py::type::of(py_component_instance);
        std::string component_type_name = py::str(py_type.attr("__name__"));
        
        flecs::entity component_entity = entity.world().entity(component_type_name.c_str());
        flecs::entity target = entity.world().entity(target_name.c_str());
        
        // Store the Python object
        flecs_component_pyobject[entity.id()][component_entity.id()] = py_component_instance;
        
        entity.add(component_entity, target);
        
        return this;
    }
    
    // Add a relationship with Python component as both relation and target
    PyEntity* add_relationship(py::object py_relation_instance, py::object py_target_instance) {
        py::object py_rel_type = py::type::of(py_relation_instance);
        py::object py_tgt_type = py::type::of(py_target_instance);
        
        std::string rel_type_name = py::str(py_rel_type.attr("__name__"));
        std::string tgt_type_name = py::str(py_tgt_type.attr("__name__"));
        
        flecs::entity rel_entity = entity.world().entity(rel_type_name.c_str());
        flecs::entity tgt_entity = entity.world().entity(tgt_type_name.c_str());
        
        // Store the Python objects
        flecs_component_pyobject[entity.id()][rel_entity.id()] = py_relation_instance;
        flecs_component_pyobject[entity.id()][tgt_entity.id()] = py_target_instance;
        
        entity.add(rel_entity, tgt_entity);
        
        return this;
    }
    
    // Overloaded add method that handles tags, relationships, and components
    PyEntity* add(const std::string& tag_or_relation_name) {
        // Single argument - treat as tag
        return add_tag(tag_or_relation_name);
    }
    
    PyEntity* add(const std::string& relation_name, const std::string& target_name) {
        // Two string arguments - treat as relationship
        return add_relationship(relation_name, target_name);
    }
    
    PyEntity* add(const std::string& relation_name, PyEntity& target) {
        // String relation, entity target
        return add_relationship(relation_name, target);
    }
    
    PyEntity* add(PyEntity& relation, PyEntity& target) {
        // Entity relation, entity target
        return add_relationship(relation, target);
    }
    
    PyEntity* add(PyEntity& relation, const std::string& target_name) {
        // Entity relation, string target
        return add_relationship(relation, target_name);
    }
    
    PyEntity* add(const std::string& relation_name, py::object py_component_instance) {
        // String relation, component target
        return add_relationship(relation_name, py_component_instance);
    }
    
    PyEntity* add(py::object py_component_instance, const std::string& target_name) {
        // Component relation, string target
        return add_relationship(py_component_instance, target_name);
    }
    
    PyEntity* add(py::object py_relation_instance, py::object py_target_instance) {
        // Component relation, component target
        return add_relationship(py_relation_instance, py_target_instance);
    }

    PyEntity* child_of(PyEntity& parent)
    {
        entity.child_of(parent.entity);
        return this;
    }
    
    // Check if entity has a tag
    bool has_tag(const std::string& tag_name) {
        flecs::entity tag = entity.world().lookup(tag_name.c_str());
        return tag.is_valid() && entity.has(tag);
    }
    
    // Check if entity has a relationship (string, string)
    bool has_relationship(const std::string& relation_name, const std::string& target_name) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        flecs::entity target = entity.world().lookup(target_name.c_str());
        return relation.is_valid() && target.is_valid() && entity.has(relation, target);
    }
    
    // Check if entity has a relationship (string, entity)
    bool has_relationship(const std::string& relation_name, PyEntity& target) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        return relation.is_valid() && entity.has(relation, target.entity);
    }
    
    // Check if entity has a relationship (entity, entity)
    bool has_relationship(PyEntity& relation, PyEntity& target) {
        return entity.has(relation.entity, target.entity);
    }
    
    // Check if entity has a relationship (entity, string)
    bool has_relationship(PyEntity& relation, const std::string& target_name) {
        flecs::entity target = entity.world().lookup(target_name.c_str());
        return target.is_valid() && entity.has(relation.entity, target);
    }
    
    // Check if entity has a relationship with component (string, component)
    bool has_relationship(const std::string& relation_name, py::object py_component_type) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        std::string component_type_name = py::str(py_component_type.attr("__name__"));
        flecs::entity component_entity = entity.world().lookup(component_type_name.c_str());
        return relation.is_valid() && component_entity.is_valid() && entity.has(relation, component_entity);
    }
    
    // Check if entity has a relationship with component (component, string)
    bool has_relationship(py::object py_component_type, const std::string& target_name) {
        std::string component_type_name = py::str(py_component_type.attr("__name__"));
        flecs::entity component_entity = entity.world().lookup(component_type_name.c_str());
        flecs::entity target = entity.world().lookup(target_name.c_str());
        return component_entity.is_valid() && target.is_valid() && entity.has(component_entity, target);
    }
    
    // Check if entity has a relationship with components (component, component)
    bool has_relationship(py::object py_relation_type, py::object py_target_type) {
        std::string rel_type_name = py::str(py_relation_type.attr("__name__"));
        std::string tgt_type_name = py::str(py_target_type.attr("__name__"));
        flecs::entity rel_entity = entity.world().lookup(rel_type_name.c_str());
        flecs::entity tgt_entity = entity.world().lookup(tgt_type_name.c_str());
        return rel_entity.is_valid() && tgt_entity.is_valid() && entity.has(rel_entity, tgt_entity);
    }
    
    // Overloaded has method
    bool has(const std::string& tag_or_relation_name) {
        return has_tag(tag_or_relation_name);
    }
    
    bool has(const std::string& relation_name, const std::string& target_name) {
        return has_relationship(relation_name, target_name);
    }
    
    bool has(const std::string& relation_name, PyEntity& target) {
        return has_relationship(relation_name, target);
    }
    
    bool has(PyEntity& relation, PyEntity& target) {
        return has_relationship(relation, target);
    }
    
    bool has(PyEntity& relation, const std::string& target_name) {
        return has_relationship(relation, target_name);
    }
    
    bool has(const std::string& relation_name, py::object py_component_type) {
        return has_relationship(relation_name, py_component_type);
    }
    
    bool has(py::object py_component_type, const std::string& target_name) {
        return has_relationship(py_component_type, target_name);
    }
    
    bool has(py::object py_relation_type, py::object py_target_type) {
        return has_relationship(py_relation_type, py_target_type);
    }
    
    // Remove a tag
    void remove_tag(const std::string& tag_name) {
        flecs::entity tag = entity.world().lookup(tag_name.c_str());
        if (tag.is_valid()) {
            entity.remove(tag);
        }
    }
    
    // Remove a relationship (string, string)
    void remove_relationship(const std::string& relation_name, const std::string& target_name) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        flecs::entity target = entity.world().lookup(target_name.c_str());
        if (relation.is_valid() && target.is_valid()) {
            entity.remove(relation, target);
        }
    }
    
    // Remove a relationship (string, entity)
    void remove_relationship(const std::string& relation_name, PyEntity& target) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        if (relation.is_valid()) {
            entity.remove(relation, target.entity);
        }
    }
    
    // Remove a relationship (entity, entity)
    void remove_relationship(PyEntity& relation, PyEntity& target) {
        entity.remove(relation.entity, target.entity);
    }
    
    // Remove a relationship (entity, string)
    void remove_relationship(PyEntity& relation, const std::string& target_name) {
        flecs::entity target = entity.world().lookup(target_name.c_str());
        if (target.is_valid()) {
            entity.remove(relation.entity, target);
        }
    }
    
    // Overloaded remove method
    void remove(const std::string& tag_or_relation_name) {
        remove_tag(tag_or_relation_name);
    }
    
    void remove(const std::string& relation_name, const std::string& target_name) {
        remove_relationship(relation_name, target_name);
    }
    
    void remove(const std::string& relation_name, PyEntity& target) {
        remove_relationship(relation_name, target);
    }
    
    void remove(PyEntity& relation, PyEntity& target) {
        remove_relationship(relation, target);
    }
    
    void remove(PyEntity& relation, const std::string& target_name) {
        remove_relationship(relation, target_name);
    }
    
    void remove_component(py::object py_component_type) {
        std::string component_type_name = py::str(py_component_type.attr("__name__"));
        flecs::entity component_entity = entity.world().lookup(component_type_name.c_str());
        
        if (component_entity.is_valid()) {
            // Remove the component from Flecs
            entity.remove(component_entity);
            
            // Remove the stored Python object
            flecs_component_pyobject[entity.id()].erase(component_entity.id());
        }
    }

    // Add this overload to your existing remove methods (after the last remove overload):
    void remove(py::object py_component_type) {
        remove_component(py_component_type);
    }

    // Get all targets for a given relation (string)
    std::vector<PyEntity> get_targets(const std::string& relation_name) {
        std::vector<PyEntity> targets;
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        
        if (relation.is_valid()) {
            entity.each(relation, [&targets](flecs::entity target) {
                targets.push_back(PyEntity(target));
            });
        }
        
        return targets;
    }
    
    // Get all targets for a given relation (entity)
    std::vector<PyEntity> get_targets(PyEntity& relation) {
        std::vector<PyEntity> targets;
        
        entity.each(relation.entity, [&targets](flecs::entity target) {
            targets.push_back(PyEntity(target));
        });
        
        return targets;
    }
    
    // Get the component data for a relationship pair
    py::object get_relationship_component(const std::string& relation_name, const std::string& target_name) {
        flecs::entity relation = entity.world().lookup(relation_name.c_str());
        flecs::entity target = entity.world().lookup(target_name.c_str());
        
        if (relation.is_valid() && target.is_valid()) {
            // Try to get component data stored for this relationship pair
            auto pair_id = ecs_pair(relation.id(), target.id());
            if (flecs_component_pyobject[entity.id()].count(pair_id)) {
                return flecs_component_pyobject[entity.id()][pair_id];
            }
        }
        
        return py::none();
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
        
        flecs::entity flecs_comp_id = entity.world().lookup(type_name.c_str());

        if (!flecs_comp_id.is_valid()) {
            return py::none();
        }

        if (flecs_component_pyobject[entity.id()].count(flecs_comp_id.id())) {
            return flecs_component_pyobject[entity.id()][flecs_comp_id.id()];
        }
        return py::none();
    }
};

class PyIterator {
private:
    ecs_iter_t* it;
    flecs::world world;
    
public:
    PyIterator(ecs_iter_t* iter, flecs::world w) : it(iter), world(w) {}
    
    // Return the actual event constant for direct comparison
    ecs_entity_t event() const {
        return it->event;
    }
    
    // Keep string version as event_name() for debugging
    std::string event_name() const {
        if (it->event) {
            const char* name = ecs_get_name(it->world, it->event);
            return name ? std::string(name) : "";
        }
        return "";
    }
    
    // Return the actual event_id constant  
    ecs_entity_t event_id() const {
        return it->event_id;
    }
    
    // Keep string version as event_id_name() for debugging
    std::string event_id_name() const {
        if (it->event_id) {
            const char* name = ecs_get_name(it->world, it->event_id);
            return name ? std::string(name) : "";
        }
        return "";
    }
    
    // Get entity count in this iteration
    int32_t count() const {
        return it->count;
    }
    
    // Get entity at index
    PyEntity entity(size_t index) const {
        if (index < static_cast<size_t>(it->count)) {
            return PyEntity(flecs::entity(world, it->entities[index]));
        }
        throw std::out_of_range("Entity index out of range");
    }
    
    // Get delta time (for systems)
    float delta_time() const {
        return it->delta_time;
    }
    
    // Get field count
    int32_t field_count() const {
        return it->field_count;
    }
    
    // Check if field is set
    bool is_set(int32_t field) const {
        return ecs_field_is_set(it, field);
    }
    
    // Get component for entity at index and field
    py::object get_component(size_t entity_index, int32_t field) const {
        if (entity_index >= static_cast<size_t>(it->count)) {
            throw std::out_of_range("Entity index out of range");
        }
        
        ecs_entity_t entity_id = it->entities[entity_index];
        ecs_id_t field_id = ecs_field_id(it, field);
        
        if (flecs_component_pyobject[entity_id].count(field_id)) {
            return flecs_component_pyobject[entity_id][field_id];
        }
        
        return py::none();
    }
};

class PyQueryIterator {
private:
    flecs::world world;
    ecs_query_t* query;
    ecs_iter_t it;
    
    struct QueryTerm {
        ecs_entity_t id;
        bool is_relationship = false;
        bool is_variable_source = false;
        std::string src_name;
        bool is_wildcard_target = false;
        bool is_variable_target = false;
        bool is_wildcard_relation = false;
        bool is_variable_relation = false;
        std::string first_name;
        std::string second_name;
        ecs_entity_t relation_id = 0;
        ecs_entity_t target_id = 0;
        bool is_tag = false;
    };
    
    std::vector<QueryTerm> query_terms;
    std::set<std::string> var_set;
    std::vector<std::string> var_names;
    std::vector<int> var_indices;
    int non_tag_component_count = 0;
    bool next_archetype = true;
    size_t i = 0;
    size_t current = 0;

    // Helper function to check if string is a variable
    bool is_variable(const std::string& str) {
        return !str.empty() && str[0] == '$';
    }
    
    // Helper function to parse entity from string or variable
    ecs_entity_t parse_entity_or_variable(const std::string& str, bool& is_var, std::string& var_name) {
        if (is_variable(str)) {
            is_var = true;
            var_name = str.substr(1); // Remove the $ prefix
            return 0; // Variables don't have concrete IDs until runtime
        } else if (str == "*") {
            return EcsWildcard;
        } else if (str == "_") {
            return EcsThis; // Self/source entity
        } else {
            is_var = false;
            return world.entity(str.c_str()).id();
        }
    }
    
public:
    PyQueryIterator(flecs::world& w, py::args args) : world(w) {
        // Parse query arguments
        py::print("Creating query iterator");
        for (auto arg : args) {
            QueryTerm term;
            py::object comp_type = arg.cast<py::object>();
            
            // Check if this is a tuple (relationship pair)
            if (py::isinstance<py::tuple>(comp_type)) {
                py::tuple rel_pair = comp_type.cast<py::tuple>();
                
                if (rel_pair.size() == 2) {
                    // Handle 2-tuple (existing logic)
                    term.is_relationship = true;
                    
                    // Parse relation (first element)
                    py::object relation = rel_pair[0];
                    if (py::isinstance<py::str>(relation)) {
                        std::string rel_name = relation.cast<std::string>();
                        if (rel_name == "*") {
                            term.is_wildcard_relation = true;
                            term.relation_id = EcsWildcard;
                        } else if (is_variable(rel_name))
                        {
                            py::print("Is variable source ");
                            term.is_variable_source = true;
                            term.src_name = rel_name;
                        }   
                        else {
                            term.relation_id = world.entity(rel_name.c_str()).id();
                        }
                    } else if (py::isinstance<PyEntity>(relation)) {
                        // Handle PyEntity directly
                        PyEntity rel_entity = relation.cast<PyEntity>();
                        term.relation_id = rel_entity.entity.id();
                    } else {
                        // Assume it's a component type
                        std::string rel_name = py::str(relation.attr("__name__"));
                        term.relation_id = world.entity(rel_name.c_str()).id();
                    }
                    
                    // Parse target (second element)
                    py::object target = rel_pair[1];
                    if (py::isinstance<py::str>(target)) {
                        std::string tgt_name = target.cast<std::string>();
                        if (tgt_name == "*") {
                            term.is_wildcard_target = true;
                            term.target_id = EcsWildcard;
                        } 
                        else if (is_variable(tgt_name))
                        {
                            term.is_variable_target = true;
                            term.second_name = tgt_name;
                        } 
                        else {
                            term.target_id = world.entity(tgt_name.c_str()).id();
                        }
                    } else if (py::isinstance<PyEntity>(target)) {
                        // Handle PyEntity directly
                        PyEntity tgt_entity = target.cast<PyEntity>();
                        term.target_id = tgt_entity.entity.id();
                    } else {
                        // Assume it's a component type
                        std::string tgt_name = py::str(target.attr("__name__"));
                        term.target_id = world.entity(tgt_name.c_str()).id();
                    }
                    
                    // Create pair ID
                    term.id = ecs_pair(term.relation_id, term.target_id);
                    term.is_tag = true; // Relationships are typically tags unless they have component data
                    
                } else if (rel_pair.size() == 3) {
                    // Handle 3-tuple (variable, relation, target)
                    term.is_relationship = true;
                    
                    // Parse variable (0th element - always a variable)
                    py::object variable = rel_pair[0];
                    if (py::isinstance<py::str>(variable)) {
                        std::string var_name = variable.cast<std::string>();
                        if (is_variable(var_name)) {
                            term.is_variable_source = true;
                            term.src_name = var_name;
                        } else {
                            throw std::runtime_error("First element of 3-tuple must be a variable");
                        }
                    } else {
                        throw std::runtime_error("First element of 3-tuple must be a variable string");
                    }
                    
                    // Parse relation (first element)
                    py::object relation = rel_pair[1];
                    if (py::isinstance<py::str>(relation)) {
                        std::string rel_name = relation.cast<std::string>();
                        if (rel_name == "*") {
                            term.is_wildcard_relation = true;
                            term.relation_id = EcsWildcard;
                        } else if (is_variable(rel_name))
                        {
                            py::print("Is variable relationship");
                            term.is_variable_relation = true;
                            term.first_name = rel_name;
                        }   
                        else {
                            term.relation_id = world.entity(rel_name.c_str()).id();
                        }
                    } else if (py::isinstance<PyEntity>(relation)) {
                        // Handle PyEntity directly
                        PyEntity rel_entity = relation.cast<PyEntity>();
                        term.relation_id = rel_entity.entity.id();
                    } else {
                        // Assume it's a component type
                        std::string rel_name = py::str(relation.attr("__name__"));
                        term.relation_id = world.entity(rel_name.c_str()).id();
                    }
                    
                    // Parse target (second element)
                    py::object target = rel_pair[2];
                    if (py::isinstance<py::str>(target)) {
                        std::string tgt_name = target.cast<std::string>();
                        if (tgt_name == "*") {
                            term.is_wildcard_target = true;
                            term.target_id = EcsWildcard;
                        } 
                        else if (is_variable(tgt_name))
                        {
                            term.is_variable_target = true;
                            term.second_name = tgt_name;
                        } 
                        else {
                            term.target_id = world.entity(tgt_name.c_str()).id();
                        }
                    } else if (py::isinstance<PyEntity>(target)) {
                        // Handle PyEntity directly
                        PyEntity tgt_entity = target.cast<PyEntity>();
                        term.target_id = tgt_entity.entity.id();
                    } else {
                        // Assume it's a component type
                        std::string tgt_name = py::str(target.attr("__name__"));
                        term.target_id = world.entity(tgt_name.c_str()).id();
                    }
                    
                    // Create pair ID
                    term.id = ecs_pair(term.relation_id, term.target_id);
                    term.is_tag = true; // Relationships are typically tags unless they have component data
                    
                } else {
                    throw std::runtime_error("Relationship tuple must have exactly 2 or 3 elements");
                }
            } else {
                // Regular component or tag
                if (py::isinstance<py::str>(comp_type)) {
                    // Tag
                    std::string component_name = comp_type.cast<std::string>();
                    term.id = world.entity(component_name.c_str()).id();
                    term.is_tag = true;
                } else {
                    // Component
                    std::string component_name = py::str(comp_type.attr("__name__"));
                    term.id = world.entity(component_name.c_str()).id();
                    term.is_tag = false;
                    non_tag_component_count++;
                }
            }

            if (term.is_variable_target && !term.is_variable_source)
            {
                if (!var_set.count("$this"))
                {
                    var_set.insert("$this");
                    var_names.push_back("this");
                }
            }
            if (term.is_variable_source)
            {
                if (!var_set.count(term.src_name))
                {
                    var_set.insert(term.src_name);
                    var_names.push_back(term.src_name.substr(1));
                }
            }
            if (term.is_variable_relation)
            {
                if (!var_set.count(term.first_name))
                {
                    var_set.insert(term.first_name);
                    var_names.push_back(term.first_name.substr(1));
                }
            }
            if (term.is_variable_target)
            {
                if (!var_set.count(term.second_name))
                {
                    var_set.insert(term.second_name);
                    var_names.push_back(term.second_name.substr(1));
                }
            }
            
            query_terms.push_back(term);
        }
        // Build query description
        py::print("Build query description");
        ecs_query_desc_t desc = {};
        for (size_t i = 0; i < query_terms.size() && i < 32; ++i) {
            if (query_terms[i].is_variable_source && query_terms[i].is_variable_target)
            {
                py::print("Add variable source term ", query_terms[i].src_name.c_str());
                desc.terms[i].src.name = query_terms[i].src_name.c_str();
                desc.terms[i].first.id = query_terms[i].relation_id;
                py::print("Add variable target term ", query_terms[i].second_name.c_str());
                desc.terms[i].second.name = query_terms[i].second_name.c_str();
            }
            else if (query_terms[i].is_variable_source)
            {
                py::print("Add variable source term ", query_terms[i].src_name.c_str());
                desc.terms[i].first.id = query_terms[i].target_id;
                desc.terms[i].src.name = query_terms[i].src_name.c_str();
            }
            else if (query_terms[i].is_variable_target)
            {
                py::print("Add variable target term ", query_terms[i].second_name.c_str());
                desc.terms[i].first.id = query_terms[i].relation_id;
                desc.terms[i].second.name = query_terms[i].second_name.c_str();
            }
            else if (query_terms[i].is_variable_relation)
            {
                py::print("Add variable relationship term ", query_terms[i].first_name.c_str());
                desc.terms[i].first.name = query_terms[i].first_name.c_str();
                // desc.terms[i].first.name = query_terms[i].second_name.c_str();
            } else
            {
                desc.terms[i].id = query_terms[i].id;
            }
        }
        
        query = ecs_query_init(world, &desc);
        for (std::string var_name : var_names)
        {
            var_indices.push_back(ecs_query_find_var(query, var_name.c_str()));
        }
        it = ecs_query_iter(world, query);
    }
    
    PyQueryIterator& iter() {
        return *this;
    }
    
    py::list next() {
        bool result = true;
        if (next_archetype) {
            result = ecs_query_next(&it);
            next_archetype = false;
            i = 0;
            current = it.count;
        }
        
        if (result) {
            ecs_entity_t source = it.entities[i];
            py::list value;
            
            // Create a PyEntity from $this flecs entity as the first argument

            int z = 0;
            for (int var_index : var_indices)
            {
                py::print(var_names[z]);
                PyEntity py_entity(flecs::entity(world, ecs_iter_get_var(&it, var_index)));
                value.append(py_entity);
                z++;
            }
            
            // Process each query term
            for (size_t term_idx = 0; term_idx < query_terms.size(); term_idx++) {
                const QueryTerm& term = query_terms[term_idx];
                
                if (term.is_relationship) {
                    if (term.is_wildcard_target || term.is_wildcard_relation) {
                        // For wildcard relationships, we need to get the actual pair from the iterator
                        ecs_id_t actual_id = ecs_field_id(&it, term_idx);
                        
                        if (ECS_IS_PAIR(actual_id)) {
                            ecs_entity_t actual_relation = ecs_pair_first(world, actual_id);
                            ecs_entity_t actual_target = ecs_pair_second(world, actual_id);
                            
                            if (term.is_wildcard_target) {
                                // Return the actual target entity
                                PyEntity target_entity(flecs::entity(world, actual_target));
                                value.append(target_entity);
                            }
                            
                            if (term.is_wildcard_relation) {
                                // Return the actual relation entity
                                PyEntity relation_entity(flecs::entity(world, actual_relation));
                                value.append(relation_entity);
                            }
                            
                            // Check if there's component data for this relationship
                            if (flecs_component_pyobject[source].count(actual_id)) {
                                value.append(flecs_component_pyobject[source][actual_id]);
                            }
                        }
                    } else {
                        // Specific relationship pair
                        if (flecs_component_pyobject[source].count(term.id)) {
                            value.append(flecs_component_pyobject[source][term.id]);
                        }
                    }
                } else if (!term.is_tag) {
                    // Regular component
                    if (flecs_component_pyobject[source].count(term.id)) {
                        value.append(flecs_component_pyobject[source][term.id]);
                    }
                }
                // Tags don't add anything to the result tuple
            }
            
            i++;
            if (i == current) {
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
        next_archetype = true;
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

void PythonSystemCallback(ecs_iter_t *it) {
    ecs_world_t *ecs = it->world;
    
    size_t callback_index = reinterpret_cast<size_t>(it->ctx);
    
    if (callback_index < system_callbacks.size()) {
        py::object callback = system_callbacks[callback_index];
        
        for (int i = 0; i < it->count; i++) {
            ecs_entity_t entity_id = it->entities[i];
            
            // Create PyEntity wrapper
            flecs::entity flecs_entity(ecs, entity_id);
            PyEntity py_entity(flecs_entity);
            
            py::list args;
            args.append(py_entity);
            
            for (int term_idx = 0; term_idx < it->field_count; term_idx++) {
                ecs_entity_t comp_id = ecs_field_id(it, term_idx);
                
                if (flecs_component_pyobject[entity_id].count(comp_id)) {
                    py::object& stored_component = flecs_component_pyobject[entity_id][comp_id];
                    args.append(stored_component);
                }
            }
            
            try {
                // Call Python callback with entity and component references
                callback(*args);
            } catch (const std::exception& e) {
                py::print("Error in system callback:", e.what());
            }
        }
    }
}

void PythonObserverIterCallback(ecs_iter_t *it) {
    size_t callback_index = reinterpret_cast<size_t>(it->ctx);
    
    if (callback_index < observer_iter_callbacks.size()) {
        py::object callback = observer_iter_callbacks[callback_index];
        
        // Create PyIterator wrapper
        flecs::world world(it->world);
        PyIterator py_iter(it, world);
        
        // Call Python callback with iterator and component arrays
        py::list args;
        args.append(py_iter);
        
        // Add component data for each field
        for (int field = 0; field < it->field_count; field++) {
            py::list field_components;
            ecs_id_t field_id = ecs_field_id(it, field);
            
            for (int i = 0; i < it->count; i++) {
                ecs_entity_t entity_id = it->entities[i];
                if (flecs_component_pyobject[entity_id].count(field_id)) {
                    field_components.append(flecs_component_pyobject[entity_id][field_id]);
                } else {
                    field_components.append(py::none());
                }
            }
            args.append(field_components);
        }
        
        try {
            callback(*args);
        } catch (const std::exception& e) {
            py::print("Error in iterator observer callback:", e.what());
        }
    }
}

// Iterator-based system callback
void PythonSystemIterCallback(ecs_iter_t *it) {
    size_t callback_index = reinterpret_cast<size_t>(it->ctx);
    
    if (callback_index < system_iter_callbacks.size()) {
        py::object callback = system_iter_callbacks[callback_index];
        
        flecs::world world(it->world);
        PyIterator py_iter(it, world);
        
        py::list args;
        args.append(py_iter);
        
        // Add component arrays for each field
        for (int field = 0; field < it->field_count; field++) {
            py::list field_components;
            ecs_id_t field_id = ecs_field_id(it, field);
            
            for (int i = 0; i < it->count; i++) {
                ecs_entity_t entity_id = it->entities[i];
                if (flecs_component_pyobject[entity_id].count(field_id)) {
                    field_components.append(flecs_component_pyobject[entity_id][field_id]);
                } else {
                    field_components.append(py::none());
                }
            }
            args.append(field_components);
        }
        
        try {
            callback(*args);
        } catch (const std::exception& e) {
            py::print("Error in iterator system callback:", e.what());
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
        system_callbacks.clear();
        observer_iter_callbacks.clear();
        system_iter_callbacks.clear();
    }

    void create_observer(py::function callback, py::args component_types, py::list events = py::list()) {
        size_t callback_index = observer_callbacks.size();
        observer_callbacks.push_back(callback);
        
        std::vector<ecs_entity_t> component_ids;
        for (auto arg : component_types) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            ecs_entity_t component_id = world.entity(component_name.c_str());
            component_ids.push_back(component_id);
        }
        
        // Parse events list - default to OnAdd if empty
        std::vector<ecs_entity_t> event_list;
        if (events.size() == 0) {
            event_list.push_back(EcsOnAdd);
        } else {
            for (auto event : events) {
                ecs_entity_t event_id = event.cast<ecs_entity_t>();
                event_list.push_back(event_id);
            }
        }
        
        // Create observer for each event
        for (ecs_entity_t event : event_list) {
            ecs_observer_desc_t desc = {};
            desc.callback = PythonObserverCallback;
            desc.ctx = reinterpret_cast<void*>(callback_index);
            desc.events[0] = event;
            
            for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
                desc.query.terms[i] = {
                    .id = component_ids[i],
                    .inout = EcsInOut
                };
            }
            
            ecs_observer_init(world, &desc);
        }
    }

    void create_system(py::function callback, py::args component_types) {
        py::print("Creating system with", component_types.size(), "components");
        
        // Store the callback
        size_t callback_index = system_callbacks.size();
        system_callbacks.push_back(callback);
        
        // Parse component types
        std::vector<ecs_entity_t> component_ids;
        for (auto arg : component_types) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            py::print("  Component:", component_name);
            ecs_entity_t component_id = world.entity(component_name.c_str()).id();
            component_ids.push_back(component_id);
        }
        
        // CRITICAL FIX: Create the system entity first with proper phase setup
        ecs_entity_t system_entity = ecs_new(world);
        ecs_add_pair(world, system_entity, EcsDependsOn, EcsOnUpdate);
        ecs_add_id(world, system_entity, EcsOnUpdate);
        
        ecs_system_desc_t desc = {};
        desc.entity = system_entity;  // Assign the pre-created entity
        desc.callback = PythonSystemCallback;
        desc.ctx = reinterpret_cast<void*>(callback_index);
        
        // Set up query terms
        for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
            desc.query.terms[i] = {
                .id = component_ids[i],
                .inout = EcsInOut,
                .oper = EcsAnd
            };
        }
        
        // Initialize the system with the pre-configured entity
        ecs_entity_t result = ecs_system_init(world, &desc);
        if (result == 0) {
            py::print("ERROR: Failed to create system!");
            return;
        }
        
        py::print("System created with ID:", result);
        
        // Verify the system setup
        if (ecs_has_pair(world, result, EcsDependsOn, EcsOnUpdate)) {
            py::print("✓ System properly depends on OnUpdate");
        }
        if (ecs_has_id(world, result, EcsOnUpdate)) {
            py::print("✓ System is in OnUpdate phase");
        }
    }

    void create_observer_iter(py::function callback, py::args component_types, py::list events = py::list()) {
        size_t callback_index = observer_iter_callbacks.size();
        observer_iter_callbacks.push_back(callback);
        
        std::vector<ecs_entity_t> component_ids;
        for (auto arg : component_types) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            ecs_entity_t component_id = world.entity(component_name.c_str()).id();
            component_ids.push_back(component_id);
        }
        
        // Parse events list - default to OnAdd if empty
        std::vector<ecs_entity_t> event_list;
        if (events.size() == 0) {
            event_list.push_back(EcsOnAdd);  // Default event
        } else {
            for (auto event : events) {
                ecs_entity_t event_id = event.cast<ecs_entity_t>();
                event_list.push_back(event_id);
            }
        }
        
        // Create observer for each event
        for (ecs_entity_t event : event_list) {
            ecs_observer_desc_t desc = {};
            desc.callback = PythonObserverIterCallback;
            desc.ctx = reinterpret_cast<void*>(callback_index);
            desc.events[0] = event;
            
            for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
                desc.query.terms[i] = {
                    .id = component_ids[i],
                    .inout = EcsInOut
                };
            }
            
            ecs_observer_init(world, &desc);
        }
    }
    
    // Create iterator-based system
    void create_system_iter(py::function callback, py::args component_types) {
        size_t callback_index = system_iter_callbacks.size();
        system_iter_callbacks.push_back(callback);
        
        std::vector<ecs_entity_t> component_ids;
        for (auto arg : component_types) {
            py::object comp_type = arg.cast<py::object>();
            std::string component_name = py::str(comp_type.attr("__name__"));
            ecs_entity_t component_id = world.entity(component_name.c_str()).id();
            component_ids.push_back(component_id);
        }
        
        ecs_entity_t system_entity = ecs_new(world);
        ecs_add_pair(world, system_entity, EcsDependsOn, EcsOnUpdate);
        ecs_add_id(world, system_entity, EcsOnUpdate);
        
        ecs_system_desc_t desc = {};
        desc.entity = system_entity;
        desc.callback = PythonSystemIterCallback;
        desc.ctx = reinterpret_cast<void*>(callback_index);
        
        for (size_t i = 0; i < component_ids.size() && i < 32; ++i) {
            desc.query.terms[i] = {
                .id = component_ids[i],
                .inout = EcsInOut,
                .oper = EcsAnd
            };
        }
        
        ecs_system_init(world, &desc);
    }
        
    // Convenience method for decorator support
    py::function observer_decorator(py::args component_types, py::list events = py::list()) {
        return py::cpp_function([this, component_types, events](py::function callback) {
            this->create_observer(callback, component_types, events);
            return callback;
        });
    }

    py::function system_decorator(py::args component_types) {
        return py::cpp_function([this, component_types](py::function callback) {
            this->create_system(callback, component_types);
            return callback;
        });
    }

    py::function observer_iter_decorator(py::args component_types, py::list events = py::list()) {
        return py::cpp_function([this, component_types, events](py::function callback) {
            this->create_observer_iter(callback, component_types, events);
            return callback;
        });
    }
    
    py::function system_iter_decorator(py::args component_types) {
        return py::cpp_function([this, component_types](py::function callback) {
            this->create_system_iter(callback, component_types);
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
    
    py::class_<PyEntity>(m, "Entity")
        .def("id", &PyEntity::id)
        .def("name", &PyEntity::name)
        .def("path", &PyEntity::path)
        .def("children", &PyEntity::children)
        .def("set_name", &PyEntity::set_name)
        .def("is_alive", &PyEntity::is_alive)
        .def("destroy", &PyEntity::destroy)
        .def("has_tag", &PyEntity::has_tag)
        .def("remove_tag", &PyEntity::remove_tag)
        .def("add_tag", &PyEntity::add_tag)
        .def("get_relationship_component", &PyEntity::get_relationship_component)
        // Overloaded add methods for relationships and tags
        .def("add", py::overload_cast<const std::string&>(&PyEntity::add))
        .def("add", py::overload_cast<const std::string&, const std::string&>(&PyEntity::add))
        .def("add", py::overload_cast<const std::string&, PyEntity&>(&PyEntity::add))
        .def("add", py::overload_cast<PyEntity&, PyEntity&>(&PyEntity::add))
        .def("add", py::overload_cast<PyEntity&, const std::string&>(&PyEntity::add))
        .def("add", py::overload_cast<const std::string&, py::object>(&PyEntity::add))
        .def("add", py::overload_cast<py::object, const std::string&>(&PyEntity::add))
        .def("add", py::overload_cast<py::object, py::object>(&PyEntity::add))

        .def("child_of", (&PyEntity::child_of))
        // Overloaded has methods
        .def("has", py::overload_cast<const std::string&>(&PyEntity::has))
        .def("has", py::overload_cast<const std::string&, const std::string&>(&PyEntity::has))
        .def("has", py::overload_cast<const std::string&, PyEntity&>(&PyEntity::has))
        .def("has", py::overload_cast<PyEntity&, PyEntity&>(&PyEntity::has))
        .def("has", py::overload_cast<PyEntity&, const std::string&>(&PyEntity::has))
        .def("has", py::overload_cast<const std::string&, py::object>(&PyEntity::has))
        .def("has", py::overload_cast<py::object, const std::string&>(&PyEntity::has))
        .def("has", py::overload_cast<py::object, py::object>(&PyEntity::has))
        // Overloaded remove methods
        .def("remove", py::overload_cast<const std::string&>(&PyEntity::remove))
        .def("remove", py::overload_cast<const std::string&, const std::string&>(&PyEntity::remove))
        .def("remove", py::overload_cast<const std::string&, PyEntity&>(&PyEntity::remove))
        .def("remove", py::overload_cast<PyEntity&, PyEntity&>(&PyEntity::remove))
        .def("remove", py::overload_cast<PyEntity&, const std::string&>(&PyEntity::remove))
        .def("remove", py::overload_cast<py::object>(&PyEntity::remove))
        // Relationship traversal methods
        .def("get_targets", py::overload_cast<const std::string&>(&PyEntity::get_targets))
        .def("get_targets", py::overload_cast<PyEntity&>(&PyEntity::get_targets))
        // Component methods
        .def("set", &PyEntity::set_component_instance)
        .def("get", &PyEntity::get_component)
        .def("__repr__", [](const PyEntity& e) {
            return e.name() + "(" + std::to_string(e.id()) + ")";
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
        .def("observer", &PyWorld::observer_decorator, py::arg("events") = py::list())
        .def("system", &PyWorld::system_decorator)
        .def("observer_iter", &PyWorld::observer_iter_decorator, py::arg("events") = py::list())
        .def("system_iter", &PyWorld::system_iter_decorator)
        .def("__repr__", [](const PyWorld& w) {
            return w.info();
        });

    py::class_<PyIterator>(m, "Iterator")
        .def("event", &PyIterator::event)
        .def("event_name", &PyIterator::event_name)
        .def("event_id", &PyIterator::event_id)
        .def("event_id_name", &PyIterator::event_id_name)
        .def("count", &PyIterator::count)
        .def("entity", &PyIterator::entity)
        .def("delta_time", &PyIterator::delta_time)
        .def("field_count", &PyIterator::field_count)
        .def("is_set", &PyIterator::is_set)
        .def("get_component", &PyIterator::get_component);

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}