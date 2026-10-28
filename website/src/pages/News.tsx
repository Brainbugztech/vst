import { news } from "../content/news";
import { NewsCard, newsCategoryLabel } from "../components/Cards";
import { EmptyState, ExternalLink, PageHeader } from "../components/ui";
import { formatDate } from "../lib/format";
import { usePageMeta } from "../lib/meta";
import { Link } from "../lib/router";
import { safeAssetUrl } from "../lib/urls";
import { NotFound } from "./NotFound";

export function News() {
  usePageMeta({ title: "News", description: "BrainBugz news: releases, plugin updates, events and behind the scenes." });
  const list = [...news].sort((a, b) => b.date.localeCompare(a.date));
  return (
    <div className="container page">
      <PageHeader code="// NEWS" title="News" intro="Releases, plugin updates, events and behind the scenes." />
      {list.length ? (
        <div className="news-grid">
          {list.map((n) => (
            <NewsCard key={n.slug} post={n} />
          ))}
        </div>
      ) : (
        <EmptyState title="No news yet">Updates from the lab will be posted here.</EmptyState>
      )}
    </div>
  );
}

export function NewsPostPage({ slug }: { slug: string }) {
  const post = news.find((n) => n.slug === slug);
  usePageMeta(post ? { title: post.title, description: post.excerpt, image: post.image?.src } : { title: "Not found", noindex: true });
  if (!post) return <NotFound />;
  const img = safeAssetUrl(post.image?.src);
  return (
    <article className="container page article">
      <nav aria-label="Breadcrumb" className="breadcrumb">
        <Link to="/news">News</Link> <span aria-hidden="true">/</span> <span aria-current="page">{post.title}</span>
      </nav>
      <p className="kicker">
        {newsCategoryLabel[post.category]} · <time dateTime={post.date}>{formatDate(post.date)}</time>
      </p>
      <h1 id="page-title" tabIndex={-1}>
        {post.title}
      </h1>
      {img && <img className="article__img" src={img} alt={post.image?.alt ?? ""} />}
      {post.body.map((p) => (
        <p key={p}>{p}</p>
      ))}
      {post.links?.length ? (
        <ul className="link-list">
          {post.links.map((l) => (
            <li key={l.url}>
              <ExternalLink href={l.url} className="btn btn--ghost btn--sm">
                {l.label}
              </ExternalLink>
            </li>
          ))}
        </ul>
      ) : null}
    </article>
  );
}
