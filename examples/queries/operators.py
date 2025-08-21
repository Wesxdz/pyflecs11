import flecs
from dataclasses import dataclass

# https://www.flecs.dev/flecs/md_docs_2Queries.html#operator-overview
# Using DSL operators only for now
@dataclass
class Position:
    x: float
    y: float

def main():
    ecs = flecs.World()

    player = ecs.entity("Player")
    player.add("Walking")
    player.add("Sleeping")
    player.set(Position(0, 0))

    # for ent in ecs.query("Walking", "!Awake"):
    #     print(ent)

    for ent in ecs.query("Walking", not Position):
        print(ent)

if __name__ == "__main__":
    main()