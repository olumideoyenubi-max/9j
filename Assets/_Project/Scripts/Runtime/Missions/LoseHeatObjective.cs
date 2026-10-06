using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// Drives LoseHeat objectives: on start it adds heat (the chase begins), and it completes the
    /// objective when stars hit zero. One instance lives in the Boot scene.
    /// </summary>
    public sealed class LoseHeatObjective : MonoBehaviour
    {
        [SerializeField] private float startingHeat = 2f;

        private MissionRun _armedRun;
        private int _armedIndex = -1;

        private void Update()
        {
            var s = GameBootstrap.Session;
            var run = s?.Missions.Active;
            var obj = run?.CurrentObjective;
            if (obj == null || obj.Type != ObjectiveType.LoseHeat) return;

            if (_armedRun != run || _armedIndex != run.ObjectiveIndex)
            {
                _armedRun = run;
                _armedIndex = run.ObjectiveIndex;
                if (!s.Wanted.IsWanted) s.Wanted.AddHeat(startingHeat);
                return;
            }

            if (!s.Wanted.IsWanted) s.Missions.Report(new ObjectiveEvent(ObjectiveType.LoseHeat, obj.TargetId));
        }
    }
}
