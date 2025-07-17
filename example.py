import flecs
from dataclasses import dataclass

@dataclass
class Position:
    x: int
    y: int

def main():
    ecs = flecs.World()
    bob = ecs.entity("Bob", [Position(10, 20), "Walking"])
    bob.set(Position(20, 30))
    print(bob.get(Position))
    alice = ecs.entity("Alice", [Position(50, 25), "Walking"])
    # print(alice.components)

    for ent, pos in ecs.query(Position):
        print(f"{ent.name()}: {pos}")

if __name__ == "__main__":
    main()