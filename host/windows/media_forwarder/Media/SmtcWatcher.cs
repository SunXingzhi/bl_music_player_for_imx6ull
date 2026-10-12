using Windows.Media.Control;
using Windows.Storage.Streams;

namespace media_forwarder.Media;

/// <summary>
/// Watches all SMTC sessions and surfaces changes as events,
/// mirroring BetterLyrics' GSMTCService:
///
///   MediaPropertiesChanged  -> track switch  -> SongChanged
///   PlaybackInfoChanged     -> play / pause  -> PlaybackChanged
///   TimelinePropertiesChanged / any change  -> position resync
/// </summary>
public sealed class SmtcWatcher
{
        private readonly PositionTracker _tracker;

        public event Action<SongInfo>?                   SongChanged;
        public event Action<bool>?                       PlaybackChanged;

        public SmtcWatcher(PositionTracker tracker) { _tracker = tracker; }

        public async Task StartAsync(CancellationToken ct)
        {
                var manager = await GlobalSystemMediaTransportControlsSessionManager
                    .RequestAsync()
                    .AsTask(ct);

                /* sessions come and go (open/close a player); resubscribe each time */
                manager.SessionsChanged += async (_, _) => await AttachSessionsAsync(manager);

                /* existing sessions were already open before we started */
                await AttachSessionsAsync(manager);
        }

        private async Task AttachSessionsAsync(
            GlobalSystemMediaTransportControlsSessionManager manager)
        {
                var session = manager.GetSessions()
                    .OrderByDescending(s => s.GetPlaybackInfo()?.PlaybackStatus ==
                        GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing)
                    .FirstOrDefault();

                if(session == null) { _tracker.ResyncEmpty(); PlaybackChanged?.Invoke(false); return; }

                session.MediaPropertiesChanged   += async (s, _) => await OnMediaPropertiesAsync(s);
                session.PlaybackInfoChanged      += (s, _)   => OnPlaybackInfo(s);
                session.TimelinePropertiesChanged += (s, _)  => OnTimeline(s);
                OnPlaybackInfo(session);
                /* Session's track may have been playing before we attached;
                 * MediaPropertiesChanged won't fire until it changes, so fetch
                 * the initial metadata now. */
                await OnMediaPropertiesAsync(session);
            }

        private async Task OnMediaPropertiesAsync(GlobalSystemMediaTransportControlsSession s)
        {
                var media = await s.TryGetMediaPropertiesAsync();
                if(media == null) return;

                byte[]? cover = null;
                try {
                        if(media.Thumbnail != null) {
                                using var stream = await media.Thumbnail.OpenReadAsync();
                                using var ms = new MemoryStream();
                                using var netStream = stream.AsStreamForRead(); /* WinRT stream -> .NET stream */
                                await netStream.CopyToAsync(ms);
                                cover = ms.ToArray();
                        }
                }
                catch(Exception) { /* some players expose a dead thumbnail handle */ }

                var song = new SongInfo(media.Title, media.Artist, media.AlbumTitle, cover);
                SongChanged?.Invoke(song);
        }

        private void OnPlaybackInfo(GlobalSystemMediaTransportControlsSession s)
        {
                _tracker.Resync(s.GetTimelineProperties(), s.GetPlaybackInfo());
                PlaybackChanged?.Invoke(_tracker.IsPlaying);
        }

        private void OnTimeline(GlobalSystemMediaTransportControlsSession s) => OnPlaybackInfo(s);
}
