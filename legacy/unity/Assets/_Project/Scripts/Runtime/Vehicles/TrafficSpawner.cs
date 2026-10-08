using System.Collections.Generic;
using System.Linq;
using NaijaHustle.Core.Economy;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

namespace NaijaHustle.Runtime.Vehicles
{
    /// <summary>
    /// Pooled ambient traffic around the player. The vehicle mix comes from content
    /// (danfos and okadas in Lagos, tankers in PH, convoy SUVs in Abuja); the count scales with
    /// district density, go-slow events and the device tier.
    /// </summary>
    public sealed class TrafficSpawner : MonoBehaviour
    {
        [SerializeField] private TrafficLane[] lanes;
        [SerializeField] private int baseMaxVehicles = 18;
        [SerializeField] private float spawnRadiusMin = 60f;
        [SerializeField] private float spawnRadiusMax = 140f;
        [SerializeField] private float despawnRadius = 180f;

        private readonly List<(VehicleDefinition def, GameObject prefab)> _types = new List<(VehicleDefinition, GameObject)>();
        private readonly List<AsyncOperationHandle<GameObject>> _handles = new List<AsyncOperationHandle<GameObject>>();
        private readonly Dictionary<string, Stack<TrafficDriver>> _pool = new Dictionary<string, Stack<TrafficDriver>>();
        private readonly List<(TrafficDriver driver, string id)> _active = new List<(TrafficDriver, string)>();
        private float _totalWeight;

        private void Start()
        {
            var session = GameBootstrap.Session;
            var city = session.Story.CurrentCity;
            foreach (var def in session.Content.Data.Vehicles.Where(v => v.TrafficCities.Contains(city)))
            {
                var h = Addressables.LoadAssetAsync<GameObject>(def.AssetKey);
                _handles.Add(h);
                h.Completed += op =>
                {
                    if (op.Status != AsyncOperationStatus.Succeeded) return;
                    _types.Add((def, op.Result));
                    _totalWeight += def.TrafficWeight;
                };
            }
            InvokeRepeating(nameof(Step), 1f, 0.5f);
        }

        private void OnDestroy()
        {
            foreach (var h in _handles) if (h.IsValid()) Addressables.Release(h);
        }

        private int MaxVehicles
        {
            get
            {
                var session = GameBootstrap.Session;
                float m = Performance.DeviceProfile.TrafficScale * session.Ambient.TrafficMultiplier;
                return Mathf.RoundToInt(baseMaxVehicles * Mathf.Min(m, 2.5f));
            }
        }

        private void Step()
        {
            var player = GameBootstrap.Instance.Player;
            if (player == null || _types.Count == 0) return;

            for (int i = _active.Count - 1; i >= 0; i--)
            {
                var (d, id) = _active[i];
                if (d == null) { _active.RemoveAt(i); continue; }
                var v = d.GetComponent<Vehicle>();
                if (v.PlayerDriving) { _active.RemoveAt(i); continue; } // player owns it now
                if ((d.transform.position - player.position).sqrMagnitude > despawnRadius * despawnRadius)
                {
                    d.gameObject.SetActive(false);
                    Pool(id).Push(d);
                    _active.RemoveAt(i);
                }
            }

            if (_active.Count >= MaxVehicles) return;
            SpawnOne(player.position);
        }

        private void SpawnOne(Vector3 around)
        {
            var lane = lanes[Random.Range(0, lanes.Length)];
            int idx = Random.Range(0, lane.Count);
            Vector3 p = lane.Point(idx);
            float d = Vector3.Distance(p, around);
            if (d < spawnRadiusMin || d > spawnRadiusMax) return;

            var type = PickType();
            var stack = Pool(type.def.Id);
            TrafficDriver driver = stack.Count > 0 ? stack.Pop() : Instantiate(type.prefab).GetComponent<TrafficDriver>();
            if (driver == null) return;

            Vector3 dir = lane.Point(lane.Next(idx)) - p;
            driver.transform.SetPositionAndRotation(p + Vector3.up * 0.5f, Quaternion.LookRotation(dir));
            driver.gameObject.SetActive(true);
            driver.Begin(lane, lane.Next(idx), Random.Range(0.8f, 1.2f));
            _active.Add((driver, type.def.Id));
        }

        private (VehicleDefinition def, GameObject prefab) PickType()
        {
            float r = Random.value * _totalWeight;
            foreach (var t in _types)
            {
                r -= t.def.TrafficWeight;
                if (r <= 0f) return t;
            }
            return _types[_types.Count - 1];
        }

        private Stack<TrafficDriver> Pool(string id)
        {
            if (!_pool.TryGetValue(id, out var s)) _pool[id] = s = new Stack<TrafficDriver>();
            return s;
        }
    }
}
