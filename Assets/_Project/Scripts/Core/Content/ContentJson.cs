using System.Collections.Generic;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using NaijaHustle.Core.Save;

namespace NaijaHustle.Core.Content
{
    /// <summary>JSON settings shared by content loading and save files (Unity runtime and the dotnet test runner).</summary>
    public static class ContentJson
    {
        public static readonly JsonSerializerSettings Settings = new JsonSerializerSettings
        {
            Converters = new List<JsonConverter> { new StringEnumConverter() },
            MissingMemberHandling = MissingMemberHandling.Ignore,
            NullValueHandling = NullValueHandling.Ignore,
            ObjectCreationHandling = ObjectCreationHandling.Replace,
        };

        public static ContentBundle ParseBundle(string json) => JsonConvert.DeserializeObject<ContentBundle>(json, Settings);

        public static ContentDatabase Load(IEnumerable<string> jsonFiles)
        {
            var db = new ContentDatabase();
            foreach (var json in jsonFiles) db.Merge(ParseBundle(json));
            return db;
        }

        public static string SerializeSave(SaveData data) => JsonConvert.SerializeObject(data, Formatting.None, Settings);

        public static SaveData DeserializeSave(string json) => SaveData.Migrate(JsonConvert.DeserializeObject<SaveData>(json, Settings));
    }
}
