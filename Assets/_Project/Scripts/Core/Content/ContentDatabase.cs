using System;
using System.Collections.Generic;
using System.Linq;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Missions;
using NaijaHustle.Core.Phone;
using NaijaHustle.Core.Travel;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Content
{
    [Serializable]
    public sealed class DistrictDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        /// <summary>Design note on what real-world area inspired it (never shown in game).</summary>
        public string InspiredBy { get; set; }
        public string Vibe { get; set; }
        /// <summary>Ambient pedestrian/traffic density 0..1 — Balo Market is 1.0, Maiden Hills 0.2.</summary>
        public float Density { get; set; } = 0.5f;
        public bool IsWater { get; set; }
    }

    [Serializable]
    public sealed class RadioStationDefinition
    {
        public string Id { get; set; }
        public string Name { get; set; }
        public string Frequency { get; set; }
        public string Genre { get; set; }
        public string Host { get; set; }
        /// <summary>Addressables label of the streamed audio clips (downloaded with the city).</summary>
        public string PlaylistKey { get; set; }
        public string[] Bumpers { get; set; } = Array.Empty<string>();
    }

    [Serializable]
    public sealed class CityDefinition
    {
        public CityId Id { get; set; }
        public string Tagline { get; set; }
        /// <summary>Addressables key of the city's root scene.</summary>
        public string SceneKey { get; set; }
        /// <summary>Addressables label for the remote content bundle (download on unlock).</summary>
        public string DownloadLabel { get; set; }
        public int ApproxDownloadMb { get; set; }
        public string StartSafehouse { get; set; }
        public DistrictDefinition[] Districts { get; set; } = Array.Empty<DistrictDefinition>();
        public List<WeatherWeights> Weather { get; set; } = new List<WeatherWeights>();
        public RadioStationDefinition[] Radio { get; set; } = Array.Empty<RadioStationDefinition>();
        public ResponseProfile Response { get; set; }
        public CheckpointRules Checkpoints { get; set; }
    }

    /// <summary>One JSON file's worth of content. Any field may be omitted; bundles are merged.</summary>
    [Serializable]
    public sealed class ContentBundle
    {
        public List<CityDefinition> Cities { get; set; } = new List<CityDefinition>();
        public List<MissionDefinition> Missions { get; set; } = new List<MissionDefinition>();
        public List<HustleDefinition> Hustles { get; set; } = new List<HustleDefinition>();
        public List<VehicleDefinition> Vehicles { get; set; } = new List<VehicleDefinition>();
        public List<VehicleModDefinition> VehicleMods { get; set; } = new List<VehicleModDefinition>();
        public List<OutfitDefinition> Outfits { get; set; } = new List<OutfitDefinition>();
        public List<BusinessDefinition> Businesses { get; set; } = new List<BusinessDefinition>();
        public List<SafehouseDefinition> Safehouses { get; set; } = new List<SafehouseDefinition>();
        public List<ContactDefinition> Contacts { get; set; } = new List<ContactDefinition>();
        public List<CrimeWeight> Crimes { get; set; } = new List<CrimeWeight>();
        public List<RouteDefinition> Routes { get; set; } = new List<RouteDefinition>();
        public List<BusMiniEvent> BusEvents { get; set; } = new List<BusMiniEvent>();
        public List<SocialTemplate> Social { get; set; } = new List<SocialTemplate>();
        public List<AmbientEventDefinition> AmbientEvents { get; set; } = new List<AmbientEventDefinition>();
    }

    /// <summary>All static game content, merged from JSON bundles, plus validation.</summary>
    public sealed class ContentDatabase
    {
        public const int MinStoryMissionsPerCity = 8;
        public const int MaxStoryMissionsPerCity = 10;

        public ContentBundle Data { get; } = new ContentBundle();

        public void Merge(ContentBundle b)
        {
            if (b == null) return;
            Data.Cities.AddRange(b.Cities ?? new List<CityDefinition>());
            Data.Missions.AddRange(b.Missions ?? new List<MissionDefinition>());
            Data.Hustles.AddRange(b.Hustles ?? new List<HustleDefinition>());
            Data.Vehicles.AddRange(b.Vehicles ?? new List<VehicleDefinition>());
            Data.VehicleMods.AddRange(b.VehicleMods ?? new List<VehicleModDefinition>());
            Data.Outfits.AddRange(b.Outfits ?? new List<OutfitDefinition>());
            Data.Businesses.AddRange(b.Businesses ?? new List<BusinessDefinition>());
            Data.Safehouses.AddRange(b.Safehouses ?? new List<SafehouseDefinition>());
            Data.Contacts.AddRange(b.Contacts ?? new List<ContactDefinition>());
            Data.Crimes.AddRange(b.Crimes ?? new List<CrimeWeight>());
            Data.Routes.AddRange(b.Routes ?? new List<RouteDefinition>());
            Data.BusEvents.AddRange(b.BusEvents ?? new List<BusMiniEvent>());
            Data.Social.AddRange(b.Social ?? new List<SocialTemplate>());
            Data.AmbientEvents.AddRange(b.AmbientEvents ?? new List<AmbientEventDefinition>());
        }

        public CityDefinition City(CityId id) => Data.Cities.FirstOrDefault(c => c.Id == id);

        /// <summary>Content sanity checks. Run in an EditMode test and on boot in development builds.</summary>
        public List<string> Validate()
        {
            var errors = new List<string>();

            void Unique<T>(IEnumerable<T> items, Func<T, string> id, string kind)
            {
                foreach (var g in items.GroupBy(id))
                {
                    if (string.IsNullOrWhiteSpace(g.Key)) errors.Add($"{kind} with empty id");
                    else if (g.Count() > 1) errors.Add($"Duplicate {kind} id '{g.Key}'");
                }
            }

            Unique(Data.Missions, m => m.Id, "mission");
            Unique(Data.Hustles, h => h.Id, "hustle");
            Unique(Data.Vehicles, v => v.Id, "vehicle");
            Unique(Data.VehicleMods, m => m.Id, "vehicle mod");
            Unique(Data.Outfits, o => o.Id, "outfit");
            Unique(Data.Businesses, b => b.Id, "business");
            Unique(Data.Safehouses, s => s.Id, "safehouse");
            Unique(Data.Contacts, c => c.Id, "contact");

            var missionIds = new HashSet<string>(Data.Missions.Select(m => m.Id));
            var contactIds = new HashSet<string>(Data.Contacts.Select(c => c.Id));
            var safehouseIds = new HashSet<string>(Data.Safehouses.Select(s => s.Id));
            var flagsSet = new HashSet<string>(Data.Missions.SelectMany(m => m.SetsFlags));

            foreach (CityId city in Enum.GetValues(typeof(CityId)))
            {
                var def = City(city);
                if (def == null) { errors.Add($"Missing city definition for {city}"); continue; }
                if (def.Response == null) errors.Add($"{city}: missing wanted response profile");
                else if (def.Response.UnitsByStar == null || def.Response.UnitsByStar.Length != 5) errors.Add($"{city}: response needs 5 unit tiers");
                if (def.Checkpoints == null) errors.Add($"{city}: missing checkpoint rules");
                if (def.Radio.Length == 0) errors.Add($"{city}: needs at least one radio station");
                if (def.Districts.Length < 4) errors.Add($"{city}: needs at least 4 districts");
                if (!safehouseIds.Contains(def.StartSafehouse ?? string.Empty)) errors.Add($"{city}: start safehouse '{def.StartSafehouse}' not found");

                int story = Data.Missions.Count(m => m.City == city && m.IsStory);
                if (story < MinStoryMissionsPerCity || story > MaxStoryMissionsPerCity)
                    errors.Add($"{city}: has {story} story missions, expected {MinStoryMissionsPerCity}-{MaxStoryMissionsPerCity}");

                if (!Data.Hustles.Any(h => h.City == city)) errors.Add($"{city}: no side hustles");
                if (!Data.Businesses.Any(b => b.City == city)) errors.Add($"{city}: no businesses");
            }

            foreach (var m in Data.Missions)
            {
                if (m.Objectives == null || m.Objectives.Length == 0) errors.Add($"Mission {m.Id}: no objectives");
                if (!string.IsNullOrEmpty(m.Giver) && !contactIds.Contains(m.Giver)) errors.Add($"Mission {m.Id}: unknown giver '{m.Giver}'");
                foreach (var r in m.Requires) if (!missionIds.Contains(r)) errors.Add($"Mission {m.Id}: requires unknown mission '{r}'");
                foreach (var f in m.RequiresFlags) if (!flagsSet.Contains(f)) errors.Add($"Mission {m.Id}: requires flag '{f}' that nothing sets");
            }

            foreach (var b in Data.Businesses)
                if (!string.IsNullOrEmpty(b.RequiresMission) && !missionIds.Contains(b.RequiresMission))
                    errors.Add($"Business {b.Id}: requires unknown mission '{b.RequiresMission}'");

            foreach (var h in Data.Hustles)
                if (!string.IsNullOrEmpty(h.UnlockedBy) && !missionIds.Contains(h.UnlockedBy))
                    errors.Add($"Hustle {h.Id}: unlocked by unknown mission '{h.UnlockedBy}'");

            // Every non-starting city must be unlockable by some mission, and story graph must be acyclic.
            foreach (CityId city in Enum.GetValues(typeof(CityId)))
                if (city != CityId.Lagos && !Data.Missions.Any(m => m.UnlocksCity == city))
                    errors.Add($"Nothing unlocks {city}");

            if (HasCycle()) errors.Add("Mission prerequisites contain a cycle");

            return errors;
        }

        private bool HasCycle()
        {
            var byId = Data.Missions.GroupBy(m => m.Id).ToDictionary(g => g.Key, g => g.First());
            var state = new Dictionary<string, int>(); // 1 visiting, 2 done

            bool Visit(string id)
            {
                if (!byId.ContainsKey(id)) return false;
                if (state.TryGetValue(id, out int s)) return s == 1;
                state[id] = 1;
                foreach (var r in byId[id].Requires) if (Visit(r)) return true;
                state[id] = 2;
                return false;
            }

            return byId.Keys.Any(Visit);
        }
    }
}
