"""Makes the street DataTable of a district from its CSV.

Run inside the Unreal Editor (Scripts/mac.sh script <this file>) after Scripts/build_district_osm.py has written
Data/streets_<district>.csv. Environment variable NH_DISTRICT picks the district (default oshodi).

It makes /Game/NaijaHustle/Data/DT_Streets_<District>, a DataTable of FNHStreetRow keyed by "W" + OpenStreetMap way
id. Generated, so not stored in the repo: run it again after the CSV changes. (c) OpenStreetMap contributors, ODbL.
"""
import os
import unreal

district = os.environ.get("NH_DISTRICT", "oshodi")
csv_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Data", f"streets_{district}.csv"))
if not os.path.isfile(csv_path):
    raise RuntimeError(f"{csv_path} not found: run Scripts/build_district_osm.py {district} first")
folder, name = "/Game/NaijaHustle/Data", f"DT_Streets_{district.capitalize()}"
eal = unreal.EditorAssetLibrary
if eal.does_asset_exist(f"{folder}/{name}"):
    table = eal.load_asset(f"{folder}/{name}")
else:
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", unreal.NHStreetRow.static_struct())
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.DataTable, factory)
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_csv_file(table, csv_path)
rows = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
names = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "StreetName")
eal.save_loaded_asset(table, only_if_is_dirty=False)
unreal.log(f"NAIJA HUSTLE: {folder}/{name}: filled {'ok' if ok else 'WITH ERRORS'}, {len(rows)} rows, {len({n for n in names if n})} different street names")
