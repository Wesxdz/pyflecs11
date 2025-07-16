#!/usr/bin/env python3
"""
Simple test of Flecs Python bindings
"""

import flecs as flecs

def test_basic_world():
    print("=== Testing Basic World Operations ===")
    
    # Create a world
    world = flecs.World()
    print(f"Created world: {world}")
    
    # Create some entities
    entity1 = world.entity()
    entity2 = world.entity("MyEntity")
    entity3 = world.entity("AnotherEntity")

    entity1.add_tag("wow")
    print(entity1.has_tag("wow"))
    print(entity2.has_tag("wow"))
    
    print(f"Entity 1 (unnamed): {entity1}")
    print(f"Entity 2 (named): {entity2}")
    print(f"Entity 3 (named): {entity3}")
    
    # Test entity properties
    print(f"\nEntity IDs:")
    print(f"Entity 1 ID: {entity1.id()}")
    print(f"Entity 2 ID: {entity2.id()}")
    print(f"Entity 3 ID: {entity3.id()}")
    
    print(f"\nEntity names:")
    print(f"Entity 1 name: '{entity1.name()}'")
    print(f"Entity 2 name: '{entity2.name()}'")
    print(f"Entity 3 name: '{entity3.name()}'")
    
    # Test alive status
    print(f"\nAlive status:")
    print(f"Entity 1 alive: {entity1.is_alive()}")
    print(f"Entity 2 alive: {entity2.is_alive()}")
    print(f"Entity 3 alive: {entity3.is_alive()}")
    
    # Test entity lookup
    print(f"\nTesting entity lookup:")
    found_entity = world.lookup("MyEntity")
    print(f"Found entity by name 'MyEntity': {found_entity}")
    print(f"Found entity ID matches: {found_entity.id() == entity2.id()}")
    
    # Test setting names
    print(f"\nTesting name setting:")
    entity1.set_name("RenamedEntity")
    print(f"Entity 1 after renaming: {entity1}")
    
    # Test lookup of renamed entity
    renamed_found = world.lookup("RenamedEntity")
    print(f"Found renamed entity: {renamed_found}")
    
    # Test world progress (even without systems)
    print(f"\nTesting world progress:")
    result = world.progress(0.1)
    print(f"World progress result: {result}")
    
    # Test entity destruction
    print(f"\nTesting entity destruction:")
    entity3.destroy()
    print(f"Entity 3 alive after destruction: {entity3.is_alive()}")
    
    print("\nBasic world test completed successfully!")


if __name__ == "__main__":
    print("=== Simple Flecs Python Bindings Test ===")
    test_basic_world()
    print("\n=== All tests completed! ===")