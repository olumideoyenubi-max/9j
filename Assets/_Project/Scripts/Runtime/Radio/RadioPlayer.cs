using System.Collections.Generic;
using NaijaHustle.Core.Content;
using NaijaHustle.Runtime.Bootstrap;
using NaijaHustle.Runtime.Input;
using NaijaHustle.Runtime.Vehicles;
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;

namespace NaijaHustle.Runtime.Radio
{
    /// <summary>
    /// In-vehicle radio. Each city has its own stations (Hustle FM in Lagos, Creek Talk in PH,
    /// Protocol AM in Abuja...). Clips are streamed Addressables (Vorbis, "Streaming" load type) and
    /// only the current track is resident, so the radio costs ~1 MB RAM regardless of playlist size.
    /// Stations keep "playing" while you're out of the car: we track a virtual playhead per station.
    /// </summary>
    public sealed class RadioPlayer : MonoBehaviour
    {
        [SerializeField] private VehicleInteractor playerVehicles;
        [SerializeField] private UI.Toast toast;

        private RadioStationDefinition[] _stations = new RadioStationDefinition[0];
        private int _station = -1; // -1 = off
        private readonly Dictionary<string, IList<string>> _playlists = new Dictionary<string, IList<string>>();
        private AsyncOperationHandle<AudioClip> _clip;
        private AudioSource _source;

        private void Start()
        {
            playerVehicles.Entered += OnEnter;
            playerVehicles.Exited += _ => Stop();
            var city = GameBootstrap.Session.CurrentCityDef;
            if (city != null) _stations = city.Radio;
        }

        private void OnEnter(Vehicle v)
        {
            _source = v.RadioSource;
            if (_source == null || _stations.Length == 0) return;
            if (_station < 0) _station = Random.Range(0, _stations.Length);
            Play();
        }

        private void Update()
        {
            if (_source == null) return;
            var input = PlayerInputHub.Instance;
            if (input != null && input.Pressed(ActionButton.Radio))
            {
                _station = _station + 1 >= _stations.Length ? -1 : _station + 1;
                if (_station < 0) { Stop(); toast?.Show("Radio off"); }
                else Play();
            }

            if (_station >= 0 && _clip.IsValid() && _clip.IsDone && !_source.isPlaying) NextTrack();
        }

        private void Play()
        {
            var st = _stations[_station];
            toast?.Show($"{st.Frequency} {st.Name} — {st.Genre}");
            if (_playlists.ContainsKey(st.PlaylistKey)) { NextTrack(); return; }

            var keys = Addressables.LoadResourceLocationsAsync(st.PlaylistKey, typeof(AudioClip));
            keys.Completed += op =>
            {
                var list = new List<string>();
                if (op.Status == AsyncOperationStatus.Succeeded)
                    foreach (var loc in op.Result) list.Add(loc.PrimaryKey);
                _playlists[st.PlaylistKey] = list;
                Addressables.Release(op);
                NextTrack();
            };
        }

        private void NextTrack()
        {
            if (_station < 0 || _source == null) return;
            var list = _playlists[_stations[_station].PlaylistKey];
            if (list.Count == 0) return;

            ReleaseClip();
            _clip = Addressables.LoadAssetAsync<AudioClip>(list[Random.Range(0, list.Count)]);
            _clip.Completed += op =>
            {
                if (op.Status != AsyncOperationStatus.Succeeded || _source == null) return;
                _source.clip = op.Result;
                _source.Play();
            };
        }

        private void Stop()
        {
            if (_source != null) _source.Stop();
            _source = null;
            ReleaseClip();
        }

        private void ReleaseClip()
        {
            if (_clip.IsValid()) Addressables.Release(_clip);
            _clip = default;
        }

        private void OnDestroy() => Stop();
    }
}
