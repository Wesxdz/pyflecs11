import flecs
from dataclasses import dataclass

def main():
    ecs = flecs.World()

    bambara = ecs.entity("Bambara groundnut", ["Healthy"])
    madagascar = ecs.entity("Madagascar")
    madagascar.add("In", "Africa")
    bambara.add("GrowsIn", "Madagascar")
    hazlenuts = ecs.entity("Hazlenuts", ["Healthy"])
    hazlenuts.add("GrowsIn", "Oregon")
    apples = ecs.entity("Apples", ["Healthy"])

    burgers = ecs.entity("Burgers")
    pizza = ecs.entity("Pizza")
    chocolate = ecs.entity("Chocolate")

    luciano = ecs.entity("Luciano")
    for food in [bambara, burgers, pizza]:
        luciano.add("Eats", food) 
    alice = ecs.entity("Alice")
    for food in [hazlenuts, chocolate, apples]:
        alice.add("Eats", food)

    # for person, food, place in ecs.query(("Eats", "$food"), ("$food", "Healthy"), ("$food", "GrowsIn", "$place")):
    for person, food, place in ecs.query(("Eats", "$food"), ("$food", "Healthy"), ("$food", "GrowsIn", "$place")):
        print(f"{person} eats {food} which grow in {place}")

if __name__ == "__main__":
    main()