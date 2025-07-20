import flecs
from datetime import datetime
from dataclasses import dataclass

if __name__ == "__main__":
    ecs = flecs.World()
    
    grows = ecs.entity("Grows")

    apples = ecs.entity("Apples")
    pears = ecs.entity("Pears")
    
    jonny = ecs.entity("Jonny")
    jonny.add("Eats", apples)
    jonny.add("Eats", pears)
    jonny.add(grows, apples)

    # Check relationships
    print(f"Jonny eats apples? {jonny.has('Eats', apples)}")
    print(f"Jonny grows food? {jonny.has(grows, "*")}")

    for food in jonny.get_targets("Eats"):
        print(f"Jonny eats {food}")