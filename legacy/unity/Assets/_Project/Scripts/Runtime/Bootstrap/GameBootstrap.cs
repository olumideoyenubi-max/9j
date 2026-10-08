using System;
using System.Linq;
using NaijaHustle.Core;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.Save;
using NaijaHustle.Runtime.World;
using UnityEngine;

namespace NaijaHustle.Runtime.Bootstrap
{
    /// <summary>
    /// Lives in the tiny Boot scene (the only scene in the base install). Loads content JSON,
    /// creates the <see cref="GameSession"/>, restores the save and asks <see cref="CityStreamer"/>
    /// to bring in the current city.
    /// </summary>
    [DefaultExecutionOrder(-1000)]
    public sealed class GameBootstrap : MonoBehaviour
    {
        public static GameBootstrap Instance { get; private set; }
        public static GameSession Session => Instance != null ? Instance._session : null;

        [SerializeField] private CityStreamer cityStreamer;
        [SerializeField] private int saveSlot = 0;
        [SerializeField] private float autosaveIntervalSeconds = 120f;

        private GameSession _session;
        private ISaveStore _saves;
        private float _autosaveTimer;

        public ISaveStore Saves => _saves;
        public int SaveSlot => saveSlot;

        /// <summary>Set by the player rig each frame; used for autosave position.</summary>
        public Transform Player { get; set; }

        /// <summary>Set by <c>ResponseDirector</c> each frame.</summary>
        public bool PlayerSeenByResponders { get; set; }

        /// <summary>Set by <c>DistrictZone</c>s.</summary>
        public bool PlayerInSafeZone { get; set; }

        public event Action<GameSession> SessionReady;

        private void Awake()
        {
            if (Instance != null) { Destroy(gameObject); return; }
            Instance = this;
            DontDestroyOnLoad(gameObject);

            Performance.DeviceProfile.Apply();

            var files = Resources.LoadAll<TextAsset>("Content");
            var content = ContentJson.Load(files.OrderBy(f => f.name).Select(f => f.text));
#if UNITY_EDITOR || DEVELOPMENT_BUILD
            foreach (var err in content.Validate()) Debug.LogError("[Content] " + err);
#endif
            _session = new GameSession(content, Environment.TickCount);
            _saves = new FileSaveStore();

            if (_saves.Exists(saveSlot))
            {
                try { _session.LoadSave(_saves.Read(saveSlot)); }
                catch (Exception e) { Debug.LogException(e); }
            }
        }

        private void Start()
        {
            SessionReady?.Invoke(_session);
            cityStreamer.EnterCity(_session.Story.CurrentCity, null);
        }

        private void Update()
        {
            _session.Tick(Time.deltaTime, PlayerSeenByResponders, PlayerInSafeZone);

            _autosaveTimer += Time.unscaledDeltaTime;
            if (_autosaveTimer >= autosaveIntervalSeconds && !_session.Wanted.IsWanted && _session.Missions.Active == null)
            {
                _autosaveTimer = 0f;
                Save();
            }
        }

        public void Save()
        {
            var p = Player != null ? Player.position : Vector3.zero;
            _saves.Write(saveSlot, _session.CreateSave(new[] { p.x, p.y, p.z }));
        }

        private void OnApplicationPause(bool paused)
        {
            // Android kills backgrounded apps aggressively on 3GB phones: always save on pause.
            if (paused && _session != null) Save();
        }
    }
}
