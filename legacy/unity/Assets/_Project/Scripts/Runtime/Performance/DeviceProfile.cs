using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

namespace NaijaHustle.Runtime.Performance
{
    public enum DeviceTier
    {
        Low,    // ≤3 GB RAM, Mali-G52/Adreno 610 class — the target minimum
        Mid,    // 4–6 GB
        High,   // 8 GB+ phones and PC
    }

    /// <summary>
    /// Picks a quality tier at boot from RAM, GPU and platform, then sets render scale, frame rate,
    /// shadow/LOD/draw distances and population density. Player can override in settings.
    /// </summary>
    public static class DeviceProfile
    {
        public static DeviceTier Tier { get; private set; } = DeviceTier.Mid;
        public static float TrafficScale { get; private set; } = 1f;
        public static float PedestrianScale { get; private set; } = 1f;
        public static float DrawDistance { get; private set; } = 600f;

        private const string OverrideKey = "nh.tier_override";

        public static DeviceTier Detect()
        {
            if (!Application.isMobilePlatform) return DeviceTier.High;
            int ramMb = SystemInfo.systemMemorySize;
            int vramMb = SystemInfo.graphicsMemorySize;
            if (ramMb <= 3200 || vramMb < 1024) return DeviceTier.Low;
            if (ramMb <= 6500) return DeviceTier.Mid;
            return DeviceTier.High;
        }

        public static void Apply()
        {
            int ov = PlayerPrefs.GetInt(OverrideKey, -1);
            Apply(ov >= 0 ? (DeviceTier)ov : Detect());
        }

        public static void SetOverride(DeviceTier tier)
        {
            PlayerPrefs.SetInt(OverrideKey, (int)tier);
            Apply(tier);
        }

        public static void Apply(DeviceTier tier)
        {
            Tier = tier;
            // Quality levels Low / Mid / High are expected in Project Settings > Quality, in that order.
            int level = Mathf.Min((int)tier, QualitySettings.names.Length - 1);
            QualitySettings.SetQualityLevel(level, applyExpensiveChanges: true);

            var urp = GraphicsSettings.currentRenderPipeline as UniversalRenderPipelineAsset;
            switch (tier)
            {
                case DeviceTier.Low:
                    Application.targetFrameRate = 30;
                    if (urp != null) { urp.renderScale = 0.7f; urp.shadowDistance = 25f; urp.msaaSampleCount = 1; }
                    QualitySettings.lodBias = 0.6f;
                    TrafficScale = 0.5f; PedestrianScale = 0.4f; DrawDistance = 300f;
                    break;
                case DeviceTier.Mid:
                    Application.targetFrameRate = 30;
                    if (urp != null) { urp.renderScale = 0.85f; urp.shadowDistance = 45f; urp.msaaSampleCount = 1; }
                    QualitySettings.lodBias = 1f;
                    TrafficScale = 0.8f; PedestrianScale = 0.75f; DrawDistance = 450f;
                    break;
                default:
                    Application.targetFrameRate = Application.isMobilePlatform ? 60 : -1;
                    if (urp != null) { urp.renderScale = 1f; urp.shadowDistance = 80f; urp.msaaSampleCount = 2; }
                    QualitySettings.lodBias = 1.5f;
                    TrafficScale = 1f; PedestrianScale = 1f; DrawDistance = 800f;
                    break;
            }

            if (Camera.main != null) Camera.main.farClipPlane = DrawDistance;
        }
    }
}
