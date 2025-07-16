#!/usr/bin/env python3
"""
Simple test of Flecs Python bindings
"""

import flecs
import numpy as np

class Position:
    x: int
    y: int
    matrix: np.array
    test: list

    def __init__(self, x: int = 0, y: int = 0):
        self.x = x
        self.y = y
        self.matrix = np.zeros(10)
        self.test = [1, "one", 1.0]

    def __str__(self):
        return f"Position({self.x}, {self.y}, {self.matrix}, {self.test})"
    
class Velocity:
    x: int
    y: int

    def __init__(self, x: int = 0, y: int = 0):
        self.x = x
        self.y = y

    def __str__(self):
        return f"Velocity({self.x}, {self.y})"


def test_basic_world():
    
    # Create a world
    world = flecs.World()
    print(f"Created world: {world}")
    
    # Create some entities
    entity1 = world.entity()
    entity2 = world.entity("MyEntity")
    entity3 = world.entity("AnotherEntity")

    entity1.add_tag("wow")
    entity2.add_tag("wow")
    entity2.add_tag("dreamlike")
    print(entity1.has_tag("wow"))
    print(entity2.has_tag("wow"))
    
    print("Add position")
    pos = Position(1, 1)
    entity1.add(pos)
    
    print("Get position")
    print(type(pos).__name__)
    print(entity1.get(Position))

    ents = world.find_with_tags(["wow", "dreamlike"])
    print(ents)

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