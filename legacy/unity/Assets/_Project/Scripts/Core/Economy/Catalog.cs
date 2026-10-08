using System;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Economy
{
    public enum OutfitStyle
    {
        Streetwear,
        Ankara,
        Kaftan,
        Agbada,
        Suit,
        Workwear,
    }

    [Serializable]
    public sealed class OutfitDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public OutfitStyle Style { get; set; }
        public long Price { get; set; }
        /// <summary>Where it's sold. Null = every city's tailor/boutique.</summary>
        public CityId? SoldIn { get; set; }
        /// <summary>Added to "Talk" checks at checkpoints and gala entry. Agbada at an Abuja gala opens doors.</summary>
        public int Respect { get; set; }
        /// <summary>Addressable key for the outfit mesh/material set.</summary>
        public string AssetKey { get; set; }
    }

    public enum VehicleClass
    {
        Okada,
        Keke,
        Danfo,
        Sedan,
        Suv,
        Pickup,
        Truck,
        Tanker,
        Boat,
        Helicopter,
        Jet,
        Crane,
        Forklift,
        Ship,
    }

    [Serializable]
    public sealed class VehicleDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public VehicleClass Class { get; set; }
        public long Price { get; set; }
        public float TopSpeedKph { get; set; }
        public float Acceleration { get; set; } = 1f;
        public float Handling { get; set; } = 1f;
        public int Seats { get; set; } = 2;
        public float Durability { get; set; } = 100f;
        /// <summary>Cities whose traffic spawns this. Empty = not in traffic (purchase / mission only).</summary>
        public CityId[] TrafficCities { get; set; } = Array.Empty<CityId>();
        /// <summary>Relative spawn weight in traffic.</summary>
        public float TrafficWeight { get; set; } = 1f;
        public bool Customizable { get; set; } = true;
        public string AssetKey { get; set; }
    }

    public enum ModSlot
    {
        Paint,
        Rims,
        Engine,
        Brakes,
        Armor,
        Horn,
        Sticker,
        Tint,
    }

    [Serializable]
    public sealed class VehicleModDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public ModSlot Slot { get; set; }
        public long Price { get; set; }
        public float SpeedBonus { get; set; }
        public float HandlingBonus { get; set; }
        public float DurabilityBonus { get; set; }
        /// <summary>Restrict to classes, e.g. danfo stickers. Empty = any customizable vehicle.</summary>
        public VehicleClass[] AllowedClasses { get; set; } = Array.Empty<VehicleClass>();
    }

    [Serializable]
    public sealed class SafehouseDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public CityId City { get; set; }
        public string DistrictId { get; set; }
        public long Price { get; set; }
        public int GarageSlots { get; set; } = 1;
        public bool HasBoatDock { get; set; }
        public bool HasHelipad { get; set; }
    }
}
