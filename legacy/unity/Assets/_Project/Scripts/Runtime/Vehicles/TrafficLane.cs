using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>Ordered waypoint chain authored in each city scene (child transforms).</summary>
    public sealed class TrafficLane : MonoBehaviour
    {
        [SerializeField] private bool loop = true;
        [SerializeField] private TrafficLane[] nextLanes;
        [Tooltip("District id for density lookups, e.g. lag_oshoja.")]
        [SerializeField] private string districtId;

        public string DistrictId => districtId;
        public int Count => transform.childCount;

        public Vector3 Point(int i) => transform.GetChild(Mathf.Clamp(i, 0, Count - 1)).position;

        public int Next(int i) => loop ? (i + 1) % Count : Mathf.Min(i + 1, Count - 1);

        public int Closest(Vector3 p)
        {
            int best = 0;
            float bestD = float.MaxValue;
            for (int i = 0; i < Count; i++)
            {
                float d = (Point(i) - p).sqrMagnitude;
                if (d < bestD) { bestD = d; best = i; }
            }
            return best;
        }

        private void OnDrawGizmos()
        {
            Gizmos.color = Color.yellow;
            for (int i = 0; i < Count - 1; i++) Gizmos.DrawLine(Point(i), Point(i + 1));
            if (loop && Count > 1) Gizmos.DrawLine(Point(Count - 1), Point(0));
        }
    }
}
