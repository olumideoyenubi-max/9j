using System.IO;
using NaijaHustle.Core.Content;
using NaijaHustle.Core.Save;
using UnityEngine;

namespace NaijaHustle.Runtime.Bootstrap
{
    /// <summary>JSON saves in persistentDataPath, written atomically (temp file + replace) so a kill mid-write can't corrupt.</summary>
    public sealed class FileSaveStore : ISaveStore
    {
        private static string PathFor(int slot) => Path.Combine(Application.persistentDataPath, $"save_{slot}.json");

        public bool Exists(int slot) => File.Exists(PathFor(slot));

        public void Write(int slot, SaveData data)
        {
            string path = PathFor(slot);
            string tmp = path + ".tmp";
            File.WriteAllText(tmp, ContentJson.SerializeSave(data));
            // File.Replace isn't reliable on all Android storage; copy-to-backup then move is.
            if (File.Exists(path))
            {
                File.Copy(path, path + ".bak", overwrite: true);
                File.Delete(path);
            }
            File.Move(tmp, path);
        }

        public SaveData Read(int slot)
        {
            string path = PathFor(slot);
            try
            {
                return ContentJson.DeserializeSave(File.ReadAllText(path));
            }
            catch (System.Exception e) when (File.Exists(path + ".bak"))
            {
                Debug.LogWarning("Save corrupt, falling back to backup: " + e.Message);
                return ContentJson.DeserializeSave(File.ReadAllText(path + ".bak"));
            }
        }

        public void Delete(int slot)
        {
            if (Exists(slot)) File.Delete(PathFor(slot));
        }
    }
}
