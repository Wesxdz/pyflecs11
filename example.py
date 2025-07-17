import flecs
from dataclasses import dataclass

@dataclass
class Position:
    x: int
    y: int

def main():
    ecs = flecs.World()
    # bob = ecs.entity("Bob", [Position(10, 20), "Walking"])
    bob = ecs.entity("Bob").set(Position(10, 20)).add("Walking")
    bob.set(Position(20, 30))
    print(bob.get(Position))
    alice = ecs.entity("Alice").set(Position(10, 20)).add("Walking")
    # print(alice.components)
    # print("Okay")
    # print(ecs.query(Position))

    # for data in ecs.query(Position):
    #     print("loop")
    #     print(data)

    # for ent, pos in ecs.query(Position):
    #     print(f"{ent.name()}: {pos}")

if __name__ == "__main__":
    main()