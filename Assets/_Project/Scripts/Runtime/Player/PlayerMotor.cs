using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using UnityEngine;

namespace NaijaHustle.Runtime.Player
{
    /// <summary>Third-person on-foot movement, camera-relative, using a CharacterController (cheap on mobile).</summary>
    [RequireComponent(typeof(CharacterController))]
    public sealed class PlayerMotor : MonoBehaviour
    {
        [SerializeField] private Transform cameraPivot;
        [SerializeField] private Animator animator;
        [SerializeField] private float walkSpeed = 2.2f;
        [SerializeField] private float runSpeed = 5.5f;
        [SerializeField] private float sprintSpeed = 8f;
        [SerializeField] private float turnSpeed = 720f;
        [SerializeField] private float jumpHeight = 1.1f;
        [SerializeField] private float gravity = -20f;

        private static readonly int SpeedParam = Animator.StringToHash("Speed");
        private static readonly int GroundedParam = Animator.StringToHash("Grounded");

        private CharacterController _cc;
        private float _verticalVelocity;

        public bool ControlEnabled { get; set; } = true;

        private void Awake() => _cc = GetComponent<CharacterController>();

        private void OnEnable()
        {
            if (GameBootstrap.Instance != null) GameBootstrap.Instance.Player = transform;
        }

        private void Update()
        {
            var input = PlayerInputHub.Instance;
            Vector2 move = ControlEnabled && input != null ? input.Move : Vector2.zero;

            Vector3 fwd = cameraPivot != null ? Vector3.ProjectOnPlane(cameraPivot.forward, Vector3.up).normalized : Vector3.forward;
            Vector3 right = Vector3.Cross(Vector3.up, fwd);
            Vector3 dir = fwd * move.y + right * move.x;

            float mag = Mathf.Clamp01(move.magnitude);
            float speed = mag < 0.5f ? walkSpeed * mag * 2f : runSpeed;
            if (input != null && input.Held(ActionButton.Sprint) && mag > 0.5f) speed = sprintSpeed;

            if (dir.sqrMagnitude > 0.001f)
            {
                var target = Quaternion.LookRotation(dir, Vector3.up);
                transform.rotation = Quaternion.RotateTowards(transform.rotation, target, turnSpeed * Time.deltaTime);
            }

            if (_cc.isGrounded)
            {
                _verticalVelocity = -2f;
                if (ControlEnabled && input != null && input.Pressed(ActionButton.Jump))
                    _verticalVelocity = Mathf.Sqrt(jumpHeight * -2f * gravity);
            }
            _verticalVelocity += gravity * Time.deltaTime;

            Vector3 velocity = dir.normalized * (speed * mag) + Vector3.up * _verticalVelocity;
            _cc.Move(velocity * Time.deltaTime);

            if (animator != null)
            {
                animator.SetFloat(SpeedParam, speed * mag, 0.1f, Time.deltaTime);
                animator.SetBool(GroundedParam, _cc.isGrounded);
            }
        }

        /// <summary>Teleport safely (CharacterController ignores transform writes while enabled).</summary>
        public void Warp(Vector3 position, Quaternion rotation)
        {
            _cc.enabled = false;
            transform.SetPositionAndRotation(position, rotation);
            _cc.enabled = true;
        }
    }
}
