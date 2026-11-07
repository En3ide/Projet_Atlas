// Config générale du front
// Doc Nuxt : https://nuxt.com/docs/api/configuration/nuxt-config

export default defineNuxtConfig({
  compatibilityDate: '2025-07-15',

  // On garde les dossiers du front à la racine avec Nuxt 4
  srcDir: '.',
  dir: {
    app: 'app',
  },

  devServer: {
    port: 3000,
  },

  devtools: {
    enabled: process.env.NODE_ENV !== 'production',
  },

  ssr: true,

  modules: [
    '@pinia/nuxt',
    '@nuxtjs/tailwindcss',
    '@nuxt/image',
    'nuxt-security',
  ],

  typescript: {
    strict: true,
    typeCheck: true,
  },

  runtimeConfig: {
    apiSecret: process.env.NUXT_API_SECRET,

    public: {
      appName: 'Atlas',
      appDescription: 'Home Infrastructure Control Center',
      apiBase: process.env.NUXT_PUBLIC_API_BASE || 'http://localhost:3001',
      wsBase: process.env.NUXT_PUBLIC_WS_BASE || 'ws://localhost:3001',
    },
  },

  app: {
    head: {
      htmlAttrs: {
        lang: 'fr',
      },
      title: 'Atlas',
      titleTemplate: '%s · Atlas',
      meta: [
        { charset: 'utf-8' },
        {
          name: 'viewport',
          content: 'width=device-width, initial-scale=1',
        },
        {
          name: 'description',
          content:
            'Atlas est une plateforme de supervision et de gestion d’infrastructure domestique.',
        },
        {
          name: 'theme-color',
          content: '#0f172a',
        },
      ],
      link: [
        {
          rel: 'icon',
          type: 'image/x-icon',
          href: '/favicon.ico',
        },
        {
          rel: 'preconnect',
          href: 'https://fonts.googleapis.com',
        },
        {
          rel: 'preconnect',
          href: 'https://fonts.gstatic.com',
          crossorigin: '',
        },
        {
          rel: 'stylesheet',
          href: 'https://fonts.googleapis.com/icon?family=Material+Icons',
        },
      ],
    },
  },

  css: ['~/assets/css/main.css'],

  postcss: {
    plugins: {
      // Nuxt 4 a besoin du chemin complet pour charger ce plugin
      'tailwindcss/nesting': false,
      'tailwindcss/nesting/index.js': {},
    },
  },

  imports: {
    dirs: [
      'stores',
      'composables',
      'utils',
    ],
  },

  components: [
    {
      path: '~/components',
      pathPrefix: false,
    },
  ],

  routeRules: {
    '/': {
      prerender: true,
    },

    '/login': {
      ssr: false,
    },

    '/dashboard/**': {
      ssr: false,
      headers: {
        'Cache-Control': 'no-store',
      },
    },

    '/admin/**': {
      ssr: false,
      headers: {
        'Cache-Control': 'no-store',
      },
    },
  },

  security: {
    headers: {
      crossOriginEmbedderPolicy: false,
      contentSecurityPolicy: {
        'default-src': ["'self'"],
        'script-src': ["'self'", "'unsafe-inline'"],
        'style-src': [
          "'self'",
          "'unsafe-inline'",
          'https://fonts.googleapis.com',
        ],
        'font-src': [
          "'self'",
          'https://fonts.gstatic.com',
          'https://fonts.googleapis.com',
        ],
        'img-src': ["'self'", 'data:', 'blob:'],
        'connect-src': [
          "'self'",
          'http://localhost:3001',
          'ws://localhost:3001',
        ],
        'frame-ancestors': ["'none'"],
      },
    },
  },

  image: {
    format: ['avif', 'webp'],
    quality: 80,
    screens: {
      xs: 320,
      sm: 640,
      md: 768,
      lg: 1024,
      xl: 1280,
      xxl: 1536,
    },
  },

  vite: {
    server: {
      strictPort: true,
    },
    optimizeDeps: {
      include: ['vue', 'pinia'],
    },
  },

  nitro: {
    compressPublicAssets: true,
    experimental: {
      websocket: true,
    },
  },

  build: {
    transpile: [],
  },
})
