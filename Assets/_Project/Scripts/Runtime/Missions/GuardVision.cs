using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.Missions
{
    /// <summary>View cone for guards, cameras and the gate aunty.</summary>
    public sealed class GuardVision : MonoBehaviour
    {
        [SerializeField] private float range = 14f;
        [SerializeField] private float halfAngle = 45f;
        [SerializeField] private LayerMask occlusion = 1;

        public bool CanSeePlayer()
        {
            var player = GameBootstrap.Instance?.Player;
            if (player == null) return false;
            Vector3 to = player.position + Vector3.up - transform.position;
            if (to.sqrMagnitude > range * range) return false;
            if (Vector3.Angle(transform.forward, to) > halfAngle) return false;
            // Crouching/darkness modifiers would plug in here.
            return !Physics.Raycast(transform.position, to.normalized, to.magnitude - 0.3f, occlusion, QueryTriggerInteraction.Ignore);
        }
    }
}
