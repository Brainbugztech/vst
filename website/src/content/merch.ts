import type { MerchProduct } from "../types";

// Merch-Produkte. Ohne shopUrl gibt es keinen Kauf-Button.
// Beispiel:
//
// {
//   id: "logo-tee-black",
//   name: "Logo Tee Black",
//   category: "tshirt",
//   description: "Schwarzes Shirt mit BrainBugz-Logo.",
//   images: [{ src: "/media/merch/logo-tee.webp", alt: "Schwarzes T-Shirt mit grünem Logo" }],
//   sizes: ["S", "M", "L", "XL"],
//   price: 25,
//   currency: "EUR",
//   availability: "available",
//   shopUrl: "https://...",
// },

export const merchProducts: MerchProduct[] = [];
