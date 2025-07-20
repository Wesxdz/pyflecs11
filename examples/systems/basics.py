import flecs
from dataclasses import dataclass

@dataclass
class Position:
    x: float
    y: float

@dataclass
class Velocity:
    x: float
    y: float

def main():
    ecs = flecs.World()

    def move(e, pos, vel):
        pos.x = pos.x + vel.x
        pos.y = pos.y + vel.y
        print(f"{e.name()} {pos}")

    system = ecs.system(move, Position, Velocity)

    ecs.entity("e1", [Position(10, 20), Velocity(1, 2)])
    ecs.entity("e2", [Position(10, 20), Velocity(3, 4)])
    ecs.entity("e3", [Position(10, 20)])

    system.run()

if __name__ == "__main__":
    main()