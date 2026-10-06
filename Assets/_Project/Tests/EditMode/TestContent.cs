using System;
using System.IO;
using System.Linq;
using NaijaHustle.Core;
using NaijaHustle.Core.Content;

namespace NaijaHustle.Tests
{
    /// <summary>Loads the real shipped content JSON, both inside Unity (cwd = project root) and under dotnet test.</summary>
    internal static class TestContent
    {
        private static ContentDatabase _cached;

        public static ContentDatabase Load()
        {
            if (_cached != null) return _cached;
            var dir = FindContentDir() ?? throw new DirectoryNotFoundException("Could not locate Resources/Content");
            _cached = ContentJson.Load(Directory.GetFiles(dir, "*.json").OrderBy(f => f).Select(File.ReadAllText));
            return _cached;
        }

        public static GameSession NewSession(int seed = 1234) => new GameSession(Load(), seed);

        private static string FindContentDir()
        {
            var candidates = new[] { AppContext.BaseDirectory, Directory.GetCurrentDirectory() };
            foreach (var start in candidates)
            {
                var local = Path.Combine(start, "Content");
                if (Directory.Exists(local) && Directory.GetFiles(local, "*.json").Length > 0) return local;

                for (var d = new DirectoryInfo(start); d != null; d = d.Parent)
                {
                    var p = Path.Combine(d.FullName, "Assets", "_Project", "Resources", "Content");
                    if (Directory.Exists(p)) return p;
                }
            }
            return null;
        }
    }
}
