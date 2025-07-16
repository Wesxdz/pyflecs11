#!/usr/bin/env python3
"""
Simple test of Flecs Python bindings
"""

import flecs
import numpy as np
from dataclasses import dataclass

@dataclass
class Position:
    x: int
    y: int

    def __repr__(self):
        return f"{{{self.x}, {self.y}}}"

def main():
    ecs = flecs.World()
    bob = ecs.entity("Bob").add("Walking").set(Position(10, 20))
    pos = bob.get(Position)
    print(pos)
    bob.set(Position(20, 30))
    alice = ecs.entity("Alice").set(Position(10, 20)).add("Walking")
    # print(alice.components)
    for ent, pos in ecs.query(Position):
        print(f"{ent.name()}: {pos}")

if __name__ == "__main__":
    main()