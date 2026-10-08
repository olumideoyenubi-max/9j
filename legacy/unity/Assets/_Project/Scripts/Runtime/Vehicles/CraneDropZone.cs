using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>A target slot (barge bay, stack, debris pile) for the crane. Scores alignment and swing.</summary>
    [RequireComponent(typeof(Collider))]
    public sealed class CraneDropZone : MonoBehaviour
    {
        [Tooltip("Mission objective target id, e.g. 'container' or 'valve_master'.")]
        [SerializeField] private string targetId = "container";
        [SerializeField] private float perfectDistance = 0.4f;
        [SerializeField] private float worstDistance = 2.5f;

        public string TargetId => targetId;

        public float Score(Transform load, float swing)
        {
            float dist = Vector3.ProjectOnPlane(load.position - transform.position, Vector3.up).magnitude;
            float pos = 1f - Mathf.InverseLerp(perfectDistance, worstDistance, dist);
            float angle = Mathf.Abs(Mathf.DeltaAngle(load.eulerAngles.y, transform.eulerAngles.y)) % 180f;
            angle = Mathf.Min(angle, 180f - angle);
            float align = 1f - Mathf.Clamp01(angle / 30f);
            float steady = 1f - Mathf.Clamp01(swing / 1.5f);
            return Mathf.Clamp01(pos * 0.6f + align * 0.25f + steady * 0.15f);
        }
    }
}
