import { useState } from "react";
import { merch } from "../config/site";
import { merchProducts } from "../content/merch";
import { EmptyState, ExternalLink, PageHeader, Pill } from "../components/ui";
import { formatPrice } from "../lib/format";
import { usePageMeta } from "../lib/meta";
import { safeAssetUrl, safeExternalUrl } from "../lib/urls";
import type { MerchCategory, MerchProduct } from "../types";

const categories: { key: MerchCategory; label: string }[] = [
  { key: "tshirt", label: "T-Shirts" },
  { key: "hoodie", label: "Hoodies" },
  { key: "sticker", label: "Stickers" },
  { key: "accessory", label: "Accessories" },
];

const availability = {
  available: { label: "Available", tone: "green" },
  "sold-out": { label: "Sold out", tone: "muted" },
  "coming-soon": { label: "Coming soon", tone: "violet" },
} as const;

function ProductCard({ product }: { product: MerchProduct }) {
  const img = safeAssetUrl(product.images[0]?.src);
  const shop = safeExternalUrl(product.shopUrl);
  const a = availability[product.availability];
  return (
    <article className="merch-card">
      <div className="merch-card__img">
        {img ? <img src={img} alt={product.images[0].alt} loading="lazy" decoding="async" /> : <span aria-hidden="true" />}
      </div>
      <div className="merch-card__body">
        <h3>{product.name}</h3>
        <p>{product.description}</p>
        <div className="merch-card__meta">
          <Pill tone={a.tone}>{a.label}</Pill>
          {product.price !== undefined && <span className="price">{formatPrice(product.price, product.currency)}</span>}
        </div>
        {product.sizes?.length ? <p className="muted small">Sizes: {product.sizes.join(", ")}</p> : null}
        {shop && product.availability === "available" && (
          <ExternalLink href={shop} className="btn btn--primary btn--sm">
            View in shop
          </ExternalLink>
        )}
      </div>
    </article>
  );
}

export function Merch() {
  usePageMeta({ title: "Merch", description: "BrainBugz merchandise: t-shirts, hoodies, stickers and accessories." });
  const [cat, setCat] = useState<MerchCategory | null>(null);
  const list = merchProducts.filter((p) => !cat || p.category === cat);
  const shop = safeExternalUrl(merch.shopUrl);

  return (
    <div className="container page">
      <PageHeader code="// MERCH" title="Merch" intro="T-shirts, hoodies, stickers and accessories." />
      {merchProducts.length === 0 ? (
        <>
          <EmptyState title="Merch is coming soon">
            <p>We're working on our first designs. No products are available yet.</p>
            {shop && (
              <ExternalLink href={shop} className="btn btn--ghost btn--sm">
                Visit our shop
              </ExternalLink>
            )}
          </EmptyState>
          <ul className="merch-teaser" aria-label="Planned product types">
            {categories.map((c) => (
              <li key={c.key}>
                <span className="kicker">Coming soon</span>
                <strong>{c.label}</strong>
              </li>
            ))}
          </ul>
        </>
      ) : (
        <>
          <div className="chips" role="group" aria-label="Filter by product type">
            <button type="button" className="chip" aria-pressed={!cat} onClick={() => setCat(null)}>
              All
            </button>
            {categories.map((c) => (
              <button key={c.key} type="button" className="chip" aria-pressed={cat === c.key} onClick={() => setCat(c.key)}>
                {c.label}
              </button>
            ))}
          </div>
          {list.length ? (
            <div className="merch-grid">
              {list.map((p) => (
                <ProductCard key={p.id} product={p} />
              ))}
            </div>
          ) : (
            <EmptyState title="Nothing in this category yet" compact />
          )}
          {shop && (
            <p>
              <ExternalLink href={shop} className="text-link">
                Full shop →
              </ExternalLink>
            </p>
          )}
        </>
      )}
    </div>
  );
}
