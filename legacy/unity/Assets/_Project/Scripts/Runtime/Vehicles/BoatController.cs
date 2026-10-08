using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Speedboats, boat taxis and the supply vessel. Buoyancy from a few float points against a
    /// flat water height (lagoon/creek planes are flat — cheap and readable on mobile) plus waves.
    /// </summary>
    [RequireComponent(typeof(Rigidbody))]
    public sealed class BoatController : Vehicle
    {
        [SerializeField] private Transform[] floatPoints;
        [SerializeField] private Transform propeller;
        [SerializeField] private float waterLevel = 0f;
        [SerializeField] private float buoyancy = 12f;
        [SerializeField] private float waterDrag = 1.5f;
        [SerializeField] private float thrust = 14000f;
        [SerializeField] private float turnTorque = 5000f;
        [SerializeField] private float waveHeight = 0.15f;
        [SerializeField] private float waveSpeed = 1.2f;
        [SerializeField] private ParticleSystem wake;

        private Rigidbody _rb;

        public override float SpeedKph => _rb != null ? Vector3.Dot(_rb.linearVelocity, transform.forward) * 3.6f : 0f;
        public override bool IsOnWater => true;

        protected override void Awake()
        {
            base.Awake();
            _rb = GetComponent<Rigidbody>();
            if (Definition != null) thrust *= Definition.Acceleration;
        }

        private void FixedUpdate()
        {
            int submerged = 0;
            foreach (var p in floatPoints)
            {
                float wave = Mathf.Sin(Time.time * waveSpeed + p.position.x * 0.3f + p.position.z * 0.2f) * waveHeight;
                float depth = waterLevel + wave - p.position.y;
                if (depth <= 0f) continue;
                submerged++;
                _rb.AddForceAtPosition(Vector3.up * (buoyancy * Mathf.Clamp01(depth) * -Physics.gravity.y / floatPoints.Length), p.position, ForceMode.Acceleration);
            }

            if (submerged == 0) return;
            float k = (float)submerged / floatPoints.Length;
            _rb.linearVelocity = Vector3.Lerp(_rb.linearVelocity, new Vector3(_rb.linearVelocity.x * 0.995f, _rb.linearVelocity.y, _rb.linearVelocity.z * 0.995f), waterDrag * k * Time.fixedDeltaTime);

            // Sideways slip damping makes boats carve instead of sliding forever.
            var local = transform.InverseTransformDirection(_rb.linearVelocity);
            local.x *= 1f - 0.9f * Time.fixedDeltaTime * k;
            _rb.linearVelocity = transform.TransformDirection(local);

            if (IsDestroyed || !HasDriver) return;
            Vector3 at = propeller != null ? propeller.position : transform.position;
            _rb.AddForceAtPosition(transform.forward * (ControlMove.y * thrust * k), at);
            float steerAuthority = Mathf.Clamp01(Mathf.Abs(local.z) / 5f) + 0.2f;
            _rb.AddTorque(Vector3.up * (ControlMove.x * turnTorque * steerAuthority * k));

            if (wake != null)
            {
                var emission = wake.emission;
                emission.rateOverTime = Mathf.Abs(local.z) * 4f;
            }
        }
    }
}
