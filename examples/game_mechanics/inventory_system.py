import flecs
import numpy as np
from dataclasses import dataclass

@dataclass
class Health:
    amount: int

@dataclass
class Material:
    amount: int

@dataclass
class ItemDrop:
    item: flecs.Entity

@dataclass
class Lootbox:
    probabilities: list[tuple[float, flecs.Entity]]
    
    def print_spawn_probabilities(self):
        if not self.probabilities:
            print("No items in lootbox")
            return
        
        probs, items = zip(*self.probabilities)
        probs = np.array(probs)
        normalized_probs = probs / probs.sum()
        
        print("Normalized spawn probabilities:")
        for prob, item in zip(normalized_probs, items):
            print(f"{item}: {prob*100:.2f}%")
    
    def loot_spawn(self, num_items=1):
        if not self.probabilities:
            return []
        probs, items = zip(*self.probabilities)
        probs = np.array(probs)
        probs = probs / probs.sum()

        selected_indices = np.random.choice(
            len(items), 
            size=num_items, 
            p=probs,
            replace=True  # Set to False if you don't want duplicate items
        )

        selected_items = [items[i] for i in selected_indices]
        return selected_items

def main():
    ecs = flecs.World()

    lootbox = Lootbox([])

    material_class = ["Wooden", "Stone", "Bronze", "Mythril"]
    equipment_types = ["Sword", "Chestplate", "Helm"]
    for i, material in enumerate(material_class):
        spawn_prob = 1/(1+(i*i))
        for equipment in equipment_types:
            item = ecs.prefab(material+equipment, [material, equipment, "Item"]).set(Health((i+1)*5))
            lootbox.probabilities.append((spawn_prob, item))
    
    lootbox.print_spawn_probabilities()
    
    # Test the loot spawning
    print("\nSpawning 5 items:")
    spawned_items = lootbox.loot_spawn(5)
    for i, item in enumerate(spawned_items):
        ecs.entity(f"{i}_" + item.name()).is_a(item)
    
    for item, in ecs.query("Item"):
        print(item)

if __name__ == "__main__":
    main()