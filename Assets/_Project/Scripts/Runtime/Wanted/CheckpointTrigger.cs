using NaijaHustle.Core.Wanted;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.UI;
using NaijaHustle.Runtime.Vehicles;
using UnityEngine;

namespace NaijaHustle.Runtime.Wanted
{
    /// <summary>
    /// A road checkpoint (city or highway). Driving in stops the car and opens the four-way choice:
    /// Comply / Settle / Talk / Floor it. Driving through the spike line without stopping = CheckpointRunning.
    /// </summary>
    [RequireComponent(typeof(Collider))]
    public sealed class CheckpointTrigger : MonoBehaviour
    {
        [SerializeField] private CheckpointPanel panel;
        [SerializeField] private VehicleInteractor playerVehicles;
        [SerializeField] private Collider spikeLine;
        [SerializeField] private float cooldownSeconds = 120f;

        private float _lastUsed = -999f;

        /// <summary>Set by missions carrying contraband-ish cargo (e.g. the hard drives in abj_05).</summary>
        public static bool PlayerCarryingCargo { get; set; }

        private void OnTriggerEnter(Collider other)
        {
            if (!other.CompareTag("Player") || Time.time - _lastUsed < cooldownSeconds) return;
            if (playerVehicles == null || playerVehicles.Current == null) return; // on foot: just walk past
            _lastUsed = Time.time;

            var v = playerVehicles.Current;
            v.ControlBrake = true;
            panel.Open(choice => Resolve(choice, v));
        }

        private void Resolve(CheckpointChoice choice, Vehicle v)
        {
            var outcome = GameBootstrap.Session.ResolveCheckpoint(choice, PlayerCarryingCargo);
            panel.ShowOutcome(outcome);
            if (spikeLine != null) spikeLine.enabled = choice == CheckpointChoice.Floor;
        }
    }
}
