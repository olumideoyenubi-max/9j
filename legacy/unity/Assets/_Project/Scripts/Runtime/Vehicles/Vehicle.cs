using NaijaHustle.Core.Economy;
using UnityEngine;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Common base for everything the player can drive: cars, okadas, kekes, danfos, boats,
    /// helicopters, the crane. Holds identity, seats, health and the driver's control inputs.
    /// </summary>
    public abstract class Vehicle : MonoBehaviour
    {
        [SerializeField] private string vehicleId;
        [SerializeField] private Transform driverSeat;
        [SerializeField] private Transform exitPoint;
        [SerializeField] private Transform[] passengerSeats;
        [SerializeField] private AudioSource radioSource;
        [SerializeField] private float maxHealth = 100f;

        public string VehicleId => vehicleId;
        public VehicleDefinition Definition { get; private set; }
        public Transform DriverSeat => driverSeat;
        public Transform ExitPoint => exitPoint != null ? exitPoint : transform;
        public Transform[] PassengerSeats => passengerSeats;
        public AudioSource RadioSource => radioSource;

        public float Health { get; protected set; }
        public bool HasDriver { get; private set; }
        public bool PlayerDriving { get; private set; }
        /// <summary>Owned by the player (from a garage) — taking it is not a crime.</summary>
        public bool OwnedByPlayer { get; set; }
        public bool IsDestroyed => Health <= 0f;

        /// <summary>Throttle/steer (-1..1). For aircraft/crane: x = yaw/trolley, y = pitch/forward.</summary>
        public Vector2 ControlMove { get; set; }
        public bool ControlBrake { get; set; }
        public bool ControlUp { get; set; }
        public bool ControlDown { get; set; }

        public abstract float SpeedKph { get; }
        public virtual bool IsOnWater => false;

        protected virtual void Awake()
        {
            Health = maxHealth;
            var session = Bootstrap.GameBootstrap.Session;
            if (session != null) Definition = session.Content.Data.Vehicles.Find(v => v.Id == vehicleId);
            if (Definition != null) Health = maxHealth = Definition.Durability;
        }

        public void SetDriver(bool player)
        {
            HasDriver = true;
            PlayerDriving = player;
        }

        public void ClearDriver()
        {
            HasDriver = false;
            PlayerDriving = false;
            ControlMove = Vector2.zero;
            ControlBrake = true;
            ControlUp = ControlDown = false;
        }

        public virtual void ApplyDamage(float amount)
        {
            if (IsDestroyed) return;
            Health = Mathf.Max(0f, Health - amount);
            if (IsDestroyed) OnDestroyed();
        }

        protected virtual void OnDestroyed() { }

        /// <summary>Class key used by mission objectives, e.g. "danfo", "boat", "helicopter".</summary>
        public string ClassKey => Definition != null ? Definition.Class.ToString().ToLowerInvariant() : string.Empty;
    }
}
