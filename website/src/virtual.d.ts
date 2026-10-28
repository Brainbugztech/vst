// Wird von vite.config.ts beim Build erzeugt (nur veröffentlichte Plugins).
declare module "virtual:brainbugz-plugins" {
  const plugins: unknown[];
  export default plugins;
}
