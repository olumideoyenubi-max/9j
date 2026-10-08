using NaijaHustle.Core.World;
using NaijaHustle.Runtime.Bootstrap;
using UnityEngine;

namespace NaijaHustle.Runtime.World
{
    /// <summary>
    /// Per-city sky, fog and weather effects. Harmattan = warm dusty fog that shortens draw distance
    /// (also a free perf win); Port Harcourt "soot" = dark particulate + a grime overlay on screen.
    /// Lives in each city scene so its prefabs stream with the city.
    /// </summary>
    public sealed class AtmosphereController : MonoBehaviour
    {
        [System.Serializable]
        private struct Look
        {
            public WeatherKind kind;
            public Color fogColor;
            public float fogDensity;
            public float sunIntensity;
            public GameObject effectRoot; // rain particles, dust motes, soot flakes
            [Range(0, 1)] public float screenGrime;
        }

        [SerializeField] private Light sun;
        [SerializeField] private Gradient sunColorOverDay;
        [SerializeField] private Look[] looks;
        [SerializeField] private CanvasGroup grimeOverlay;
        [SerializeField] private float blendSeconds = 8f;

        private Look _target;
        private float _blend = 1f;
        private Color _fromFog;
        private float _fromDensity;
        private float _fromSun;
        private float _fromGrime;

        private void OnEnable()
        {
            RenderSettings.fog = true;
            RenderSettings.fogMode = FogMode.ExponentialSquared;
            var session = GameBootstrap.Session;
            if (session == null) return;
            session.Weather.Changed += Apply;
            Apply(session.Weather.Current);
            _blend = 1f;
        }

        private void OnDisable()
        {
            var session = GameBootstrap.Session;
            if (session != null) session.Weather.Changed -= Apply;
        }

        private void Apply(WeatherKind kind)
        {
            _fromFog = RenderSettings.fogColor;
            _fromDensity = RenderSettings.fogDensity;
            _fromSun = sun != null ? sun.intensity : 1f;
            _fromGrime = grimeOverlay != null ? grimeOverlay.alpha : 0f;
            _blend = 0f;

            foreach (var l in looks)
            {
                if (l.effectRoot != null) l.effectRoot.SetActive(l.kind == kind);
                if (l.kind == kind) _target = l;
            }
        }

        private void Update()
        {
            var session = GameBootstrap.Session;
            if (session == null) return;

            if (_blend < 1f)
            {
                _blend = Mathf.Min(1f, _blend + Time.deltaTime / blendSeconds);
                RenderSettings.fogColor = Color.Lerp(_fromFog, _target.fogColor, _blend);
                RenderSettings.fogDensity = Mathf.Lerp(_fromDensity, _target.fogDensity, _blend);
                if (grimeOverlay != null) grimeOverlay.alpha = Mathf.Lerp(_fromGrime, _target.screenGrime, _blend);
            }

            if (sun != null)
            {
                float t = session.Clock.DayFraction;
                sun.transform.rotation = Quaternion.Euler(t * 360f - 90f, 170f, 0f);
                float daylight = Mathf.Clamp01(Mathf.Sin(t * Mathf.PI * 2f - Mathf.PI / 2f) * 0.5f + 0.5f);
                sun.intensity = Mathf.Lerp(_fromSun, _target.sunIntensity, _blend) * daylight;
                if (sunColorOverDay != null) sun.color = sunColorOverDay.Evaluate(t);
            }
        }
    }
}
