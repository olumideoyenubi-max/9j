"""Checks how assign_vehicle_meshes.py reads model names and fits a model to a vehicle type.
    python3 unreal/NaijaHustle/Scripts/tests/test_assign_vehicle_meshes.py"""
import importlib.util
import json
import os
import sys

HERE = os.path.dirname(__file__)
spec = importlib.util.spec_from_file_location("assign_vehicle_meshes", os.path.join(HERE, "..", "assign_vehicle_meshes.py"))
av = importlib.util.module_from_spec(spec)
spec.loader.exec_module(av)

fails = 0
def ok(name, cond, info=None):
    global fails
    print(("PASS " if cond else "FAIL ") + name + ("" if cond else f"  {info}"))
    fails += 0 if cond else 1

more = ["SM_Hypercar_Blue", "Super_SUV_01", "Coupe_SUV", "Royal_SUV_Black", "Grand_Coupe"]
ok("the other luxury classes from model names", [av.vehicle_type(n) for n in more] == ["hypercar", "supersuv", "coupesuv", "royalsuv", "luxcoupe"], [av.vehicle_type(n) for n in more])
names = ["SM_SportsCar_01", "Luxury_SUV_Black", "SM_Limousine", "Old_Sedan", "SM_Jeep_4x4", "Mini_Bus_Yellow", "Tuk_Tuk", "SM_Motorcycle", "Dump_Truck", "Pickup_Truck", "SM_Carpet", "Rock_Cliff"]
got = [av.vehicle_type(n) for n in names]
ok("vehicle types from model names", got == ["sports", "luxsuv", "luxsedan", "sedan", "suv", "danfo", "keke", "okada", "truck", "tfpick", None, None], got)
ok("wheels, doors and other parts are not taken for the car itself", av.is_part("SM_SportsCar_Wheel_FL") and av.is_part("Sedan_Door_L") and not av.is_part("SM_SportsCar_Body"))

# a 4 m model built along Y, off-centre and floating: fitted to a 455 cm sports car
box = ((-90.0, 100.0, 20.0), (90.0, 500.0, 140.0))
yaw, scale, offset, height = av.fit([box], 455)
ok("a model built along Y is turned to face along X and scaled to the type's length", yaw == 90.0 and abs(scale - 455 / 400) < 1e-9, (yaw, scale))
ok("it is centred under the vehicle and its lowest point sits on the ground", abs(offset[0] - 300 * scale) < 1e-6 and abs(offset[1]) < 1e-6 and abs(offset[2] + 20 * scale) < 1e-6 and abs(height - 120 * scale) < 1e-6, (offset, height))

lengths = {}
data = os.path.join(HERE, "..", "..", "Data")
for name, key in (("naija_rules.json", "vehicles"), ("unreal_vehicles.json", "types")):
    for kind, s in json.load(open(os.path.join(data, name), encoding="utf-8"))[key].items():
        lengths[kind] = s["len"]
LUX = {"luxsedan", "luxsuv", "sports", "royalsuv", "coupesuv", "supersuv", "luxcoupe", "hypercar"}
ok("the luxury types are in the game's data", LUX <= set(lengths), sorted(LUX - set(lengths)))
extra = json.load(open(os.path.join(data, "unreal_vehicles.json"), encoding="utf-8"))
city = json.load(open(os.path.join(data, "lagos_city.json"), encoding="utf-8"))
ok("every parked luxury car is a known type on a road or in the motor park", all(p["type"] in extra["types"] and city["tiles"][p["y"] // 400][p["x"] // 400] in "RP" for p in extra["parked"])
   and {p["type"] for p in extra["parked"]} == LUX and all(t["body"] in ("sedan", "suv", "sports") for t in extra["types"].values()), [(p["type"], city["tiles"][p["y"] // 400][p["x"] // 400]) for p in extra["parked"]])

car = ((-230.0, -100.0, 0.0), (230.0, 100.0, 130.0))
wheel = ((120.0, 80.0, 0.0), (190.0, 105.0, 70.0))
meshes = [("/Game/Fab/SportsCar", "SM_SportsCar_Body", "/Game/Fab/SportsCar/SM_SportsCar_Body.SM_SportsCar_Body", car),
          ("/Game/Fab/SportsCar", "SM_SportsCar_Wheel", "/Game/Fab/SportsCar/SM_SportsCar_Wheel.SM_SportsCar_Wheel", wheel),
          ("/Game/Fab/Luxury_SUV", "SM_Body", "/Game/Fab/Luxury_SUV/SM_Body.SM_Body", car),
          ("/Game/Fab/Rocks", "SM_Rock_01", "/Game/Fab/Rocks/SM_Rock_01.SM_Rock_01", car),
          ("/Game/Fab/Wheels", "SM_Car_Wheel", "/Game/Fab/Wheels/SM_Car_Wheel.SM_Car_Wheel", wheel)]
p = av.plan(meshes, lengths)
ok("each car folder becomes one model with all its meshes; rocks and loose wheels are left out", sorted(p) == ["luxsuv", "sports"] and len(p["sports"]["meshes"]) == 2 and len(p["luxsuv"]["meshes"]) == 1, {k: v["meshes"] for k, v in p.items()})
ok("the model is scaled to its type's length", abs(p["sports"]["scale"] - lengths["sports"] / 460) < 1e-4 and abs(p["luxsuv"]["scale"] - lengths["luxsuv"] / 460) < 1e-4)
flipped = av.plan(meshes, lengths, flip={"sports"})
ok("a flipped type is turned round", flipped["sports"]["yaw"] == p["sports"]["yaw"] + 180 and flipped["luxsuv"]["yaw"] == p["luxsuv"]["yaw"])

print("ALL PASS" if not fails else f"{fails} FAILED")
sys.exit(1 if fails else 0)
