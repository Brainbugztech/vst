import { gigs } from "../content/gigs";
import { GigCard } from "../components/Cards";
import { EmptyState, PageHeader } from "../components/ui";
import { todayISO } from "../lib/format";
import { splitGigs } from "../lib/gigs";
import { usePageMeta } from "../lib/meta";
import { Link } from "../lib/router";

export function Gigs() {
  usePageMeta({ title: "Gigs", description: "Upcoming BrainBugz gigs and past events." });
  const { upcoming, past } = splitGigs(gigs, todayISO());
  return (
    <div className="container page">
      <PageHeader code="// GIGS" title="Gigs" intro="Upcoming shows and the event archive." />
      <section aria-labelledby="upcoming-h" className="stack">
        <h2 id="upcoming-h">Upcoming</h2>
        {upcoming.length ? (
          <div className="gig-list">
            {upcoming.map((g) => (
              <GigCard key={g.id} gig={g} />
            ))}
          </div>
        ) : (
          <EmptyState title="No upcoming gigs announced">
            New dates will be posted here. Promoters: <Link to="/booking">send a booking request</Link>.
          </EmptyState>
        )}
      </section>
      <section aria-labelledby="past-h" className="stack">
        <h2 id="past-h">Past events</h2>
        {past.length ? (
          <div className="gig-list">
            {past.map((g) => (
              <GigCard key={g.id} gig={g} past />
            ))}
          </div>
        ) : (
          <EmptyState title="The archive is still empty" compact />
        )}
      </section>
    </div>
  );
}
