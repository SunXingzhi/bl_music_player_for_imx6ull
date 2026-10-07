namespace media_forwarder.Media;

/// <summary>
/// LrcParser converts an LRC string (from LRCLIB or a file) into an
/// event timeline used to highlight the active line by playback position.
///
/// Only line-level timing ([mm:ss.xx]) is needed for this demo; word-level
/// karaoke formats (TTML/QRC) would need a richer model.
/// </summary>
public sealed record LrcLine(int TimeMs, string Text);

public static class LrcParser
{
        private static readonly System.Text.RegularExpressions.Regex TagRegex =
            new(@"\[(\d+):(\d+)(?:\.(\d+))?\]", System.Text.RegularExpressions.RegexOptions.Compiled);

        public static IReadOnlyList<LrcLine> Parse(string? lrc)
        {
                if(string.IsNullOrWhiteSpace(lrc))
                        return Array.Empty<LrcLine>();

                List<LrcLine> lines = new();

                foreach(string raw in lrc.Split('\n')) {
                        string line = raw.TrimEnd('\r');
                        if(line.Length == 0) continue;

                        var matches = TagRegex.Matches(line);
                        if(matches.Count == 0) continue;

                        /* LRC timestamps can repeat (a line appearing at
                         * multiple times); keep text after the last tag. */
                        string text = line[(matches[^1].Index + matches[^1].Length)..].Trim();

                        foreach(System.Text.RegularExpressions.Match m in matches) {
                                int min   = int.Parse(m.Groups[1].Value);
                                int sec   = int.Parse(m.Groups[2].Value);
                                string frac = m.Groups[3].Value;
                                /* ".45" -> 450ms, ".4" -> 400ms (LRC is humble) */
                                int ms = frac.Length switch
                                {
                                        0 => 0,
                                        1 => int.Parse(frac) * 100,
                                        2 => int.Parse(frac) * 10,
                                        _ => int.Parse(frac[..3]),
                                };
                                lines.Add(new LrcLine(min * 60_000 + sec * 1000 + ms, text));
                        }
                }

                return lines.OrderBy(l => l.TimeMs).ToList();
        }

        /// <summary>Returns index of the line valid at <paramref name="posMs"/>, or -1.</summary>
        public static int FindCurrentLine(IReadOnlyList<LrcLine> lines, int posMs)
        {
                if(lines.Count == 0) return -1;

                int lo = 0, hi = lines.Count - 1, found = -1;
                while(lo <= hi) {
                        int mid   = (lo + hi) / 2;
                        if(lines[mid].TimeMs <= posMs) { found = mid; lo = mid + 1; }
                        else hi = mid - 1;
                }
                return found;
        }
}
