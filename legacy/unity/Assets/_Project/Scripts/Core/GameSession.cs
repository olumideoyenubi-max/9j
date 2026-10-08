using System;
using System.Collections.Generic;
using System.Linq;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Missions;
using NaijaHustle.Core.Phone;
using NaijaHustle.Core.Progression;
using NaijaHustle.Core.Save;
using NaijaHustle.Core.Travel;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core
{
    /// <summary>
    /// Composition root for all engine-free game state. The Unity layer owns one of these and
    /// forwards ticks and gameplay events into it; everything here is unit-testable.
    /// </summary>
    public sealed class GameSession
    {
        public const long StartingCash = 5000;
        public const string StarterOutfit = "fit_street_basic";

        public ContentDatabase Content { get; }
        public Random Rng { get; }

        public WorldClock Clock { get; } = new WorldClock();
        public Wallet Wallet { get; } = new Wallet(StartingCash);
        public PlayerProfile Profile { get; } = new PlayerProfile();
        public StoryProgress Story { get; } = new StoryProgress();
        public WantedSystem Wanted { get; }
        public MissionBoard Missions { get; }
        public BusinessPortfolio Businesses { get; }
        public Wardrobe Wardrobe { get; }
        public Garage Garage { get; }
        public HashSet<string> OwnedSafehouses { get; } = new HashSet<string>();
        public Dictionary<string, int> HustleJobs { get; } = new Dictionary<string, int>();
        public HustleShift ActiveShift { get; private set; }
        public WeatherDirector Weather { get; }
        public AmbientEventScheduler Ambient { get; }
        public CheckpointResolver Checkpoints { get; }
        public TravelPlanner Travel { get; }
        public ChatApp Chat { get; } = new ChatApp();
        public TransferApp Transfers { get; }
        public SocialFeed Social { get; }

        public string LastSafehouse { get; set; }

        public event Action<CityId, CityId> CityChanged;

        public GameSession(ContentDatabase content, int seed)
        {
            Content = content ?? throw new ArgumentNullException(nameof(content));
            Rng = new Random(seed);
            var data = content.Data;

            Wanted = new WantedSystem(data.Crimes, content.City(CityId.Lagos)?.Response ?? new ResponseProfile());
            Missions = new MissionBoard(data.Missions, Story, Wallet, Profile, Wanted);
            Businesses = new BusinessPortfolio(data.Businesses);
            Wardrobe = new Wardrobe(data.Outfits, StarterOutfit);
            Garage = new Garage(data.Vehicles, data.VehicleMods);
            Weather = new WeatherDirector(Rng);
            Ambient = new AmbientEventScheduler(Rng);
            Checkpoints = new CheckpointResolver(Rng);
            Travel = new TravelPlanner(data.Routes, data.BusEvents, Rng);
            Transfers = new TransferApp(Rng);
            Social = new SocialFeed(data.Social, Rng);

            var lagos = content.City(CityId.Lagos);
            if (lagos?.StartSafehouse != null)
            {
                OwnedSafehouses.Add(lagos.StartSafehouse);
                LastSafehouse = lagos.StartSafehouse;
            }

            Clock.HoursElapsed += OnHoursElapsed;
            Missions.MissionPassed += (run, pay) => Social.React("mission_passed:" + run.Definition.Id, Clock.TotalMinutes, Profile.Name);
            Social.Posted += (post, clout) => { if (clout != 0) Profile.AddClout(clout); };
            Story.CityUnlocked += city => Social.React("city_unlocked:" + city, Clock.TotalMinutes, Profile.Name);
        }

        public CityDefinition CurrentCityDef => Content.City(Story.CurrentCity);

        /// <summary>Per-frame update from the runtime layer.</summary>
        public void Tick(float dt, bool seenByResponders, bool inSafeZone)
        {
            Clock.Tick(dt);
            Wanted.Tick(dt, seenByResponders, inSafeZone);
            Missions.Tick(dt);
        }

        private int _lastWeatherHour = -1;

        private void OnHoursElapsed(double hours)
        {
            Businesses.Accrue(hours);
            int hourStamp = (int)(Clock.TotalMinutes / 60);
            if (hourStamp == _lastWeatherHour) return;
            _lastWeatherHour = hourStamp;

            var city = CurrentCityDef;
            if (city == null) return;
            Ambient.RollHour(Content.Data.AmbientEvents, city.Id, Clock);
            if (hourStamp % 3 == 0) Weather.Roll(city.Weather, Clock.Day);
        }

        /// <summary>Arrive in a new city (after a bus, road trip or flight).</summary>
        public void EnterCity(CityId city)
        {
            if (!Story.IsUnlocked(city)) throw new InvalidOperationException(city + " is locked");
            var from = Story.CurrentCity;
            Story.CurrentCity = city;
            var def = Content.City(city);
            if (def?.Response != null) Wanted.SetProfile(def.Response);
            Wanted.Clear();
            Ambient.Clear();
            if (def?.StartSafehouse != null) OwnedSafehouses.Add(def.StartSafehouse);
            if (def != null) Weather.Roll(def.Weather, Clock.Day);
            CityChanged?.Invoke(from, city);
        }

        /// <summary>Busted: lose a share of cash and respawn at the nearest safehouse.</summary>
        public long Bust()
        {
            long lost = Wallet.LosePercent(Wanted.Profile.BustedCashPenalty, "busted");
            Wanted.Clear();
            Missions.Active?.Fail("Busted");
            Profile.AddCred(Story.CurrentCity, -10);
            return lost;
        }

        public CheckpointOutcome ResolveCheckpoint(CheckpointChoice choice, bool carryingCargo)
        {
            var rules = CurrentCityDef?.Checkpoints ?? new CheckpointRules { City = Story.CurrentCity };
            var outcome = Checkpoints.Resolve(choice, rules, carryingCargo, Wallet, Profile, Wardrobe.Respect);
            if (outcome.HeatAdded > 0) Wanted.AddHeat(outcome.HeatAdded);
            if (outcome.DelayMinutes > 0) Clock.Advance(outcome.DelayMinutes);
            Social.React("checkpoint:" + outcome.Kind, Clock.TotalMinutes, Profile.Name);
            return outcome;
        }

        public bool TryBuySafehouse(string id)
        {
            var def = Content.Data.Safehouses.FirstOrDefault(s => s.Id == id);
            if (def == null || OwnedSafehouses.Contains(id)) return false;
            if (!Story.IsUnlocked(def.City)) return false;
            if (!Wallet.TrySpend(def.Price, "safehouse:" + id)) return false;
            OwnedSafehouses.Add(id);
            return true;
        }

        public bool IsHustleUnlocked(HustleDefinition h) =>
            Story.IsUnlocked(h.City) && (string.IsNullOrEmpty(h.UnlockedBy) || Story.IsComplete(h.UnlockedBy));

        public HustleShift StartShift(string hustleId)
        {
            if (ActiveShift != null || Missions.Active != null) return null;
            var def = Content.Data.Hustles.FirstOrDefault(h => h.Id == hustleId);
            if (def == null || def.City != Story.CurrentCity || !IsHustleUnlocked(def)) return null;
            HustleJobs.TryGetValue(hustleId, out int jobs);
            ActiveShift = new HustleShift(def, jobs);
            return ActiveShift;
        }

        public long CompleteShiftJob(float quality)
        {
            if (ActiveShift == null) return 0;
            long pay = ActiveShift.CompleteJob(quality);
            Wallet.Earn(pay, "hustle:" + ActiveShift.Definition.Id);
            HustleJobs.TryGetValue(ActiveShift.Definition.Id, out int jobs);
            HustleJobs[ActiveShift.Definition.Id] = jobs + 1;
            return pay;
        }

        public HustleShift EndShift()
        {
            var shift = ActiveShift;
            ActiveShift = null;
            return shift;
        }

        // ---------- Save / Load ----------

        public SaveData CreateSave(float[] position)
        {
            return new SaveData
            {
                SavedAtUtc = DateTime.UtcNow.ToString("o"),
                Cash = Wallet.Balance,
                ClockMinutes = Clock.TotalMinutes,
                CurrentCity = Story.CurrentCity,
                Position = position ?? new float[3],
                LastSafehouse = LastSafehouse,
                CompletedMissions = Story.CompletedMissions.ToList(),
                Flags = Story.Flags.ToList(),
                UnlockedCities = Story.UnlockedCities.ToList(),
                Cred = Profile.SnapshotCred(),
                Integrity = Profile.Integrity,
                Clout = Profile.Clout,
                OwnedOutfits = Wardrobe.OwnedIds.ToList(),
                EquippedOutfit = Wardrobe.EquippedId,
                OwnedSafehouses = OwnedSafehouses.ToList(),
                Vehicles = Garage.Owned.ToList(),
                Businesses = Businesses.Owned.Select(b => new OwnedBusiness { BusinessId = b.BusinessId, Uncollected = b.Uncollected }).ToList(),
                HustleJobs = new Dictionary<string, int>(HustleJobs),
                Chats = Chat.All.ToList(),
            };
        }

        public void LoadSave(SaveData save)
        {
            save = SaveData.Migrate(save) ?? throw new ArgumentNullException(nameof(save));
            Wallet.Restore(save.Cash);
            Clock.Restore(save.ClockMinutes);
            Story.Restore(save.CompletedMissions, save.Flags, save.UnlockedCities, save.CurrentCity);
            Profile.Restore(save.Cred ?? new Dictionary<CityId, int>(), save.Integrity, save.Clout);
            Wardrobe.Restore(save.OwnedOutfits, save.EquippedOutfit);
            OwnedSafehouses.Clear();
            OwnedSafehouses.UnionWith(save.OwnedSafehouses);
            LastSafehouse = save.LastSafehouse;
            Garage.Restore(save.Vehicles);
            Businesses.Restore(save.Businesses);
            HustleJobs.Clear();
            foreach (var kv in save.HustleJobs) HustleJobs[kv.Key] = kv.Value;
            Chat.Restore(save.Chats);

            var def = CurrentCityDef;
            if (def?.Response != null) Wanted.SetProfile(def.Response);
            Wanted.Clear();
        }
    }
}
