<template>
  <div class="contenu">

    <div class="pannel lan-pannel">

      <!-- Titre et bouton pour lancer le scan -->
      <div class="lan-header">

        <div>
          <h1>LAN Monitor</h1>

          <p>
            Appareils détectés sur le réseau local
          </p>
        </div>

        <div class="lan-actions">
          <span v-if="hasScanned" class="lan-status">
            {{ devices.length }} appareil{{ devices.length === 1 ? '' : 's' }}
          </span>

          <button
            class="scan-button"
            type="button"
            :disabled="loading"
            @click="loadDevices"
          >
            {{ loading ? 'Scan en cours…' : 'Scanner le réseau' }}
          </button>
        </div>

      </div>

      <p v-if="errorMessage" class="scan-message scan-error" role="alert">
        {{ errorMessage }}
      </p>

      <p v-else-if="!hasScanned && !loading" class="scan-message">
        Lance un scan pour afficher les appareils détectés sur ton réseau.
      </p>

      <p v-if="hasScanned && scannedAt" class="scan-time">
        Dernier scan : {{ new Date(scannedAt).toLocaleString('fr-FR') }}
        ({{ scannedNetwork }})
      </p>

      <!-- Appareils trouvés sur le réseau -->
      <div class="table-wrapper">

        <table class="lan-table">

          <thead>
            <tr>

              <th @click="sortBy('hostname')">
                Hostname
              </th>

              <th @click="sortBy('ipv4')">
                IPv4
              </th>

              <th @click="sortBy('mac')">
                MAC
              </th>

              <th @click="sortBy('vendor')">
                Vendor
              </th>

              <th @click="sortBy('type')">
                Type
              </th>

              <th @click="sortBy('status')">
                Status
              </th>

            </tr>
          </thead>

          <tbody>

            <tr v-if="loading">
              <td colspan="6" class="table-message">Scan du réseau en cours…</td>
            </tr>

            <tr v-else-if="hasScanned && devices.length === 0 && !errorMessage">
              <td colspan="6" class="table-message">Aucun appareil détecté.</td>
            </tr>

            <template v-else>
              <tr
                v-for="device in sortedDevices"
                :key="device.id"
              >

              <td>
                {{ device.hostname }}
              </td>

              <td>
                {{ device.ipv4 }}
              </td>

              <td>
                {{ device.mac }}
              </td>

              <td>
                {{ device.vendor }}
              </td>

              <td>
                {{ device.type }}
              </td>

              <td>
                <span
                  class="status"
                  :class="device.status === 'ONLINE'
                    ? 'online'
                    : 'offline'"
                >
                  {{ device.status }}
                </span>
              </td>

              </tr>
            </template>

          </tbody>

        </table>

      </div>

    </div>

  </div>
</template>

<script lang="ts">
type Device = {
  id: string
  hostname: string
  ipv4: string
  mac: string
  vendor: string
  type: string
  status: string
}

type DeviceScanResponse = {
  network: string
  scannedAt: string
  devices: Device[]
}

type SortKey = keyof Device
type SortDirection = 'asc' | 'desc'

export default {
  name: 'LAN',

  setup() {
    const config = useRuntimeConfig()

    return {
      apiBase: config.public.apiBase,
    }
  },

  data() {
    return {
      devices: [] as Device[],
      loading: false,
      hasScanned: false,
      errorMessage: '',
      scannedAt: '',
      scannedNetwork: '',

      currentSort: 'hostname' as SortKey,

      sortDirection: 'asc' as SortDirection,
    }
  },

  computed: {
    sortedDevices(): Device[] {
      const devices = [...this.devices]

      devices.sort((a: Device, b: Device) => {
        const valueA = a[this.currentSort].toLowerCase()
        const valueB = b[this.currentSort].toLowerCase()

        if (valueA < valueB) {
          return this.sortDirection === 'asc' ? -1 : 1
        }

        if (valueA > valueB) {
          return this.sortDirection === 'asc' ? 1 : -1
        }

        return 0
      })

      return devices
    },
  },

  methods: {
    async loadDevices() {
      this.loading = true
      this.errorMessage = ''

      try {
        const response = await fetch(`${this.apiBase}/api/devices`)

        if (!response.ok) {
          const error = await response.json().catch(() => null)
          const message = typeof error?.message === 'string'
            ? error.message
            : `Erreur API (${response.status})`
          throw new Error(message)
        }

        const data = await response.json() as DeviceScanResponse

        if (!Array.isArray(data.devices)) {
          throw new Error('La réponse du serveur est invalide.')
        }

        this.devices = data.devices
        this.scannedAt = data.scannedAt
        this.scannedNetwork = data.network
        this.hasScanned = true
      } catch (error) {
        console.error(
          'Impossible de récupérer les appareils réseau :',
          error
        )

        this.devices = []
        this.hasScanned = false
        this.scannedAt = ''
        this.scannedNetwork = ''
        this.errorMessage = error instanceof Error
          ? error.message
          : 'Une erreur inattendue est survenue pendant le scan.'
      } finally {
        this.loading = false
      }
    },

    sortBy(field: SortKey) {
      if (this.currentSort === field) {
        this.sortDirection = this.sortDirection === 'asc' ? 'desc' : 'asc'
      } else {
        this.currentSort = field
        this.sortDirection = 'asc'
      }
    },
  },

}
</script>
<style scoped>

.lan-actions {
    display: flex;
    align-items: center;
    gap: 12px;
}

.scan-button {
    padding: 10px 16px;
    border: 1px solid rgba(0,255,120,0.25);
    border-radius: 12px;
    color: rgb(0,255,120);
    background: rgba(0,255,120,0.1);
    font: inherit;
    cursor: pointer;
}

.scan-button:disabled {
    opacity: 0.6;
    cursor: wait;
}

.scan-message,
.scan-time {
    color: rgba(255,255,255,0.65);
}

.scan-error {
    color: rgb(255,120,120);
}

.table-message {
    text-align: center !important;
    color: rgba(255,255,255,0.65) !important;
}

.lan-pannel {
    width: 100%;

    min-height: 85vh;
}

/* Haut du panneau */

.lan-header {
    display: flex;

    justify-content: space-between;
    align-items: center;

    margin-bottom: 24px;
}

.lan-header h1 {
    margin: 0;

    font-size: 28px;
}

.lan-header p {
    margin: 8px 0 0 0;

    color: rgba(255,255,255,0.55);
}

/* Nombre d'appareils détectés */

.lan-status {
    padding: 10px 18px;

    border-radius: 12px;

    color: rgb(0,255,120);

    background-color: rgba(0,255,120,0.12);

    border: 1px solid rgba(0,255,120,0.2);

    font-size: 13px;
    font-weight: bold;
}

/* Tableau des appareils */

.table-wrapper {
    width: 100%;

    overflow-x: auto;
}

.lan-table {
    width: 100%;

    border-collapse: collapse;

    min-width: 900px;
}

.lan-table th,
.lan-table td {
    padding: 16px;

    text-align: left;

    border-bottom:
      1px solid rgba(255,255,255,0.06);
}

.lan-table th {
    color: rgba(255,255,255,0.45);

    font-size: 12px;

    text-transform: uppercase;

    letter-spacing: 1px;

    cursor: pointer;

    transition: color 0.2s ease;
}

.lan-table th:hover {
    color: white;
}

.lan-table td {
    color: rgba(255,255,255,0.82);

    font-size: 14px;
}

.lan-table tbody tr:hover {
    background-color: rgba(255,255,255,0.03);
}

/* État de chaque appareil */

.status {
    padding: 6px 10px;

    border-radius: 10px;

    font-size: 12px;
    font-weight: bold;
}

.online {
    color: rgb(0,255,120);

    background-color:
      rgba(0,255,120,0.12);
}

.offline {
    color: rgb(255,95,86);

    background-color:
      rgba(255,95,86,0.12);
}

/* Affichage sur les petits écrans */

@media screen and (max-width: 768px) {

    .lan-header {
        flex-direction: column;

        align-items: flex-start;

        gap: 16px;
    }

    .lan-actions {
        flex-wrap: wrap;
    }
}

</style>
