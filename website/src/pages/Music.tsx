import { releases } from "../content/releases";
import { ReleaseCard } from "../components/Cards";
import { SocialLinks, configuredSocials } from "../components/Social";
import { EmptyState, PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";

export function Music() {
  usePageMeta({ title: "Music", description: "Releases by BrainBugz: psychedelic HiTech tracks, players and streaming links." });
  const list = [...releases].sort((a, b) => b.releaseDate.localeCompare(a.releaseDate));
  return (
    <div className="container page">
      <PageHeader code="// MUSIC" title="Music" intro="Releases, players and streaming links." />
      {list.length ? (
        <div className="release-list">
          {list.map((r) => (
            <ReleaseCard key={r.slug} release={r} />
          ))}
        </div>
      ) : (
        <EmptyState title="No releases listed yet">
          <p>Our first releases will show up here with players and streaming links.</p>
          {configuredSocials().length > 0 && (
            <>
              <p>Follow us in the meantime:</p>
              <SocialLinks />
            </>
          )}
        </EmptyState>
      )}
    </div>
  );
}
