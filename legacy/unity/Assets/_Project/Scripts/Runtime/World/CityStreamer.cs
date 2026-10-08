using System;
using System.Collections;
using NaijaHustle.Core.World;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;
using UnityEngine.AddressableAssets;
using UnityEngine.ResourceManagement.AsyncOperations;
using UnityEngine.ResourceManagement.ResourceProviders;
using UnityEngine.SceneManagement;

namespace NaijaHustle.Runtime.World
{
    /// <summary>
    /// One city in memory at a time. Lagos ships in the base install; Port Harcourt and Abuja are
    /// remote Addressables groups downloaded on unlock (see docs/TECH.md). Switching cities fully
    /// unloads the old scene and its bundles before loading the next — essential on 3GB devices.
    /// </summary>
    public sealed class CityStreamer : MonoBehaviour
    {
        [SerializeField] private LoadingScreen loadingScreen;

        private AsyncOperationHandle<SceneInstance> _currentScene;
        private bool _busy;

        public CityId? LoadedCity { get; private set; }
        public bool IsBusy => _busy;

        /// <summary>(city, 0..1 progress, status text)</summary>
        public event Action<CityId, float, string> Progress;
        public event Action<CityId> CityLoaded;

        /// <summary>Bytes still to download for a city (0 if cached / bundled). Use to warn on mobile data.</summary>
        public IEnumerator GetDownloadSize(CityId city, Action<long> result)
        {
            var def = GameBootstrap.Session.Content.City(city);
            var handle = Addressables.GetDownloadSizeAsync(def.DownloadLabel);
            yield return handle;
            result(handle.Status == AsyncOperationStatus.Succeeded ? handle.Result : -1);
            Addressables.Release(handle);
        }

        /// <param name="interlude">Optional coroutine to run while loading (bus ride cutscene, flight).</param>
        public void EnterCity(CityId city, Func<IEnumerator> interlude)
        {
            if (_busy) return;
            StartCoroutine(SwitchRoutine(city, interlude));
        }

        private IEnumerator SwitchRoutine(CityId city, Func<IEnumerator> interlude)
        {
            _busy = true;
            var session = GameBootstrap.Session;
            var def = session.Content.City(city);
            if (loadingScreen != null) loadingScreen.Show(def.Id.DisplayName(), def.Tagline);

            // 1. Unload the current city first so peak memory is one city, not two.
            if (_currentScene.IsValid())
            {
                Report(city, 0f, "Leaving " + LoadedCity?.DisplayName());
                var unload = Addressables.UnloadSceneAsync(_currentScene);
                yield return unload;
                _currentScene = default;
                LoadedCity = null;
                yield return Resources.UnloadUnusedAssets();
                GC.Collect();
            }

            // 2. Download the city's remote bundle if needed.
            var sizeHandle = Addressables.GetDownloadSizeAsync(def.DownloadLabel);
            yield return sizeHandle;
            long bytes = sizeHandle.Status == AsyncOperationStatus.Succeeded ? sizeHandle.Result : 0;
            Addressables.Release(sizeHandle);

            if (bytes > 0)
            {
                var dl = Addressables.DownloadDependenciesAsync(def.DownloadLabel, false);
                while (!dl.IsDone)
                {
                    var status = dl.GetDownloadStatus();
                    Report(city, 0.6f * status.Percent, $"Downloading {city.DisplayName()} ({status.DownloadedBytes / 1048576}/{status.TotalBytes / 1048576} MB)");
                    yield return null;
                }
                bool ok = dl.Status == AsyncOperationStatus.Succeeded;
                Addressables.Release(dl);
                if (!ok)
                {
                    Report(city, 0f, "Download failed. Check your connection and try again.");
                    _busy = false;
                    yield break;
                }
            }

            // 3. Interlude (bus ride with mini-events, flight) plays while the scene loads.
            var load = Addressables.LoadSceneAsync(def.SceneKey, LoadSceneMode.Additive, activateOnLoad: false);
            if (interlude != null) yield return StartCoroutine(interlude());
            while (!load.IsDone && load.PercentComplete < 0.9f)
            {
                Report(city, 0.6f + 0.35f * load.PercentComplete, "Loading " + city.DisplayName());
                yield return null;
            }
            yield return load;
            if (load.Status != AsyncOperationStatus.Succeeded)
            {
                Report(city, 0f, "Could not load " + city.DisplayName());
                _busy = false;
                yield break;
            }

            yield return load.Result.ActivateAsync();
            SceneManager.SetActiveScene(load.Result.Scene);
            _currentScene = load;
            LoadedCity = city;

            if (session.Story.CurrentCity != city) session.EnterCity(city);
            Report(city, 1f, string.Empty);
            if (loadingScreen != null) loadingScreen.Hide();
            _busy = false;
            CityLoaded?.Invoke(city);
        }

        private void Report(CityId city, float p, string text)
        {
            Progress?.Invoke(city, p, text);
            if (loadingScreen != null) loadingScreen.SetProgress(p, text);
        }
    }
}
