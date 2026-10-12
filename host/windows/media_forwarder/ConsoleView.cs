using media_forwarder.Media;

namespace media_forwarder;

/// <summary>
/// Console "view" of the demo: renders metadata, extrapolated position and
/// the active lyric line — a stand-in for the LVGL UI on the IMX6ULL board.
/// </summary>
public static class ConsoleView
{
        private static SongInfo _song = SongInfo.Unknown;
        private static bool _playing;
        private static IReadOnlyList<LrcLine> _lines = Array.Empty<LrcLine>();
        private static int _lastLine = -2;

        public static void OnSongChanged(SongInfo song) { _song = song; _lastLine = -2; }
        public static void OnPlaybackChanged(bool playing) => _playing = playing;
        public static void OnLyrics(IReadOnlyList<LrcLine> lines) { _lines = lines; _lastLine = -2; }

        public static void Render(PositionTracker tracker)
        {
                Console.SetCursorPosition(0, 0);

                PrintPad($"标题 : {_song.Title}");
                PrintPad($"歌手 : {_song.Artist}");
                PrintPad($"专辑 : {_song.Album}");
                PrintPad($"状态 : {(_playing ? "playing" : "paused")}");
                PrintPad($"封面 : {(_song.CoverBytes is { Length: > 0 } b ? $"{b.Length} bytes" : "-")}");
                PrintPad(new string('─', 50));

                int posMs = tracker.PositionMs;
                PrintPad($"进度 : {TimeSpan.FromMilliseconds(posMs):mm\\:ss} / {TimeSpan.FromMilliseconds(tracker.TotalMs):mm\\:ss}");
                PrintPad($"歌词 : {(_lines.Count == 0 ? "(no synced lyrics)" : $"{_lines.Count} lines")}     ");
                PrintPad("");

                int cur = LrcParser.FindCurrentLine(_lines, posMs);
                if(cur != _lastLine) {
                        _lastLine = cur;
                        PrintPad("─ Now playing line ─");
                        PrintPad(cur >= 0 ? _lines[cur].Text.PadRight(50) : "".PadRight(50));
                }
        }

        /* overwrite the previous line fully, so shorter text doesn't leave residue */
        private static void PrintPad(string s) => Console.WriteLine(s.PadRight(Console.WindowWidth - 1 > 0 ? Console.WindowWidth - 1 : s.Length));
}
