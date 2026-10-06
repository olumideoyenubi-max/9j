using UnityEngine;
using UnityEngine.InputSystem;

namespace NaijaHustle.Runtime.Input
{
    public enum ActionButton
    {
        Jump,
        Sprint,
        Interact,   // enter/exit vehicle, talk, pick up
        Attack,
        Brake,      // handbrake in vehicles
        Horn,
        Phone,
        Camera,     // phone camera for Photograph objectives
        Radio,      // next station
        Ascend,     // helicopter / crane hoist up
        Descend,
    }

    /// <summary>
    /// One place gameplay reads input from, whatever the device: on-screen joystick + buttons on
    /// Android, keyboard/mouse or gamepad on PC. Touch widgets write into it; the Input System
    /// bindings are created in code so there's no .inputactions asset to keep in sync.
    /// </summary>
    [DefaultExecutionOrder(-500)]
    public sealed class PlayerInputHub : MonoBehaviour
    {
        public static PlayerInputHub Instance { get; private set; }

        [SerializeField] private GameObject touchControlsRoot;
        [SerializeField] private float mouseLookScale = 0.1f;
        [SerializeField] private float stickLookScale = 180f;

        private const int ButtonCount = 11;
        private readonly bool[] _touchHeld = new bool[ButtonCount];
        private readonly bool[] _held = new bool[ButtonCount];
        private readonly bool[] _pressed = new bool[ButtonCount];
        private readonly InputAction[] _buttons = new InputAction[ButtonCount];

        private InputAction _move;
        private InputAction _look;
        private InputAction _stickLook;

        public Vector2 TouchMove { get; set; }
        public Vector2 TouchLookDelta { get; set; }

        /// <summary>Movement / throttle+steer, -1..1 per axis.</summary>
        public Vector2 Move { get; private set; }
        /// <summary>Camera look delta in degrees this frame.</summary>
        public Vector2 Look { get; private set; }

        public bool Held(ActionButton b) => _held[(int)b];
        public bool Pressed(ActionButton b) => _pressed[(int)b];

        public void SetTouchButton(ActionButton b, bool down) => _touchHeld[(int)b] = down;

        public static bool UseTouchControls => Application.isMobilePlatform || Touchscreen.current != null && Keyboard.current == null;

        private void Awake()
        {
            if (Instance != null) { Destroy(gameObject); return; }
            Instance = this;

            _move = new InputAction("Move", InputActionType.Value);
            _move.AddCompositeBinding("2DVector")
                .With("Up", "<Keyboard>/w").With("Down", "<Keyboard>/s")
                .With("Left", "<Keyboard>/a").With("Right", "<Keyboard>/d");
            _move.AddBinding("<Gamepad>/leftStick");

            _look = new InputAction("Look", InputActionType.Value, "<Mouse>/delta");
            _stickLook = new InputAction("StickLook", InputActionType.Value, "<Gamepad>/rightStick");

            Bind(ActionButton.Jump, "<Keyboard>/space", "<Gamepad>/buttonSouth");
            Bind(ActionButton.Sprint, "<Keyboard>/leftShift", "<Gamepad>/leftStickPress");
            Bind(ActionButton.Interact, "<Keyboard>/f", "<Gamepad>/buttonNorth");
            Bind(ActionButton.Attack, "<Mouse>/leftButton", "<Gamepad>/rightTrigger");
            Bind(ActionButton.Brake, "<Keyboard>/space", "<Gamepad>/buttonEast");
            Bind(ActionButton.Horn, "<Keyboard>/h", "<Gamepad>/rightStickPress");
            Bind(ActionButton.Phone, "<Keyboard>/tab", "<Gamepad>/dpad/up");
            Bind(ActionButton.Camera, "<Keyboard>/c", "<Gamepad>/dpad/down");
            Bind(ActionButton.Radio, "<Keyboard>/r", "<Gamepad>/dpad/right");
            Bind(ActionButton.Ascend, "<Keyboard>/e", "<Gamepad>/rightShoulder");
            Bind(ActionButton.Descend, "<Keyboard>/q", "<Gamepad>/leftShoulder");

            if (touchControlsRoot != null) touchControlsRoot.SetActive(UseTouchControls);
        }

        private void Bind(ActionButton b, string kb, string pad)
        {
            var a = new InputAction(b.ToString(), InputActionType.Button);
            a.AddBinding(kb);
            a.AddBinding(pad);
            _buttons[(int)b] = a;
        }

        private void OnEnable()
        {
            _move.Enable(); _look.Enable(); _stickLook.Enable();
            foreach (var a in _buttons) a.Enable();
        }

        private void OnDisable()
        {
            _move.Disable(); _look.Disable(); _stickLook.Disable();
            foreach (var a in _buttons) a.Disable();
        }

        private void Update()
        {
            var move = _move.ReadValue<Vector2>() + TouchMove;
            Move = Vector2.ClampMagnitude(move, 1f);

            Look = _look.ReadValue<Vector2>() * mouseLookScale
                 + _stickLook.ReadValue<Vector2>() * (stickLookScale * Time.deltaTime)
                 + TouchLookDelta;
            TouchLookDelta = Vector2.zero; // touch look is a per-frame delta

            for (int i = 0; i < ButtonCount; i++)
            {
                bool now = _buttons[i].IsPressed() || _touchHeld[i];
                _pressed[i] = now && !_held[i];
                _held[i] = now;
            }
        }
    }
}
