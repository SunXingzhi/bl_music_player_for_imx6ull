namespace media_forwarder.Media;

/// <summary>
/// Metadata of the track SMTC is currently reporting.
/// Lyrics are not part of SMTC; they are fetched separately by LrclibClient.
/// </summary>
public sealed record SongInfo(string Title, string Artist, string Album, byte[]? CoverBytes)
{
        public static SongInfo Unknown = new("", "", "", null);
}
