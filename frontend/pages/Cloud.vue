<template>
  <div class="contenu">
    <div class="pannel cloud-pannel">

      <div class="cloud-header">
        <div class="cloud-header-left">
          <h1>Atlas Cloud</h1>
          <p>File Browser • App Directory • Local Storage</p>
        </div>

        <div class="cloud-status">
          ONLINE
        </div>
      </div>

      <div class="cloud-body">

        <div class="cloud-toolbar">
          <div class="toolbar-url">
            {{ currentPath }}
          </div>
        </div>

        <div class="file-browser">

          <!-- Colonnes du navigateur de fichiers -->
          <div class="file-row file-head">

            <span @click="sortBy('name')" class="sortable">
              Nom
            </span>

            <span @click="sortBy('type')" class="sortable">
              Type
            </span>

            <span @click="sortBy('size')" class="sortable">
              Taille
            </span>

            <span @click="sortBy('modified')" class="sortable">
              Modifié
            </span>

          </div>

          <!-- Liste des fichiers et dossiers -->
          <div
            v-for="file in sortedFiles"
            :key="file.name"
            class="file-row"
          >

            <span class="file-name">
              <span>{{ file.icon }}</span>
              {{ file.name }}
            </span>

            <span>{{ file.type }}</span>

            <span>{{ file.size }}</span>

            <span>{{ file.modified }}</span>

          </div>

        </div>
      </div>

    </div>
  </div>
</template>
<script lang="ts">
type FileItem = {
  name: string
  type: string
  size: string
  modified: string
  icon: string
}

type SortDirection = 'asc' | 'desc'
type SortKey = keyof FileItem

export default {
  name: 'Cloud',

  data() {
    return {
      currentPath: 'Répertoire de l’application',

      files: [] as FileItem[],

      currentSort: 'name' as SortKey,

      sortDirection: 'asc' as SortDirection,
    }
  },

  computed: {
    sortedFiles(): FileItem[] {
      const files = [...this.files]

      files.sort((a: FileItem, b: FileItem) => {
        let valueA: string | number = a[this.currentSort]
        let valueB: string | number = b[this.currentSort]

        if (this.currentSort === 'size') {
          valueA = this.parseSize(valueA)
          valueB = this.parseSize(valueB)
        }

        if (typeof valueA === 'string') {
          valueA = valueA.toLowerCase()
        }

        if (typeof valueB === 'string') {
          valueB = valueB.toLowerCase()
        }

        if (valueA < valueB) {
          return this.sortDirection === 'asc' ? -1 : 1
        }

        if (valueA > valueB) {
          return this.sortDirection === 'asc' ? 1 : -1
        }

        return 0
      })

      return files
    },
  },

  methods: {
    async loadFiles() {
      const response = await fetch('/api/files')
      this.files = await response.json()
    },

    sortBy(field: SortKey) {
      if (this.currentSort === field) {
        this.sortDirection = this.sortDirection === 'asc' ? 'desc' : 'asc'
      } else {
        this.currentSort = field
        this.sortDirection = 'asc'
      }
    },

    parseSize(size: string): number {
      if (size === '-') return 0

      const value = parseFloat(size)

      if (size.includes('GB')) {
        return value * 1024 * 1024
      }

      if (size.includes('MB')) {
        return value * 1024
      }

      if (size.includes('KB')) {
        return value
      }

      return value
    },
  },

  mounted() {
    this.loadFiles()
  },
}
</script>


<style scoped>
.cloud-pannel {
    width: 100%;
    min-height: 85vh;

    display: flex;
    flex-direction: column;

    padding: 0;

    overflow: hidden;
}

.cloud-header {
    height: 90px;

    background-color: rgba(255,255,255,0.02);

    border-bottom: 1px solid rgba(255,255,255,0.05);

    padding: 20px 30px;

    display: flex;
    align-items: center;
    justify-content: space-between;

    flex-shrink: 0;
}

.cloud-header-left {
    display: flex;
    flex-direction: column;
}

.cloud-header h1 {
    margin: 0;

    font-size: 28px;
    font-weight: 700;

    color: white;
}

.cloud-header p {
    margin: 8px 0 0 0;

    color: rgba(255,255,255,0.55);

    font-size: 14px;

    letter-spacing: 0.5px;
}

.cloud-status {
    background-color: rgba(0,255,120,0.12);

    color: rgb(0,255,120);

    padding: 10px 18px;

    border-radius: 12px;

    font-size: 13px;
    font-weight: bold;

    letter-spacing: 1px;

    border: 1px solid rgba(0,255,120,0.2);

    box-shadow: 0 0 15px rgba(0,255,120,0.15);
}

.cloud-body {
    flex: 1;

    display: flex;
    flex-direction: column;

    background-color: rgb(15,15,15);

    overflow: hidden;
}

.cloud-toolbar {
    height: 55px;

    background-color: rgb(18,18,18);

    border-bottom: 1px solid rgba(255,255,255,0.05);

    display: flex;
    align-items: center;

    padding: 0 20px;

    flex-shrink: 0;
}

.toolbar-url {
    background-color: rgb(25,25,25);

    color: rgba(255,255,255,0.65);

    height: 34px;

    padding: 0 16px;

    border-radius: 10px;

    display: flex;
    align-items: center;

    font-size: 13px;

    font-family: monospace;

    width: 100%;
    max-width: 500px;

    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
}

.file-browser {
    flex: 1;

    padding: 20px;

    overflow-y: auto;
}

.file-row {
    display: grid;

    grid-template-columns: 1fr 140px 120px 160px;

    gap: 16px;

    align-items: center;

    padding: 14px 18px;

    margin-bottom: 10px;

    background-color: rgba(255,255,255,0.03);

    border: 1px solid rgba(255,255,255,0.05);

    border-radius: 14px;

    color: rgba(255,255,255,0.75);

    font-size: 14px;
}

.file-row:hover {
    background-color: rgba(255,255,255,0.06);
}

.file-head {
    background-color: transparent;

    border: none;

    color: rgba(255,255,255,0.45);

    font-size: 12px;

    text-transform: uppercase;

    letter-spacing: 1px;
}

.file-name {
    color: white;

    display: flex;
    align-items: center;

    gap: 12px;

    font-weight: 500;
}

.sortable {
    cursor: pointer;

    transition: color 0.2s ease;
}

.sortable:hover {
    color: white;
}

@media screen and (max-width: 900px) {

    .file-row {
        grid-template-columns: 1fr;

        gap: 6px;
    }

    .file-head {
        display: none;
    }
}
</style>
