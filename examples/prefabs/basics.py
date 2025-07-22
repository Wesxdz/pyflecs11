import flecs
from dataclasses import dataclass

@dataclass
class Defense:
    value: float

def main():
    ecs = flecs.World()

    ecs.component(Defense).add(flecs.OnInstantiate, flecs.Inherit)
    SpaceShip = ecs.prefab("SpaceShip").set(Defense(50))
    inst = ecs.entity("my_spaceship").is_a(SpaceShip)
    print(inst.get(Defense))
    SpaceShip.set(Defense(100))
    for ent, d in ecs.query(Defense):
        print(f"{ent.path}: {d.value}")

if __name__ == "__main__":
    main()