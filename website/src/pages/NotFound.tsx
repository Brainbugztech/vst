import { PageHeader } from "../components/ui";
import { usePageMeta } from "../lib/meta";
import { Link } from "../lib/router";

export function NotFound() {
  usePageMeta({ title: "Page not found", noindex: true });
  return (
    <div className="container page">
      <PageHeader code="// ERROR 404" title="Lost in the lab" intro="This page doesn't exist (or isn't public yet)." />
      <div className="btn-row">
        <Link to="/" className="btn btn--primary">
          Back to home
        </Link>
        <Link to="/plugins" className="btn btn--ghost">
          Plugins
        </Link>
      </div>
    </div>
  );
}
