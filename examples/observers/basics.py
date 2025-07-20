import flecs
from dataclasses import dataclass

@dataclass
class Position:
    x: float
    y: float

def main():
    ecs = flecs.World()

    @ecs.observer_iter(Position, events=[flecs.OnAdd, flecs.OnRemove])
    def demo_observer(it, pos):
        if it.event() == flecs.OnAdd:
            print(f"Added {pos[0]}")
        elif it.event() == flecs.OnRemove:
            print(f"Removed {pos[0]}")

    e = ecs.entity("e").set(Position(10, 20))
    e.remove(Position)

if __name__ == "__main__":
    main()