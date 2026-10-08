using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Gantry crane for Port Harcourt's yard and port (ph_03, ph_07 and the Yard Crane Shift hustle).
    /// Stick moves the trolley (x) and gantry (y); Ascend/Descend run the hoist; Interact latches.
    /// The hook swings on a spring, so placement takes some skill: accuracy = hustle "quality".
    /// </summary>
    public sealed class CraneController : Vehicle
    {
        [SerializeField] private Transform gantry;
        [SerializeField] private Transform trolley;
        [SerializeField] private Transform hook;
        [SerializeField] private LineRenderer cable;
        [SerializeField] private Vector2 gantryRange = new Vector2(-40f, 40f);
        [SerializeField] private Vector2 trolleyRange = new Vector2(-12f, 12f);
        [SerializeField] private Vector2 hoistRange = new Vector2(2f, 25f);
        [SerializeField] private float moveSpeed = 4f;
        [SerializeField] private float hoistSpeed = 3f;
        [SerializeField] private float swingStiffness = 6f;
        [SerializeField] private float swingDamping = 1.2f;
        [SerializeField] private float latchRadius = 1.5f;
        [SerializeField] private LayerMask containerMask;

        private float _hoist = 10f;
        private Vector3 _swing;
        private Vector3 _swingVel;
        private Rigidbody _carried;

        public Rigidbody Carried => _carried;

        public override float SpeedKph => 0f;

        /// <summary>Fired when a container is set down in a target: (zone, quality 0..1).</summary>
        public event System.Action<CraneDropZone, float> Placed;

        private void Update()
        {
            if (!HasDriver) return;
            float dt = Time.deltaTime;

            Vector3 g = gantry.localPosition;
            g.z = Mathf.Clamp(g.z + ControlMove.y * moveSpeed * dt, gantryRange.x, gantryRange.y);
            Vector3 accel = -gantry.forward * (ControlMove.y * moveSpeed);
            gantry.localPosition = g;

            Vector3 t = trolley.localPosition;
            t.x = Mathf.Clamp(t.x + ControlMove.x * moveSpeed * dt, trolleyRange.x, trolleyRange.y);
            accel += -trolley.right * (ControlMove.x * moveSpeed);
            trolley.localPosition = t;

            float hoistInput = (ControlDown ? 1f : 0f) - (ControlUp ? 1f : 0f);
            _hoist = Mathf.Clamp(_hoist + hoistInput * hoistSpeed * dt, hoistRange.x, hoistRange.y);

            // Damped pendulum offset of the hook: moving the trolley makes the load swing.
            _swingVel += (accel * 0.5f - _swing * swingStiffness - _swingVel * swingDamping) * dt;
            _swing += _swingVel * dt;
            _swing.y = 0f;
            hook.position = trolley.position + Vector3.down * _hoist + _swing;

            if (cable != null)
            {
                cable.SetPosition(0, trolley.position);
                cable.SetPosition(1, hook.position);
            }

            var input = PlayerInputHub.Instance;
            if (input != null && input.Pressed(ActionButton.Interact)) ToggleLatch();
        }

        private void FixedUpdate()
        {
            if (_carried != null) _carried.MovePosition(hook.position + Vector3.down * 1.3f);
        }

        private void ToggleLatch()
        {
            if (_carried != null)
            {
                Release();
                return;
            }

            var hits = Physics.OverlapSphere(hook.position, latchRadius, containerMask, QueryTriggerInteraction.Ignore);
            foreach (var h in hits)
            {
                if (h.attachedRigidbody == null) continue;
                _carried = h.attachedRigidbody;
                _carried.isKinematic = true;
                return;
            }
        }

        private void Release()
        {
            var body = _carried;
            _carried = null;
            body.isKinematic = false;

            var zones = Physics.OverlapSphere(body.position, 2f, ~0, QueryTriggerInteraction.Collide);
            foreach (var z in zones)
            {
                var zone = z.GetComponent<CraneDropZone>();
                if (zone == null) continue;

                float quality = zone.Score(body.transform, _swing.magnitude);
                Placed?.Invoke(zone, quality);

                var session = GameBootstrap.Session;
                if (session == null) break;
                if (session.ActiveShift != null) session.CompleteShiftJob(quality);
                else session.Missions.Report(new ObjectiveEvent(ObjectiveType.OperateCrane, zone.TargetId));
                break;
            }
        }
    }
}
