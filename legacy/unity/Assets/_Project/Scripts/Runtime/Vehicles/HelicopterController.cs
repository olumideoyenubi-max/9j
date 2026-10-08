using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Assisted helicopter: auto-hover, stick = pitch/roll toward a direction, Ascend/Descend for
    /// collective. Deliberately forgiving — it's the late-game escape tool, not a sim.
    /// </summary>
    [RequireComponent(typeof(Rigidbody))]
    public sealed class HelicopterController : Vehicle
    {
        [SerializeField] private Transform mainRotor;
        [SerializeField] private float liftAccel = 6f;
        [SerializeField] private float moveAccel = 14f;
        [SerializeField] private float maxTilt = 22f;
        [SerializeField] private float yawRate = 70f;
        [SerializeField] private float spinUpSeconds = 3f;

        private Rigidbody _rb;
        private float _rotor;

        public override float SpeedKph => _rb != null ? _rb.linearVelocity.magnitude * 3.6f : 0f;

        protected override void Awake()
        {
            base.Awake();
            _rb = GetComponent<Rigidbody>();
        }

        private void FixedUpdate()
        {
            float dt = Time.fixedDeltaTime;
            _rotor = Mathf.MoveTowards(_rotor, HasDriver && !IsDestroyed ? 1f : 0f, dt / spinUpSeconds);
            if (_rotor <= 0.01f) return;

            // Hover: cancel gravity, then collective adds/subtracts climb.
            float collective = (ControlUp ? 1f : 0f) - (ControlDown ? 1f : 0f);
            _rb.AddForce(-Physics.gravity * _rotor, ForceMode.Acceleration);
            _rb.AddForce(Vector3.up * (collective * liftAccel * _rotor), ForceMode.Acceleration);

            // Horizontal: move in the heading frame; heading turns toward stick x.
            var heading = Quaternion.Euler(0f, transform.eulerAngles.y, 0f);
            Vector3 wish = heading * new Vector3(ControlMove.x * 0.4f, 0f, ControlMove.y);
            _rb.AddForce(wish * (moveAccel * _rotor), ForceMode.Acceleration);
            _rb.AddForce(-new Vector3(_rb.linearVelocity.x, _rb.linearVelocity.y * 2f, _rb.linearVelocity.z) * (0.3f * _rotor), ForceMode.Acceleration);

            float yaw = transform.eulerAngles.y + ControlMove.x * yawRate * dt;
            var tilt = Quaternion.Euler(ControlMove.y * maxTilt, yaw, -ControlMove.x * maxTilt * 0.6f);
            _rb.MoveRotation(Quaternion.Slerp(_rb.rotation, tilt, 4f * dt));
            _rb.angularVelocity = Vector3.zero;
        }

        private void Update()
        {
            if (mainRotor != null) mainRotor.Rotate(0f, _rotor * 1400f * Time.deltaTime, 0f, Space.Self);
        }
    }
}
