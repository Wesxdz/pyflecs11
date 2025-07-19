import flecs
from dataclasses import dataclass

@dataclass
class Position:
    x: int
    y: int

def iterate_tree(e: flecs.Entity, p_parent = Position(0, 0)):
    p = e.get(Position)
    p_actual = Position(p_parent.x + p.x, p_parent.y + p.y)
    print(e.path())
    print(f"{p_actual}\n")
    for child in e.children():
        iterate_tree(child, p_actual)


def main():
    ecs = flecs.World()

    sun = ecs.entity("Sun", ["Star", Position(1, 1)])
    venus = ecs.entity("Venus", ["Planet", Position(2, 2)]).child_of(sun)
    earth = ecs.entity("Earth", ["Planet", Position(3, 3)]).child_of(sun)
    luna = ecs.entity("Luna", ["Moon", Position(0.1, 0.1)]).child_of(earth)
    
    print(f"Child of Earth? {luna.has("ChildOf", earth)}\n")
    print(f"Moon found: {ecs.lookup("Sun::Earth::Luna").path()}\n")

    iterate_tree(sun)


if __name__ == "__main__":
    main()