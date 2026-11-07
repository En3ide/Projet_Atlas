<template>
  <div class="grid-pannels">

    <!-- Température du CPU -->
    <div class="pannel">
      <h2>CPU Temperature</h2>

      <Line
        :data="cpuTempData"
        :options="chartOptions"
      />
    </div>

    <!-- Utilisation de la RAM -->
    <div class="pannel">
      <h2>RAM Usage</h2>

      <Doughnut
        :data="ramData"
        :options="doughnutOptions"
      />
    </div>

    <!-- Trafic réseau -->
    <div class="pannel">
      <h2>Network Traffic</h2>

      <Line
        :data="networkData"
        :options="chartOptions"
      />
    </div>

  </div>
</template>

<script setup lang="ts">
import {
  Chart as ChartJS,
  Title,
  Tooltip,
  Legend,
  LineElement,
  ArcElement,
  CategoryScale,
  LinearScale,
  PointElement,
  Filler
} from 'chart.js'

import {
  Line,
  Doughnut
} from 'vue-chartjs'

ChartJS.register(
  Title,
  Tooltip,
  Legend,
  LineElement,
  ArcElement,
  CategoryScale,
  LinearScale,
  PointElement,
  Filler
)

// Valeurs d'exemple pour la température du CPU

const cpuTempData = {
  labels: [
    '12:00',
    '12:05',
    '12:10',
    '12:15',
    '12:20',
    '12:25'
  ],

  datasets: [
    {
      label: 'CPU °C',

      data: [42, 45, 48, 46, 51, 49],

      borderColor: 'cyan',

      backgroundColor: 'rgba(0,255,255,0.12)',

      fill: true,

      tension: 0.4,

      pointRadius: 3
    }
  ]
}

// Valeurs d'exemple pour la RAM

const ramData = {
  labels: [
    'Used',
    'Free'
  ],

  datasets: [
    {
      data: [12, 20],

      backgroundColor: [
        'cyan',
        'rgba(255,255,255,0.08)'
      ],

      borderWidth: 0
    }
  ]
}

// Valeurs d'exemple pour le trafic réseau

const networkData = {
  labels: [
    '12:00',
    '12:05',
    '12:10',
    '12:15',
    '12:20',
    '12:25'
  ],

  datasets: [
    {
      label: 'Download Mbps',

      data: [25, 40, 35, 70, 60, 90],

      borderColor: 'lime',

      backgroundColor: 'rgba(0,255,0,0.12)',

      fill: true,

      tension: 0.4
    },

    {
      label: 'Upload Mbps',

      data: [10, 15, 12, 20, 18, 25],

      borderColor: 'orange',

      backgroundColor: 'rgba(255,165,0,0.12)',

      fill: true,

      tension: 0.4
    }
  ]
}

// Réglages des graphiques

const chartOptions = {
  responsive: true,

  maintainAspectRatio: false,

  plugins: {
    legend: {
      labels: {
        color: 'white'
      }
    }
  },

  scales: {
    x: {
      ticks: {
        color: 'white'
      },

      grid: {
        color: 'rgba(255,255,255,0.05)'
      }
    },

    y: {
      ticks: {
        color: 'white'
      },

      grid: {
        color: 'rgba(255,255,255,0.05)'
      }
    }
  }
}

const doughnutOptions = {
  responsive: true,

  plugins: {
    legend: {
      labels: {
        color: 'white'
      }
    }
  }
}
</script>

<style scoped>
.pannel {
  height: 400px;
}

canvas {
  width: 100% !important;
  height: 300px !important;
}
</style>
