using System.Collections.Generic;
using NaijaHustle.Core.Missions;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Vehicles;
using UnityEngine;

namespace NaijaHustle.Runtime.Hustles
{
    /// <summary>
    /// Lagos danfo conductor side hustle (also used for the PH boat taxi and Abuja VIP chauffeur with
    /// different stop lists). Drive the route, stop at each bus stop, passengers board/alight; each
    /// leg is one job whose quality comes from smooth driving and punctuality.
    /// </summary>
    public sealed class DanfoConductorHustle : MonoBehaviour
    {
        [SerializeField] private string hustleId = "hus_lag_conductor";
        [SerializeField] private VehicleInteractor playerVehicles;
        [SerializeField] private List<Transform> stops = new List<Transform>();
        [SerializeField] private float stopRadius = 6f;
        [SerializeField] private float stopMaxKph = 8f;
        [SerializeField] private float targetLegSeconds = 60f;
        [Tooltip("Sudden speed changes above this (km/h per second) count as a rough ride.")]
        [SerializeField] private float harshAccel = 25f;
        [SerializeField] private string[] conductorCalls =
        {
            "Oshoja! Oshoja! Enter with your change!",
            "Eko Crest, straight! No stopping!",
            "Owa! Owa! Hold your bag well!",
            "Driver, wait! One chance remain!",
        };

        private int _nextStop;
        private float _legTimer;
        private float _roughness;
        private float _lastKph;

        public bool OnShift => GameBootstrap.Session?.ActiveShift?.Definition.Id == hustleId;
        public string CurrentCall { get; private set; }
        public Transform NextStop => OnShift && stops.Count > 0 ? stops[_nextStop] : null;

        /// <summary>Hooked to the "Start shift" prompt shown when the player sits in a matching vehicle.</summary>
        public bool StartShift()
        {
            var v = playerVehicles.Current;
            var def = GameBootstrap.Session.Content.Data.Hustles.Find(h => h.Id == hustleId);
            if (v == null || def == null) return false;
            if (!string.IsNullOrEmpty(def.RequiredVehicle) && v.ClassKey != def.RequiredVehicle && v.VehicleId != def.RequiredVehicle) return false;
            if (GameBootstrap.Session.StartShift(hustleId) == null) return false;

            _nextStop = ClosestStop(v.transform.position);
            ResetLeg();
            return true;
        }

        public void EndShift() => GameBootstrap.Session.EndShift();

        private void Update()
        {
            if (!OnShift) return;
            var v = playerVehicles.Current;
            if (v == null) { EndShift(); return; } // left the bus = end of shift

            float kph = Mathf.Abs(v.SpeedKph);
            float accel = Mathf.Abs(kph - _lastKph) / Mathf.Max(Time.deltaTime, 0.0001f);
            if (accel > harshAccel) _roughness += Time.deltaTime;
            _lastKph = kph;
            _legTimer += Time.deltaTime;

            var stop = stops[_nextStop];
            if ((v.transform.position - stop.position).sqrMagnitude < stopRadius * stopRadius && kph < stopMaxKph)
            {
                float punctual = Mathf.Clamp01(1.5f - _legTimer / targetLegSeconds);
                float smooth = Mathf.Clamp01(1f - _roughness / 5f);
                GameBootstrap.Session.CompleteShiftJob(punctual * 0.5f + smooth * 0.5f);
                _nextStop = (_nextStop + 1) % stops.Count;
                ResetLeg();
            }
        }

        private void ResetLeg()
        {
            _legTimer = 0f;
            _roughness = 0f;
            CurrentCall = conductorCalls[Random.Range(0, conductorCalls.Length)];
        }

        private int ClosestStop(Vector3 p)
        {
            int best = 0;
            float bestD = float.MaxValue;
            for (int i = 0; i < stops.Count; i++)
            {
                float d = (stops[i].position - p).sqrMagnitude;
                if (d < bestD) { bestD = d; best = i; }
            }
            return (best + 1) % stops.Count; // head to the next one, not the one you're sitting at
        }
    }
}
