using System.Net.Http;
using System.Text.Json;

namespace media_forwarder.Media;

/// <summary>
/// Free, no-auth lyrics provider: https://lrclib.net
/// Lookup strategy (strictest first, same idea as BetterLyrics' SearchSmartly):
///   1. /api/get     exact match on title+artist+album+duration
///   2. /api/search  title + artist, pick entry with synced lyrics
///   3. /api/search  q="title artist" full-text, pick nearest duration
/// Every step is logged so failures are diagnosable from the console.
/// </summary>
public sealed class LrclibClient
{
        private readonly HttpClient _http = new()
        {
                /* LRCLIB asks clients to identify themselves */
                DefaultRequestHeaders = { { "User-Agent", "bl_music_player-demo (https://github.com)" } },
                Timeout = TimeSpan.FromSeconds(20),
        };

        public async Task<string?> FetchSyncedLyricsAsync(SongInfo song, int? durationSec,
                                                          CancellationToken ct = default)
        {
                if(string.IsNullOrEmpty(song.Title)) return null;

                Console.WriteLine($"[LRCLIB] 查询: '{song.Title}' - '{song.Artist}' - '{song.Album}' (dur {durationSec}s)");
                string? lrc;

                if(durationSec is not null) {
                        lrc = await TryGetAsync(
                            "https://lrclib.net/api/get?" +
                            $"track_name={Uri.EscapeDataString(song.Title)}" +
                            $"&artist_name={Uri.EscapeDataString(song.Artist)}" +
                            $"&album_name={Uri.EscapeDataString(song.Album)}" +
                            $"&duration={durationSec}", ct);
                        if(lrc != null) goto found;
                }

                lrc = await SearchAsync(
                    "https://lrclib.net/api/search?" +
                    $"track_name={Uri.EscapeDataString(song.Title)}" +
                    $"&artist_name={Uri.EscapeDataString(song.Artist)}",
                    durationSec, ct);
                if(lrc != null) goto found;

                lrc = await SearchAsync(
                    "https://lrclib.net/api/search?q=" +
                    Uri.EscapeDataString($"{song.Title} {song.Artist}"), durationSec, ct);
                if(lrc != null) goto found;

                Console.WriteLine("[LRCLIB] 三种查询均未命中(可能该歌曲未收录, 或只有纯文本歌词)");
                return null;

        found:
                Console.WriteLine($"[LRCLIB] 命中, 歌词 {lrc!.Length} chars");
                return lrc;
        }

        private async Task<string?> TryGetAsync(string url, CancellationToken ct)
        {
                try {
                        using JsonDocument doc =
                            JsonDocument.Parse(await _http.GetStringAsync(url, ct));
                        return doc.RootElement.TryGetProperty("syncedLyrics", out var lrc)
                            && lrc.ValueKind == JsonValueKind.String
                            ? lrc.GetString()
                            : null;
                }
                catch(HttpRequestException ex) {
                        Console.WriteLine($"[LRCLIB] /api/get 请求失败: {ex.StatusCode?.ToString() ?? ex.Message}");
                        return null;
                }
                catch(TaskCanceledException) when(!ct.IsCancellationRequested) {
                        /* HttpClient.Timeout surfaces as TaskCanceledException */
                        Console.WriteLine("[LRCLIB] /api/get 超时(20s): 网络不可达或需要代理");
                        return null;
                }
                catch(JsonException) { return null; }
        }

        private async Task<string?> SearchAsync(string url, int? durationSec, CancellationToken ct)
        {
                try {
                        string json = await _http.GetStringAsync(url, ct);
                        using JsonDocument doc = JsonDocument.Parse(json);

                        /* prefer an entry with synced lyrics whose duration is
                         * close to the playing track (LRCLIB returns many fuzzy
                         * matches; the first hit is often a cover/same-name song) */
                        string? best = null;
                        int bestDelta = int.MaxValue;

                        foreach(var item in doc.RootElement.EnumerateArray()) {
                                string title   = item.TryGetProperty("trackName", out var tn) ? tn.GetString()   ?? "?" : "?";
                                string artist  = item.TryGetProperty("artistName", out var an) ? an.GetString() ?? "?" : "?";
                                int dur        = item.TryGetProperty("duration", out var d) ? d.GetInt32()      : -1;
                                bool hasSync   = item.TryGetProperty("syncedLyrics", out var sl)
                                                 && sl.ValueKind == JsonValueKind.String
                                                 && !string.IsNullOrEmpty(sl.GetString());
                                Console.WriteLine($"[LRCLIB]   候选: {title} - {artist} ({dur}s, synced: {(hasSync ? "yes" : "no")})");

                                if(!hasSync) continue;

                                if(durationSec is not null && dur >= 0) {
                                        int delta = Math.Abs(dur - durationSec.Value);
                                        if(delta < bestDelta) {
                                                bestDelta = delta;
                                                string? s = sl.GetString();
                                                if(s != null) best = s;
                                        }
                                }
                                else if(best == null) {
                                        best = sl.GetString();
                                }
                        }

                        if(best == null)
                                Console.WriteLine("[LRCLIB] search 返回条目但都无同步歌词");
                        return best;
                }
                catch(HttpRequestException ex) {
                        Console.WriteLine($"[LRCLIB] search 请求失败: {ex.StatusCode?.ToString() ?? ex.Message}");
                        return null;
                }
                catch(TaskCanceledException) when(!ct.IsCancellationRequested) {
                        Console.WriteLine("[LRCLIB] search 超时(20s): 网络不可达或需要代理");
                        return null;
                }
                catch(JsonException) { return null; }
        }
}
