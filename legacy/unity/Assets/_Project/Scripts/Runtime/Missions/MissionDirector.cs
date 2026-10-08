using System.Collections.Generic;
using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// Bridges the engine-free <see cref="MissionBoard"/> to the scene: streams each mission's
    /// set-dressing prefab (NPCs, cargo, markers) in on start and out on finish, sends the giver's
    /// chat message when a mission becomes available, and shows pass/fail cards.
    /// </summary>
    public sealed class MissionDirector : MonoBehaviour
    {
        [SerializeField] private UI.MissionResultCard resultCard;

        private AsyncOperationHandle<GameObject> _setHandle;
        private readonly HashSet<string> _announced = new HashSet<string>();

        private void Start()
        {
            var s = GameBootstrap.Session;
            s.Missions.MissionStarted += OnStarted;
            s.Missions.MissionPassed += OnPassed;
            s.Missions.MissionFailed += OnFailed;
            foreach (var m in s.Chat.All) if (m.MissionId != null) _announced.Add(m.MissionId);
            AnnounceAvailable();
        }

        private void OnDestroy()
        {
            var s = GameBootstrap.Session;
            if (s == null) return;
            s.Missions.MissionStarted -= OnStarted;
            s.Missions.MissionPassed -= OnPassed;
            s.Missions.MissionFailed -= OnFailed;
            ReleaseSet();
        }

        /// <summary>Text the player from each giver whose mission just opened up.</summary>
        private void AnnounceAvailable()
        {
            var s = GameBootstrap.Session;
            foreach (var m in s.Missions.Available(s.Story.CurrentCity))
            {
                if (!_announced.Add(m.Id) || string.IsNullOrEmpty(m.Giver)) continue;
                s.Chat.Receive(m.Giver, m.Summary, s.Clock.TotalMinutes, m.Id);
            }
        }

        private void OnStarted(MissionRun run)
        {
            if (string.IsNullOrEmpty(run.Definition.AssetKey)) return;
            _setHandle = Addressables.InstantiateAsync(run.Definition.AssetKey);
        }

        private void OnPassed(MissionRun run, long pay)
        {
            ReleaseSet();
            if (resultCard != null) resultCard.ShowPassed(run.Definition.Title, pay, run.Definition.CredReward);
            GameBootstrap.Instance.Save();
            AnnounceAvailable();
        }

        private void OnFailed(MissionRun run)
        {
            ReleaseSet();
            if (resultCard != null) resultCard.ShowFailed(run.Definition.Title, run.FailReason ?? "Abandoned");
        }

        private void ReleaseSet()
        {
            if (_setHandle.IsValid()) Addressables.ReleaseInstance(_setHandle);
            _setHandle = default;
        }

        /// <summary>Called by the mission start blip or the phone's chat "Accept" button.</summary>
        public static bool TryStart(string missionId) => GameBootstrap.Session.Missions.TryStart(missionId);
    }
}
