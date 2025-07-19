import flecs
from dataclasses import dataclass
import numpy as np

@dataclass
class Position:
    x: int
    y: int

@dataclass
class Sleep:
    mat: np.array

def main():
    ecs = flecs.World()

    lewis = ecs.entity("Lewis", [Position(10, 20), "Writing"])
    lewis.set(Position(20, 30))
    print(lewis.get(Position))
    alice = ecs.entity("Alice", [Position(50, 25), Sleep(np.zeros([5, 5])), "Walking"])
    lewis.remove("Writing")

    for ent, sleep in ecs.query(Sleep, "Walking"):
        print(f"{ent.name()}: {sleep}")

if __name__ == "__main__":
    main()