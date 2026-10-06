namespace NaijaHustle.Core.World
{
    /// <summary>The three playable regions. Each is a separately streamed Addressables scene group.</summary>
    public enum CityId
    {
        Lagos = 0,
        PortHarcourt = 1,
        Abuja = 2,
    }

    public static class CityIdExtensions
    {
        public static string DisplayName(this CityId city)
        {
            switch (city)
            {
                case CityId.Lagos: return "Lagos";
                case CityId.PortHarcourt: return "Port Harcourt";
                case CityId.Abuja: return "Abuja";
                default: return city.ToString();
            }
        }

        public static string Nickname(this CityId city)
        {
            switch (city)
            {
                case CityId.Lagos: return "The Hustle";
                case CityId.PortHarcourt: return "Garden City";
                case CityId.Abuja: return "The Capital";
                default: return string.Empty;
            }
        }
    }
}
