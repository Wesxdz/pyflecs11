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

    @ecs.observer(Position)
    def pos_added(ent, pos):
        print(pos)

    @ecs.system(Position, Velocity)
    def move(ent, pos, vel):
        pos.x = pos.x + vel.x
        pos.y = pos.y + vel.y

    @ecs.system(Position, Velocity)
    def move(ent, pos, vel):
        pos.x = pos.x + 0.5

    sander = ecs.entity("Sander", [Position(0, 0), Velocity(1, 2)])
    sander.add("Programs", "Flecs")

    for i in range(100):
        ecs.progress()
    
    print(sander.get(Position))

if __name__ == "__main__":
    main()