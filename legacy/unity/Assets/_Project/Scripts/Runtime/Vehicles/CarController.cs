using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Arcade raycast car: no WheelColliders (expensive and twitchy on mobile). Each wheel is a
    /// spring-damper ray; grip is lateral velocity cancellation. Tuned per vehicle via the definition
    /// (danfo = heavy and wallowy, okada = twitchy, Pathmaster = planted).
    /// </summary>
    [RequireComponent(typeof(Rigidbody))]
    public sealed class CarController : Vehicle
    {
        [System.Serializable]
        private struct Wheel
        {
            public Transform anchor;
            public Transform visual;
            public bool steer;
            public bool drive;
            [HideInInspector] public float compression;
            [HideInInspector] public float spin;
        }

        [SerializeField] private Wheel[] wheels;
        [SerializeField] private float wheelRadius = 0.35f;
        [SerializeField] private float suspensionRest = 0.45f;
        [SerializeField] private float springStrength = 35000f;
        [SerializeField] private float springDamper = 3500f;
        [SerializeField] private float engineForce = 9000f;
        [SerializeField] private float brakeForce = 12000f;
        [SerializeField] private float maxSteerAngle = 32f;
        [SerializeField] private float gripFactor = 0.8f;
        [SerializeField] private float handbrakeGrip = 0.25f;
        [SerializeField] private Vector3 centerOfMass = new Vector3(0f, -0.4f, 0f);
        [SerializeField] private LayerMask groundMask = ~0;
        [Tooltip("Two-wheelers lean into turns and auto-balance.")]
        [SerializeField] private bool isBike;

        private Rigidbody _rb;
        private float _topSpeedMs;
        private float _steer;

        public override float SpeedKph => _rb != null ? Vector3.Dot(_rb.linearVelocity, transform.forward) * 3.6f : 0f;

        protected override void Awake()
        {
            base.Awake();
            _rb = GetComponent<Rigidbody>();
            _rb.centerOfMass = centerOfMass;
            _rb.interpolation = RigidbodyInterpolation.Interpolate;
            float top = Definition != null ? Definition.TopSpeedKph : 160f;
            _topSpeedMs = top / 3.6f;
            if (Definition != null)
            {
                engineForce *= Definition.Acceleration;
                gripFactor *= Mathf.Lerp(0.8f, 1.2f, Definition.Handling * 0.5f);
            }
        }

        /// <summary>Apply garage mod stats (top speed / handling bonuses).</summary>
        public void ApplyStats(float topSpeedKph, float handling)
        {
            _topSpeedMs = topSpeedKph / 3.6f;
            gripFactor = 0.8f * Mathf.Lerp(0.8f, 1.2f, handling * 0.5f);
        }

        private void FixedUpdate()
        {
            if (IsDestroyed) return;
            float dt = Time.fixedDeltaTime;
            float throttle = ControlMove.y;
            float speed = Vector3.Dot(_rb.linearVelocity, transform.forward);
            float speedFactor = Mathf.Clamp01(Mathf.Abs(speed) / _topSpeedMs);

            // Less steering lock at speed so the Third Lagoon Bridge doesn't end in the lagoon.
            float targetSteer = ControlMove.x * maxSteerAngle * Mathf.Lerp(1f, 0.35f, speedFactor);
            _steer = Mathf.MoveTowards(_steer, targetSteer, 120f * dt);

            int driven = 0;
            foreach (var w in wheels) if (w.drive) driven++;
            driven = Mathf.Max(1, driven);

            for (int i = 0; i < wheels.Length; i++)
            {
                var w = wheels[i];
                if (w.steer) w.anchor.localRotation = Quaternion.Euler(0f, _steer, 0f);

                Vector3 origin = w.anchor.position;
                Vector3 down = -w.anchor.up;
                float maxLen = suspensionRest + wheelRadius;

                if (Physics.Raycast(origin, down, out var hit, maxLen, groundMask, QueryTriggerInteraction.Ignore))
                {
                    float compression = maxLen - hit.distance;
                    Vector3 pointVel = _rb.GetPointVelocity(origin);

                    // Suspension
                    float springVel = Vector3.Dot(w.anchor.up, pointVel);
                    float spring = compression * springStrength - springVel * springDamper;
                    _rb.AddForceAtPosition(w.anchor.up * spring, origin);

                    // Lateral grip
                    Vector3 side = w.anchor.right;
                    float lateral = Vector3.Dot(side, pointVel);
                    float grip = ControlBrake && !w.steer ? handbrakeGrip : gripFactor;
                    float mass = _rb.mass / wheels.Length;
                    _rb.AddForceAtPosition(-side * lateral * grip * mass / dt, hit.point);

                    // Drive / brake
                    Vector3 fwd = w.anchor.forward;
                    if (w.drive && Mathf.Abs(throttle) > 0.01f)
                    {
                        bool braking = speed > 0.5f && throttle < 0f || speed < -0.5f && throttle > 0f;
                        float force = braking ? brakeForce : engineForce * (1f - speedFactor * speedFactor);
                        if (!braking && throttle < 0f) force *= 0.4f; // reverse is slow
                        _rb.AddForceAtPosition(fwd * throttle * force / driven, hit.point);
                    }
                    else if (ControlBrake || !HasDriver)
                    {
                        float roll = Vector3.Dot(fwd, pointVel);
                        _rb.AddForceAtPosition(-fwd * Mathf.Clamp(roll * mass, -brakeForce, brakeForce) / wheels.Length, hit.point);
                    }

                    w.compression = compression;
                    w.spin += Vector3.Dot(fwd, pointVel) / wheelRadius * Mathf.Rad2Deg * dt;
                }
                else
                {
                    w.compression = 0f;
                }
                wheels[i] = w;
            }

            if (isBike) Balance();
        }

        private void Balance()
        {
            // Lean into the turn, otherwise stay upright.
            float speed = _rb.linearVelocity.magnitude;
            float lean = -_steer * Mathf.Clamp01(speed / 15f) * 0.8f;
            var targetUp = Quaternion.AngleAxis(lean, transform.forward) * Vector3.up;
            var torque = Vector3.Cross(transform.up, targetUp) * 400f - _rb.angularVelocity * 40f;
            _rb.AddTorque(Vector3.Project(torque, transform.forward), ForceMode.Acceleration);
        }

        private void LateUpdate()
        {
            foreach (var w in wheels)
            {
                if (w.visual == null) continue;
                w.visual.localPosition = new Vector3(0f, -(suspensionRest - w.compression), 0f);
                w.visual.localRotation = Quaternion.Euler(w.spin, w.steer ? _steer : 0f, 0f);
            }
        }

        private void OnCollisionEnter(Collision c)
        {
            float impact = c.relativeVelocity.magnitude;
            if (impact > 6f) ApplyDamage((impact - 6f) * 1.5f);
        }
    }
}
