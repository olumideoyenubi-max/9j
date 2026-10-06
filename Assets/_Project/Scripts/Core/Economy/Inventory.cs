using System;
using System.Collections.Generic;
using System.Linq;

namespace NaijaHustle.Core.Economy
{
    /// <summary>Owned outfits and the one currently worn.</summary>
    public sealed class Wardrobe
    {
        private readonly Dictionary<string, OutfitDefinition> _defs;
        private readonly HashSet<string> _owned = new HashSet<string>();

        public string EquippedId { get; private set; }
        public OutfitDefinition Equipped => EquippedId != null && _defs.TryGetValue(EquippedId, out var o) ? o : null;
        public IEnumerable<string> OwnedIds => _owned;

        public event Action<OutfitDefinition> OutfitChanged;

        public Wardrobe(IEnumerable<OutfitDefinition> definitions, string starterOutfitId)
        {
            _defs = definitions.ToDictionary(d => d.Id);
            if (starterOutfitId != null && _defs.ContainsKey(starterOutfitId))
            {
                _owned.Add(starterOutfitId);
                EquippedId = starterOutfitId;
            }
        }

        public bool Owns(string id) => _owned.Contains(id);

        public bool TryBuy(string id, Wallet wallet)
        {
            if (!_defs.TryGetValue(id, out var def) || Owns(id)) return false;
            if (!wallet.TrySpend(def.Price, "outfit:" + id)) return false;
            _owned.Add(id);
            return true;
        }

        public bool Equip(string id)
        {
            if (!Owns(id)) return false;
            EquippedId = id;
            OutfitChanged?.Invoke(_defs[id]);
            return true;
        }

        public int Respect => Equipped?.Respect ?? 0;

        internal void Restore(IEnumerable<string> owned, string equipped)
        {
            _owned.Clear();
            foreach (var id in owned) if (_defs.ContainsKey(id)) _owned.Add(id);
            EquippedId = equipped != null && _owned.Contains(equipped) ? equipped : _owned.FirstOrDefault();
        }
    }

    public sealed class OwnedVehicle
    {
        public string InstanceId { get; set; }
        public string VehicleId { get; set; }
        public string SafehouseId { get; set; }
        public Dictionary<ModSlot, string> Mods { get; set; } = new Dictionary<ModSlot, string>();
        public string PaintHex { get; set; } = "#FFFFFF";
    }

    /// <summary>Vehicles stored in safehouse garages plus their customization.</summary>
    public sealed class Garage
    {
        private readonly Dictionary<string, VehicleDefinition> _vehicles;
        private readonly Dictionary<string, VehicleModDefinition> _mods;
        private readonly List<OwnedVehicle> _owned = new List<OwnedVehicle>();
        private int _nextId = 1;

        public IReadOnlyList<OwnedVehicle> Owned => _owned;

        public Garage(IEnumerable<VehicleDefinition> vehicles, IEnumerable<VehicleModDefinition> mods)
        {
            _vehicles = vehicles.ToDictionary(v => v.Id);
            _mods = mods.ToDictionary(m => m.Id);
        }

        public int CountIn(string safehouseId) => _owned.Count(v => v.SafehouseId == safehouseId);

        /// <summary>Store a vehicle (bought or "acquired" off the street) in a safehouse garage.</summary>
        public OwnedVehicle Store(string vehicleId, string safehouseId, int garageSlots)
        {
            if (!_vehicles.ContainsKey(vehicleId)) return null;
            if (CountIn(safehouseId) >= garageSlots) return null;
            var v = new OwnedVehicle { InstanceId = "veh_" + _nextId++, VehicleId = vehicleId, SafehouseId = safehouseId };
            _owned.Add(v);
            return v;
        }

        public bool Remove(string instanceId) => _owned.RemoveAll(v => v.InstanceId == instanceId) > 0;

        public bool TryApplyMod(string instanceId, string modId, Wallet wallet)
        {
            var owned = _owned.Find(v => v.InstanceId == instanceId);
            if (owned == null || !_mods.TryGetValue(modId, out var mod)) return false;
            var vehicle = _vehicles[owned.VehicleId];
            if (!vehicle.Customizable) return false;
            if (mod.AllowedClasses.Length > 0 && Array.IndexOf(mod.AllowedClasses, vehicle.Class) < 0) return false;
            if (owned.Mods.TryGetValue(mod.Slot, out var current) && current == modId) return false;
            if (!wallet.TrySpend(mod.Price, "mod:" + modId)) return false;

            owned.Mods[mod.Slot] = modId;
            return true;
        }

        /// <summary>Base stats plus all installed mod bonuses.</summary>
        public (float topSpeed, float handling, float durability) EffectiveStats(OwnedVehicle owned)
        {
            var v = _vehicles[owned.VehicleId];
            float speed = v.TopSpeedKph, handling = v.Handling, durability = v.Durability;
            foreach (var modId in owned.Mods.Values)
            {
                if (!_mods.TryGetValue(modId, out var m)) continue;
                speed += m.SpeedBonus;
                handling += m.HandlingBonus;
                durability += m.DurabilityBonus;
            }
            return (speed, handling, durability);
        }

        internal void Restore(IEnumerable<OwnedVehicle> owned)
        {
            _owned.Clear();
            int max = 0;
            foreach (var v in owned)
            {
                if (!_vehicles.ContainsKey(v.VehicleId)) continue;
                _owned.Add(v);
                if (v.InstanceId != null && v.InstanceId.StartsWith("veh_") && int.TryParse(v.InstanceId.Substring(4), out int n)) max = Math.Max(max, n);
            }
            _nextId = max + 1;
        }
    }
}
