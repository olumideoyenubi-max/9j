using System.Collections.Generic;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Vehicles;
using UnityEngine;
using UnityEngine.AddressableAssets;

namespace NaijaHustle.Runtime.Wanted
{
    /// <summary>
    /// Spawns the city's responders as heat rises and reports whether any of them can see the
    /// player (which pauses the star-decay search). Unit names come from the city's ResponseProfile
    /// (e.g. "Eko Task Force pickups", "Marine Patrol gunboat", "Capital Guard armored SUVs")
    /// and map to prefabs via the <see cref="unitPrefabs"/> table in each city scene.
    /// </summary>
    public sealed class ResponseDirector : MonoBehaviour
    {
        [System.Serializable]
        private struct UnitPrefab
        {
            public string unitName;
            public AssetReferenceGameObject prefab;
            public int countPerStar;
        }

        [SerializeField] private UnitPrefab[] unitPrefabs;
        [SerializeField] private VehicleInteractor playerVehicles;
        [SerializeField] private float spawnDistance = 90f;
        [SerializeField] private float sightRange = 70f;
        [SerializeField] private LayerMask occlusion = 1;
        [SerializeField] private Transform lastSeenMarker;

        private readonly List<GameObject> _units = new List<GameObject>();
        private string _currentUnit;
        private Vector3 _lastSeen;

        public static readonly List<Transform> ActiveResponders = new List<Transform>();

        private void OnEnable()
        {
            var s = GameBootstrap.Session;
            if (s != null) s.Wanted.StarsChanged += OnStars;
        }

        private void OnDisable()
        {
            var s = GameBootstrap.Session;
            if (s != null) s.Wanted.StarsChanged -= OnStars;
            Clear();
        }

        private void OnStars(int stars)
        {
            if (stars == 0) { Clear(); return; }
            bool onWater = playerVehicles != null && playerVehicles.Current != null && playerVehicles.Current.IsOnWater;
            string unit = GameBootstrap.Session.Wanted.CurrentUnit(onWater);
            if (unit == _currentUnit && _units.Count >= stars) return;
            _currentUnit = unit;
            SpawnUnits(unit, stars);
        }

        private void SpawnUnits(string unit, int stars)
        {
            var player = GameBootstrap.Instance.Player;
            if (player == null) return;
            foreach (var u in unitPrefabs)
            {
                if (u.unitName != unit) continue;
                int count = Mathf.Max(1, u.countPerStar) * stars - _units.Count;
                for (int i = 0; i < count; i++)
                {
                    Vector2 r = Random.insideUnitCircle.normalized * spawnDistance;
                    Vector3 pos = player.position + new Vector3(r.x, 2f, r.y);
                    u.prefab.InstantiateAsync(pos, Quaternion.LookRotation(player.position - pos)).Completed += op =>
                    {
                        if (op.Result == null) return;
                        _units.Add(op.Result);
                        ActiveResponders.Add(op.Result.transform);
                    };
                }
                return;
            }
            Debug.LogWarning("No prefab mapped for responder unit: " + unit);
        }

        private void Clear()
        {
            foreach (var u in _units)
            {
                if (u == null) continue;
                ActiveResponders.Remove(u.transform);
                Addressables.ReleaseInstance(u);
            }
            _units.Clear();
            _currentUnit = null;
        }

        private void Update()
        {
            var boot = GameBootstrap.Instance;
            if (boot == null || boot.Player == null) return;
            if (!GameBootstrap.Session.Wanted.IsWanted)
            {
                boot.PlayerSeenByResponders = false;
                return;
            }

            Vector3 p = boot.Player.position + Vector3.up;
            bool seen = false;
            foreach (var r in ActiveResponders)
            {
                if (r == null) continue;
                Vector3 eye = r.position + Vector3.up * 1.5f;
                if ((eye - p).sqrMagnitude > sightRange * sightRange) continue;
                if (Physics.Linecast(eye, p, occlusion, QueryTriggerInteraction.Ignore)) continue;
                seen = true;
                break;
            }

            if (seen) _lastSeen = p;
            boot.PlayerSeenByResponders = seen;
            if (lastSeenMarker != null)
            {
                // Search circle on the minimap around where they last saw you.
                lastSeenMarker.gameObject.SetActive(!seen);
                lastSeenMarker.position = _lastSeen;
                float radius = GameBootstrap.Session.Wanted.Profile.SearchRadius;
                lastSeenMarker.localScale = new Vector3(radius * 2f, 1f, radius * 2f);
            }
        }
    }
}
