using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>
    /// Stealth objective volume. Guards (<see cref="GuardVision"/>) inside raise suspicion while
    /// they see the player; full suspicion fails the mission. Reaching the goal trigger completes it.
    /// </summary>
    public sealed class StealthZone : MonoBehaviour
    {
        [SerializeField] private string targetId;
        [SerializeField] private Collider goal;
        [SerializeField] private float suspicionPerSecond = 0.6f;
        [SerializeField] private float coolPerSecond = 0.25f;

        public float Suspicion { get; private set; }
        public static StealthZone Active { get; private set; }

        private bool IsCurrent
        {
            get
            {
                var obj = GameBootstrap.Session?.Missions.Active?.CurrentObjective;
                return obj != null && obj.Type == ObjectiveType.Stealth && obj.TargetId == targetId;
            }
        }

        private void Update()
        {
            if (!IsCurrent) { if (Active == this) Active = null; return; }
            Active = this;

            bool seen = false;
            foreach (var g in GetComponentsInChildren<GuardVision>())
                if (g.CanSeePlayer()) { seen = true; break; }

            Suspicion = Mathf.Clamp01(Suspicion + (seen ? suspicionPerSecond : -coolPerSecond) * Time.deltaTime);
            if (Suspicion >= 1f)
            {
                GameBootstrap.Session.Missions.Active.Fail("You were spotted");
                Suspicion = 0f;
                return;
            }

            var player = GameBootstrap.Instance.Player;
            if (player != null && goal != null && goal.bounds.Contains(player.position))
                GameBootstrap.Session.Missions.Report(new ObjectiveEvent(ObjectiveType.Stealth, targetId));
        }
    }
}
