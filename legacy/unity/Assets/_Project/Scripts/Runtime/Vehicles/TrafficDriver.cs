using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Minimal lane-follow AI for ambient traffic: follows a <see cref="TrafficLane"/> waypoint
    /// chain, brakes for whatever is ahead and honks (it's Lagos). Kept simple for mobile CPU budgets.
    /// </summary>
    [RequireComponent(typeof(Vehicle))]
    public sealed class TrafficDriver : MonoBehaviour
    {
        [SerializeField] private float cruiseKph = 45f;
        [SerializeField] private float lookAhead = 9f;
        [SerializeField] private float honkChancePerSecond = 0.05f;
        [SerializeField] private AudioSource horn;
        [SerializeField] private GameObject driverVisual;

        private Vehicle _vehicle;
        private TrafficLane _lane;
        private int _index;

        public System.Action<TrafficDriver> Despawn;

        private void Awake() => _vehicle = GetComponent<Vehicle>();

        public void Begin(TrafficLane lane, int index, float speedMultiplier)
        {
            _lane = lane;
            _index = index;
            cruiseKph *= speedMultiplier;
            enabled = true;
            if (driverVisual != null) driverVisual.SetActive(true);
            _vehicle.SetDriver(false);
        }

        /// <summary>Player pulled the driver out.</summary>
        public void Eject()
        {
            if (driverVisual != null) driverVisual.SetActive(false);
            // Spawn a fleeing/angry pedestrian here in the full game.
            enabled = false;
        }

        private void FixedUpdate()
        {
            if (_lane == null || _vehicle.PlayerDriving) return;
            Vector3 target = _lane.Point(_index);
            Vector3 to = target - transform.position;
            to.y = 0f;
            if (to.magnitude < 4f) _index = _lane.Next(_index);

            float steer = Vector3.SignedAngle(transform.forward, to, Vector3.up) / 35f;
            float throttle = Mathf.Clamp01((cruiseKph - _vehicle.SpeedKph) / 15f);

            if (Physics.Raycast(transform.position + Vector3.up * 0.7f, transform.forward, out var hit, lookAhead, ~0, QueryTriggerInteraction.Ignore)
                && hit.rigidbody != null && hit.rigidbody != GetComponent<Rigidbody>())
            {
                throttle = -0.6f;
                if (horn != null && !horn.isPlaying && Random.value < honkChancePerSecond * Time.fixedDeltaTime * 10f) horn.Play();
            }

            _vehicle.ControlMove = new Vector2(Mathf.Clamp(steer, -1f, 1f), throttle);
            _vehicle.ControlBrake = throttle < 0f;
        }
    }
}
