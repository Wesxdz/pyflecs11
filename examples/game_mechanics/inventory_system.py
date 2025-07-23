import flecs
import numpy as np
from dataclasses import dataclass

@dataclass
class Health:
    amount: int

@dataclass
class ItemDrop:
    item: flecs.Entity

@dataclass
class Lootbox:
    possible: list[ItemDrop]
    
    def loot_spawn(self):
        items = []
        for drop in self.possible:
            print(drop)
        return items

def main():
    ecs = flecs.World()

    material_class = ["Wooden", "Iron"]
    for i, material in enumerate(material_class):
        ecs.prefab(material+"Sword", ["Sword", "Item"]).set_over(Health((i+1)*5))
        ecs.prefab(material+"Armor", ["Armor", "Item"]).set_over(Health((i+1)*10))
    
    loot_box = [ecs.entity().is_a("IronSword")]


if __name__ == "__main__":
    main()