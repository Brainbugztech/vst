import { useEffect, useRef } from "react";
import { Footer } from "./components/Footer";
import { Header } from "./components/Header";
import { MiniPlayer } from "./components/MiniPlayer";
import { RadioProvider } from "./lib/radio";
import { matchRoute, usePathname } from "./lib/router";
import { About } from "./pages/About";
import { Booking } from "./pages/Booking";
import { Gigs } from "./pages/Gigs";
import { Home } from "./pages/Home";
import { Datenschutz, Impressum } from "./pages/Legal";
import { Merch } from "./pages/Merch";
import { Music } from "./pages/Music";
import { News, NewsPostPage } from "./pages/News";
import { NotFound } from "./pages/NotFound";
import { PluginDetail } from "./pages/PluginDetail";
import { Plugins } from "./pages/Plugins";
import { Radio } from "./pages/Radio";

const routes = [
  { name: "home", path: "/" },
  { name: "music", path: "/music" },
  { name: "radio", path: "/radio" },
  { name: "gigs", path: "/gigs" },
  { name: "plugins", path: "/plugins" },
  { name: "plugin", path: "/plugins/:slug" },
  { name: "merch", path: "/merch" },
  { name: "news", path: "/news" },
  { name: "post", path: "/news/:slug" },
  { name: "about", path: "/about" },
  { name: "booking", path: "/booking" },
  { name: "impressum", path: "/impressum" },
  { name: "datenschutz", path: "/datenschutz" },
];

function Page({ pathname }: { pathname: string }) {
  const m = matchRoute(routes, pathname);
  switch (m?.name) {
    case "home":
      return <Home />;
    case "music":
      return <Music />;
    case "radio":
      return <Radio />;
    case "gigs":
      return <Gigs />;
    case "plugins":
      return <Plugins />;
    case "plugin":
      return <PluginDetail key={m.params.slug} slug={m.params.slug} />;
    case "merch":
      return <Merch />;
    case "news":
      return <News />;
    case "post":
      return <NewsPostPage key={m.params.slug} slug={m.params.slug} />;
    case "about":
      return <About />;
    case "booking":
      return <Booking />;
    case "impressum":
      return <Impressum />;
    case "datenschutz":
      return <Datenschutz />;
    default:
      return <NotFound />;
  }
}

export function App() {
  const pathname = usePathname();
  const first = useRef(true);

  // Nach Seitenwechsel: Fokus auf die neue Überschrift (für Screenreader und Tastatur).
  useEffect(() => {
    if (first.current) {
      first.current = false;
      return;
    }
    const h = document.getElementById("page-title");
    (h ?? document.getElementById("main"))?.focus({ preventScroll: true });
  }, [pathname]);

  return (
    <RadioProvider>
      <a className="skip-link" href="#main">
        Skip to content
      </a>
      <Header />
      <main id="main" tabIndex={-1}>
        <Page pathname={pathname} />
      </main>
      <Footer />
      <MiniPlayer />
    </RadioProvider>
  );
}
