using System.Linq;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.World;
using UnityEditor;
using UnityEditor.AddressableAssets;
using UnityEditor.AddressableAssets.Settings;
using UnityEditor.AddressableAssets.Settings.GroupSchemas;
using UnityEngine;

namespace NaijaHustle.Editor
{
    /// <summary>Editor helpers under the "Naija Hustle" menu.</summary>
    public static class NaijaHustleMenu
    {
        [MenuItem("Naija Hustle/Validate Content")]
        public static void ValidateContent()
        {
            var files = Resources.LoadAll<TextAsset>("Content");
            var db = ContentJson.Load(files.OrderBy(f => f.name).Select(f => f.text));
            var errors = db.Validate();
            foreach (var e in errors) Debug.LogError("[Content] " + e);

            var d = db.Data;
            Debug.Log($"[Content] {(errors.Count == 0 ? "OK" : errors.Count + " error(s)")} — " +
                      $"{d.Missions.Count} missions, {d.Hustles.Count} hustles, {d.Vehicles.Count} vehicles, " +
                      $"{d.Outfits.Count} outfits, {d.Businesses.Count} businesses, {d.Contacts.Count} contacts.");
        }

        /// <summary>
        /// Creates one Addressables group per city. Lagos is local (ships in the base APK/AAB);
        /// Port Harcourt and Abuja are remote and downloaded on unlock (see docs/TECH.md).
        /// Drop each city's scene and assets into its group afterwards.
        /// </summary>
        [MenuItem("Naija Hustle/Setup Addressables Groups")]
        public static void SetupGroups()
        {
            var settings = AddressableAssetSettingsDefaultObject.GetSettings(true);
            Make(settings, "City_Lagos", "city-lagos", remote: false);
            Make(settings, "City_PortHarcourt", "city-portharcourt", remote: true);
            Make(settings, "City_Abuja", "city-abuja", remote: true);
            Make(settings, "Shared_Vehicles", "shared-vehicles", remote: false);
            AssetDatabase.SaveAssets();
            Debug.Log("Addressables groups ready. Assign city scenes to their groups and label them.");
        }

        private static void Make(AddressableAssetSettings settings, string name, string label, bool remote)
        {
            var group = settings.FindGroup(name) ?? settings.CreateGroup(name, false, false, true, null,
                typeof(BundledAssetGroupSchema), typeof(ContentUpdateGroupSchema));
            settings.AddLabel(label);

            var bundle = group.GetSchema<BundledAssetGroupSchema>();
            bundle.BundleMode = BundledAssetGroupSchema.BundlePackingMode.PackSeparately;
            bundle.Compression = BundledAssetGroupSchema.BundleCompressionMode.LZ4;
            bundle.BuildPath.SetVariableByName(settings, remote ? AddressableAssetSettings.kRemoteBuildPath : AddressableAssetSettings.kLocalBuildPath);
            bundle.LoadPath.SetVariableByName(settings, remote ? AddressableAssetSettings.kRemoteLoadPath : AddressableAssetSettings.kLocalLoadPath);

            group.GetSchema<ContentUpdateGroupSchema>().StaticContent = !remote;
        }

        [MenuItem("Naija Hustle/Unlock All Cities (Play Mode)")]
        public static void UnlockAll()
        {
            var s = Runtime.Bootstrap.GameBootstrap.Session;
            if (s == null) { Debug.LogWarning("Enter Play Mode first."); return; }
            s.Story.UnlockCity(CityId.PortHarcourt);
            s.Story.UnlockCity(CityId.Abuja);
            s.Story.SetFlag("flight_unlocked");
            s.Wallet.Earn(10_000_000, "debug");
        }
    }
}
