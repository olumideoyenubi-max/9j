using System;
using System.Collections.Generic;
using System.Linq;
using NaijaHustle.Core.Economy;
using NaijaHustle.Core.Progression;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Core.World;

namespace NaijaHustle.Core.Missions
{
    /// <summary>Decides which missions are available, runs one at a time, and pays out.</summary>
    public sealed class MissionBoard
    {
        /// <summary>Bonus pay fraction lost per failed optional objective.</summary>
        public const float OptionalPenalty = 0.1f;

        private readonly Dictionary<string, MissionDefinition> _missions;
        private readonly StoryProgress _story;
        private readonly Wallet _wallet;
        private readonly PlayerProfile _profile;
        private readonly WantedSystem _wanted;

        public MissionRun Active { get; private set; }

        public event Action<MissionRun> MissionStarted;
        public event Action<MissionRun, long> MissionPassed;
        public event Action<MissionRun> MissionFailed;

        public MissionBoard(IEnumerable<MissionDefinition> missions, StoryProgress story, Wallet wallet, PlayerProfile profile, WantedSystem wanted)
        {
            _missions = missions.ToDictionary(m => m.Id);
            _story = story;
            _wallet = wallet;
            _profile = profile;
            _wanted = wanted;
        }

        public MissionDefinition Get(string id) => _missions.TryGetValue(id, out var m) ? m : null;

        public bool IsAvailable(MissionDefinition m)
        {
            if (_story.IsComplete(m.Id)) return false;
            if (!_story.IsUnlocked(m.City)) return false;
            foreach (var r in m.Requires) if (!_story.IsComplete(r)) return false;
            foreach (var f in m.RequiresFlags) if (!_story.HasFlag(f)) return false;
            return true;
        }

        public IEnumerable<MissionDefinition> Available(CityId city) =>
            _missions.Values.Where(m => m.City == city && IsAvailable(m)).OrderBy(m => m.IsStory ? 0 : 1).ThenBy(m => m.Order);

        /// <summary>Next story mission in a city, for the HUD's "next" blip.</summary>
        public MissionDefinition NextStory(CityId city) => Available(city).FirstOrDefault(m => m.IsStory);

        public bool TryStart(string id)
        {
            if (Active != null) return false;
            var def = Get(id);
            if (def == null || !IsAvailable(def) || def.City != _story.CurrentCity) return false;
            if (def.RequiresNoHeat && _wanted.IsWanted) return false;

            Active = new MissionRun(def);
            Active.Finished += OnFinished;
            MissionStarted?.Invoke(Active);
            Active.Begin();
            return true;
        }

        public void Tick(float dt) => Active?.Tick(dt);

        public bool Report(ObjectiveEvent e) => Active != null && Active.Report(e);

        private void OnFinished(MissionRun run)
        {
            run.Finished -= OnFinished;
            Active = null;

            if (run.State != MissionState.Passed)
            {
                MissionFailed?.Invoke(run);
                return;
            }

            var def = run.Definition;
            long pay = (long)(def.Reward * Math.Max(0f, 1f - OptionalPenalty * run.OptionalFailed));
            _wallet.Earn(pay, "mission:" + def.Id);
            _profile.AddCred(def.City, def.CredReward);
            _profile.AddIntegrity(def.IntegrityDelta);
            _story.MarkComplete(def.Id);
            foreach (var f in def.SetsFlags) _story.SetFlag(f);
            if (def.UnlocksCity.HasValue) _story.UnlockCity(def.UnlocksCity.Value);

            MissionPassed?.Invoke(run, pay);
        }
    }
}
