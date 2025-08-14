import flecs
from dataclasses import dataclass

def main():
   ecs = flecs.World()
   at = "LocatedIn"
   ecs.component(at).add_trait("Transitive")
   
   earth = ecs.entity("Earth", ["Planet"])
   
   north_america = ecs.entity("North America", ["Continent", (at, earth)])
   europe = ecs.entity("Europe", ["Continent", (at, earth)])
   
   united_states = ecs.entity("United States", ["Country", (at, north_america)])
   netherlands = ecs.entity("Netherlands", ["Country", (at, europe)])
   
   california = ecs.entity("California", ["State", (at, united_states)])
   massachusetts = ecs.entity("Massachusetts", ["State", (at, united_states)])
   noord_holland = ecs.entity("Noord Holland", ["State", (at, netherlands)])
   
   palo_alto = ecs.entity("Palo Alto", ["City", (at, california)])
   cambridge = ecs.entity("Cambridge", ["City", (at, massachusetts)])
   amsterdam = ecs.entity("Amsterdam", ["City", (at, noord_holland)])
   
   ecs.entity("McCarthy", ["Person", (at, palo_alto)])
   ecs.entity("Minsky", ["Person", (at, cambridge)])
   ecs.entity("Dijkstra", ["Person", (at, amsterdam)])

   for person, country in ecs.query(
        "Person",
        (at, "$location"),
        ("$location", "Country")):
       print(f"{person} in {country}")

if __name__ == "__main__":
    main()