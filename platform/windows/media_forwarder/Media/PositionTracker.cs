using Windows.Media.Control;

namespace media_forwarder.Media;

/// <summary>
/// Converts SMTC's event-driven position snapshots into a continuously
/// readable playback position.
///
/// SMTC only pushes TimelineProperties on play/pause/seek/track-switch,
/// so the reported Position must be extrapolated locally:
///     pos(t) = anchor + (t - anchorTime)  while playing
/// Same idea as BetterLyrics' position-offset handling in GSMTCService.
/// </summary>
public sealed class PositionTracker
{
        private TimeSpan _anchor;                                   /* position at anchor time */
        private DateTime _anchorTime = DateTime.MinValue;           /* wall clock of the snapshot */
        private int      _totalMs;                                  /* EndTime of the timeline snapshot */
        private bool     _playing;

        /// <summary>Call on every TimelinePropertiesChanged / PlaybackInfoChanged / track switch.</summary>
        public void Resync(GlobalSystemMediaTransportControlsSessionTimelineProperties tl,
                           GlobalSystemMediaTransportControlsSessionPlaybackInfo pb)
        {
                _anchor     = tl.Position;
                _anchorTime = tl.LastUpdatedTime.LocalDateTime;
                _totalMs = (int)(tl.EndTime - tl.StartTime).TotalMilliseconds;
                _playing = pb.PlaybackStatus ==
                           GlobalSystemMediaTransportControlsSessionPlaybackStatus.Playing;
        }

        /// <summary>No active session: stop the clock and zero out.</summary>
        public void ResyncEmpty()
        {
                _anchor = TimeSpan.Zero;
                _anchorTime = DateTime.Now;
                _totalMs = 0;
                _playing = false;
        }

        public bool IsPlaying => _playing;
        public int  TotalMs   => _totalMs;

        public int PositionMs
        {
                get {
                        int pos = (int)_anchor.TotalMilliseconds;
                        if(_playing) /* clock only runs while playing */
                                pos += (int)(DateTime.Now - _anchorTime).TotalMilliseconds;

                        return Math.Clamp(pos, 0, _totalMs is > 0 ? _totalMs : int.MaxValue);
                }
        }
}
