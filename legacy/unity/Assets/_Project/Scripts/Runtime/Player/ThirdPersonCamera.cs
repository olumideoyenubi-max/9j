using NaijaHustle.Runtime.Input;
using UnityEngine;

namespace NaijaHustle.Runtime.Player
{
    /// <summary>
    /// Orbit camera with collision. Follows the player on foot, pulls back and auto-centres behind
    /// vehicles. Written by hand (no Cinemachine) to keep the base install lean.
    /// </summary>
    public sealed class ThirdPersonCamera : MonoBehaviour
    {
        [SerializeField] private Transform target;
        [SerializeField] private Vector3 pivotOffset = new Vector3(0f, 1.6f, 0f);
        [SerializeField] private float distance = 4.5f;
        [SerializeField] private float vehicleDistance = 8f;
        [SerializeField] private float minPitch = -30f;
        [SerializeField] private float maxPitch = 70f;
        [SerializeField] private float followSharpness = 12f;
        [SerializeField] private float autoCenterDelay = 1.5f;
        [SerializeField] private LayerMask collisionMask = ~0;
        [SerializeField] private float collisionRadius = 0.25f;

        private float _yaw;
        private float _pitch = 15f;
        private float _currentDistance;
        private float _sinceLook;
        private bool _inVehicle;

        public Transform Pivot => transform;

        public void Follow(Transform newTarget, bool isVehicle)
        {
            target = newTarget;
            _inVehicle = isVehicle;
        }

        private void LateUpdate()
        {
            if (target == null) return;
            var input = PlayerInputHub.Instance;
            Vector2 look = input != null ? input.Look : Vector2.zero;

            if (look.sqrMagnitude > 0.0001f) _sinceLook = 0f;
            else _sinceLook += Time.deltaTime;

            _yaw += look.x;
            _pitch = Mathf.Clamp(_pitch - look.y, minPitch, maxPitch);

            // In vehicles, drift back behind the car when the player stops steering the camera.
            if (_inVehicle && _sinceLook > autoCenterDelay)
            {
                _yaw = Mathf.LerpAngle(_yaw, target.eulerAngles.y, 1f - Mathf.Exp(-3f * Time.deltaTime));
                _pitch = Mathf.Lerp(_pitch, 12f, 1f - Mathf.Exp(-2f * Time.deltaTime));
            }

            var rot = Quaternion.Euler(_pitch, _yaw, 0f);
            Vector3 pivot = target.position + pivotOffset * (_inVehicle ? 1.3f : 1f);
            float wanted = _inVehicle ? vehicleDistance : distance;

            if (Physics.SphereCast(pivot, collisionRadius, rot * Vector3.back, out var hit, wanted, collisionMask, QueryTriggerInteraction.Ignore))
                wanted = Mathf.Max(0.5f, hit.distance - 0.1f);

            _currentDistance = Mathf.Lerp(_currentDistance <= 0 ? wanted : _currentDistance, wanted, 1f - Mathf.Exp(-followSharpness * Time.deltaTime));
            transform.SetPositionAndRotation(pivot + rot * Vector3.back * _currentDistance, rot);
        }
    }
}
