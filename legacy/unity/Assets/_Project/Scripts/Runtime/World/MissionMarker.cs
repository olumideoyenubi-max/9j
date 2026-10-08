using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.World
{
    /// <summary>
    /// A world-space marker (mk_* ids in mission JSON). Reports GoTo / Deliver when the player (or the
    /// player's vehicle) enters, and only shows its beacon when the active objective points at it.
    /// </summary>
    [RequireComponent(typeof(Collider))]
    public sealed class MissionMarker : MonoBehaviour
    {
        [SerializeField] private string markerId;
        [SerializeField] private GameObject beacon;
        [Tooltip("Extra ids this marker also answers to, e.g. the '_clean' bonus variant of a delivery point.")]
        [SerializeField] private string[] aliases;

        public string MarkerId => markerId;

        private void Reset() => GetComponent<Collider>().isTrigger = true;

        private void Update()
        {
            if (beacon == null) return;
            var obj = GameBootstrap.Session?.Missions.Active?.CurrentObjective;
            beacon.SetActive(obj != null && Matches(obj.TargetId));
        }

        private void OnTriggerEnter(Collider other)
        {
            if (!other.CompareTag("Player")) return;
            var board = GameBootstrap.Session?.Missions;
            var obj = board?.Active?.CurrentObjective;
            if (obj == null || !Matches(obj.TargetId)) return;
            if (obj.Type == ObjectiveType.GoTo || obj.Type == ObjectiveType.Deliver)
                board.Report(new ObjectiveEvent(obj.Type, obj.TargetId));
        }

        private bool Matches(string id)
        {
            if (id == markerId) return true;
            if (aliases == null) return false;
            foreach (var a in aliases) if (a == id) return true;
            return false;
        }
    }
}
