using NaijaHustle.Core.Missions;
using NaijaHustle.Core.Wanted;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using NaijaHustle.Runtime.Player;
using NaijaHustle.Runtime.Wanted;
using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Enter / exit vehicles. Taking an occupied or unowned vehicle in view of witnesses is
    /// VehicleTheft. While driving, routes input from the hub into the vehicle's controls.
    /// </summary>
    public sealed class VehicleInteractor : MonoBehaviour
    {
        [SerializeField] private PlayerMotor motor;
        [SerializeField] private CharacterController characterController;
        [SerializeField] private ThirdPersonCamera cam;
        [SerializeField] private GameObject characterVisual;
        [SerializeField] private float enterRange = 3f;
        [SerializeField] private LayerMask vehicleMask;
        [SerializeField] private TouchButton contextButton;

        private readonly Collider[] _hits = new Collider[8];

        public Vehicle Current { get; private set; }
        public Vehicle Nearest { get; private set; }

        public event System.Action<Vehicle> Entered;
        public event System.Action<Vehicle> Exited;

        private void Update()
        {
            var input = PlayerInputHub.Instance;
            if (input == null) return;

            if (Current == null)
            {
                Nearest = FindNearest();
                if (contextButton != null) contextButton.ContextVisible = Nearest != null;
                if (Nearest != null && input.Pressed(ActionButton.Interact)) Enter(Nearest);
                return;
            }

            Current.ControlMove = input.Move;
            Current.ControlBrake = input.Held(ActionButton.Brake);
            Current.ControlUp = input.Held(ActionButton.Ascend);
            Current.ControlDown = input.Held(ActionButton.Descend);

            // Crane uses Interact for its latch; exit the crane with Phone-hold or by a dedicated UI button.
            bool exitPressed = Current is CraneController ? input.Pressed(ActionButton.Jump) : input.Pressed(ActionButton.Interact);
            if (exitPressed && Mathf.Abs(Current.SpeedKph) < 15f) Exit();
        }

        private Vehicle FindNearest()
        {
            int n = Physics.OverlapSphereNonAlloc(transform.position, enterRange, _hits, vehicleMask, QueryTriggerInteraction.Ignore);
            Vehicle best = null;
            float bestDist = float.MaxValue;
            for (int i = 0; i < n; i++)
            {
                var v = _hits[i].GetComponentInParent<Vehicle>();
                if (v == null || v.IsDestroyed || v.PlayerDriving) continue;
                float d = (v.transform.position - transform.position).sqrMagnitude;
                if (d < bestDist) { bestDist = d; best = v; }
            }
            return best;
        }

        public void Enter(Vehicle v)
        {
            var session = GameBootstrap.Session;
            bool stolen = !v.OwnedByPlayer && !IsMissionVehicle(v);
            if (stolen && session != null)
                CrimeReporter.Report(CrimeType.VehicleTheft, v.transform.position, v.HasDriver ? 1f : 0.5f);

            if (v.HasDriver) v.GetComponent<TrafficDriver>()?.Eject();

            Current = v;
            v.SetDriver(true);
            motor.ControlEnabled = false;
            characterController.enabled = false;
            if (characterVisual != null) characterVisual.SetActive(false);
            transform.SetParent(v.DriverSeat, false);
            transform.localPosition = Vector3.zero;
            transform.localRotation = Quaternion.identity;
            cam.Follow(v.transform, true);

            session?.Missions.Report(new ObjectiveEvent(ObjectiveType.EnterVehicle, v.ClassKey));
            session?.Missions.Report(new ObjectiveEvent(ObjectiveType.EnterVehicle, v.VehicleId));
            Entered?.Invoke(v);
        }

        public void Exit()
        {
            var v = Current;
            if (v == null) return;
            v.ClearDriver();
            Current = null;

            transform.SetParent(null, true);
            motor.Warp(v.ExitPoint.position, Quaternion.LookRotation(Vector3.ProjectOnPlane(v.transform.forward, Vector3.up)));
            characterController.enabled = true;
            if (characterVisual != null) characterVisual.SetActive(true);
            motor.ControlEnabled = true;
            cam.Follow(transform, false);
            Exited?.Invoke(v);
        }

        private static bool IsMissionVehicle(Vehicle v) => v.GetComponent<MissionVehicleTag>() != null;
    }
}
