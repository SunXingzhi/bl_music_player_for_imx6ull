using media_forwarder;
using media_forwarder.Media;

/* Composition root, structured after BetterLyrics:
 *
 *   SmtcWatcher    (event-driven GSMTC layer)
 *   PositionTracker(local extrapolation of the position snapshots)
 *   LrclibClient   (lyrics source, decoupled from SMTC)
 *   ConsoleView    (renderer stand-in for the board's LVGL UI)
 */
PositionTracker tracker = new();
SmtcWatcher watcher = new(tracker);
LrclibClient lrclib = new();

watcher.SongChanged += async song =>
{
        ConsoleView.OnSongChanged(song);

        /* lyrics only on track change */
        IReadOnlyList<LrcLine> lines = Array.Empty<LrcLine>();
        try {
                string? lrc = await lrclib.FetchSyncedLyricsAsync(song, DurationHintMs());
                lines = LrcParser.Parse(lrc);
        }
        catch(Exception) { /* offline / no match: empty lyrics view */ }

        ConsoleView.OnLyrics(lines);
};
watcher.PlaybackChanged += ConsoleView.OnPlaybackChanged;

await watcher.StartAsync(CancellationToken.None);

Console.CursorVisible = false;
Console.WriteLine("正在初始化... 请打开任意 SMTC 播放器（Spotify/网易云/浏览器）播放音乐");

/* UI tick: pull the locally extrapolated position, redraw the view */
while(true) {
        ConsoleView.Render(tracker);
        await Task.Delay(250);
}

/* duration of the current timeline, for LRCLIB's precise /api/get lookup */
int? DurationHintMs() => tracker.TotalMs > 0 ? tracker.TotalMs / 1000 : null;
