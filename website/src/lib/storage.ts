// Kleine Browser-Speicher-Helfer. Speicher kann fehlen (privater Modus, blockiert), daher immer try/catch.

export function readFlag(key: string): boolean {
  try {
    return window.localStorage.getItem(key) === "1";
  } catch {
    return false;
  }
}

export function writeFlag(key: string, value: boolean) {
  try {
    if (value) window.localStorage.setItem(key, "1");
    else window.localStorage.removeItem(key);
  } catch {
    /* Speicher nicht verfügbar: dann eben nur für diesen Besuch */
  }
}

export function readNumber(key: string, fallback: number): number {
  try {
    const v = window.localStorage.getItem(key);
    const n = v === null ? NaN : Number(v);
    return Number.isFinite(n) ? n : fallback;
  } catch {
    return fallback;
  }
}

export function writeNumber(key: string, value: number) {
  try {
    window.localStorage.setItem(key, String(value));
  } catch {
    /* egal */
  }
}
