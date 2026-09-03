import { playwrightLauncher } from '@web/test-runner-playwright';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

const aliases = {
  '#store/': '/src/store/',
  '#utils/': '/src/utils/',
  '#atoms/': '/src/components/atoms/',
  '#molecules/': '/src/components/molecules/',
  '#organisms/': '/src/components/organisms/',
  '#templates/': '/src/components/templates/',
  '#pages/': '/src/pages/',
};

/** Resolve #prefix/ imports for @web/test-runner. */
function importMapPlugin() {
  return {
    name: 'import-map-aliases',
    resolveImport({ source }) {
      for (const [prefix, target] of Object.entries(aliases)) {
        if (source.startsWith(prefix)) return source.replace(prefix, target);
      }
    },
  };
}

export default {
  files: 'src/components/**/*.test.js',
  nodeResolve: true,
  rootDir: ROOT,
  browsers: [playwrightLauncher({ product: 'chromium' })],
  plugins: [importMapPlugin()],
};
