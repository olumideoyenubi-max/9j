using System;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.World
{
    /// <summary>
    /// Trigger volume marking a district. Drives the "entering Balo Market" banner, traffic and
    /// pedestrian density, radio ambience and safe zones (safehouse interiors) for wanted decay.
    /// </summary>
    [RequireComponent(typeof(Collider))]
    public sealed class DistrictZone : MonoBehaviour
    {
        public static DistrictZone Current { get; private set; }
        public static event Action<DistrictZone> Entered;

        [SerializeField] private string districtId;
        [SerializeField] private bool safeZone;

        public string DistrictId => districtId;
        public bool IsSafeZone => safeZone;

        private void Reset() => GetComponent<Collider>().isTrigger = true;

        private void OnTriggerEnter(Collider other)
        {
            if (!other.CompareTag("Player")) return;
            Current = this;
            if (GameBootstrap.Instance != null) GameBootstrap.Instance.PlayerInSafeZone = safeZone;
            Entered?.Invoke(this);
        }

        private void OnTriggerExit(Collider other)
        {
            if (!other.CompareTag("Player") || Current != this) return;
            if (GameBootstrap.Instance != null) GameBootstrap.Instance.PlayerInSafeZone = false;
        }
    }
}
